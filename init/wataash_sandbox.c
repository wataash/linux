#ifdef CONFIG_X86_64
#include <asm/io.h> // inb
#endif /* CONFIG_X86_64 */

#include <linux/device.h> // device_create
#include <linux/device/class.h> // class_create
#include <linux/fs.h> // noop_llseek
#include <linux/init.h> // __init
#include <linux/input.h> // input_dev
#include <linux/irqreturn.h> // irqreturn_t
#include <linux/kernel.h> // ARRAY_SIZE
#include <linux/linkage.h> // SYM_FUNC_START
#include <linux/module.h> // module_init
#include <linux/percpu.h> // DECLARE_PER_CPU
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/serio.h> // struct serio
#include <linux/smp.h> // smp_processor_id()
#include <linux/sysctl.h>
#include <linux/typecheck.h> // typecheck
#include <linux/uaccess.h> // copy_from_user
#include <uapi/linux/major.h> // UNNAMED_MAJOR

static void asm_(void);
static int device(void);
static void locking(void);
static void macros(void);
static void percpu_(void);
static void rcu(void);
static void sysctl(void);
static void typecheck_(void);
static void workqueue(void);

static void sandbox(void)
{
	__asm__("nop");
}
void wataash_sandbox(void)
{
	int rc;

	pr_notice("wataash_sandbox: begin");

	sandbox();

	asm_();
	rc = device();
	if (rc < 0) {
		pr_err("wataash_sandbox: device() failed: %d", rc);
	}
	locking();
	macros();
	percpu_();
	rcu();
	sysctl();
	typecheck_();
	workqueue();

	pr_notice("wataash_sandbox: end");
	__asm__("nop");
}

// -----------------------------------------------------------------------------
// asm

static void asm_(void) {
}

// linkage
// https://www.kernel.org/doc/html/latest/asm-annotations.html

// arch/x86/kernel/irqflags.S

// SYM_FUNC_START(native_save_fl)
//
// SYM_START(native_save_fl, SYM_L_GLOBAL, SYM_A_ALIGN)
//
// SYM_START(native_save_fl, .globl name, ALIGN)
//
// SYM_ENTRY(native_save_fl, .globl name, ALIGN)
//
// linkage(native_save_fl) ASM_NL align ASM_NL name:
//
// .globl native_save_fl; ALIGN; name:

// -----------------------------------------------------------------------------
// device
// ref: drivers/char/mem.c
// TODO: https://wiki.bit-hive.com/north/pg/sysfsとkobject
// TOOD: https://wiki.bit-hive.com/north/pg/sysfsとkset

// #define WDEV_MAJOR 333
enum {
	ATKBD_MINOR = 1,
};

struct serio *wataash_serio_atkbd;
irqreturn_t atkbd_interrupt_(struct serio *serio, unsigned char data, unsigned int flags);

static ssize_t wdev_atkbd_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	size_t i;
	unsigned long copied;
	char kbuf[1024];

	// TODO: remove logs
	pr_info("%s\n", __func__);
	pr_info("%s: count:%zu\n", __func__, count);
	pr_info("%s: ppos:%p\n", __func__, ppos);
	pr_info("%s: ppos:%lld\n", __func__, *ppos);
	pr_info("%s: file:%p\n", __func__, file);

	if (count > sizeof(kbuf))
		return -EMSGSIZE;

	copied = copy_from_user(kbuf, buf, count);
	if (copied)
		return -EFAULT;

	for (i = 0; i < count; i++) {
		pr_info("%s: kbuf[%zu]:%c\n", __func__, i, kbuf[i]);
		atkbd_interrupt_(wataash_serio_atkbd, kbuf[i], 0);
		pr_info("%s: kbuf[%zu]:%c done\n", __func__, i, kbuf[i]);
	}
	pr_info("%s: return\n", __func__);

	return count;
}

static const struct file_operations wdev_atkbd_fops = {
	.write = wdev_atkbd_write,
};

static int wdev_atkbd_open(struct inode *inode, struct file *filp)
{
	int minor;

	minor = iminor(inode);
	if (minor != ATKBD_MINOR)
		return -ENXIO;

	filp->f_op = &wdev_atkbd_fops;

	return 0;
}

static const struct file_operations wdev_fops = {
	.open = wdev_atkbd_open,
};

static struct class *wdev_class;

static int device(void)
{
	int major;
	struct device *dev_atkbd;

	(void)UNNAMED_MAJOR; // char_dev.c: ret = find_dynamic_major();
	major = register_chrdev(UNNAMED_MAJOR, "wdev", &wdev_fops);
	if (major < 0) {
		pr_err("wataash_sandbox: unable to get major %d for wdev devs (%d)\n",
		       UNNAMED_MAJOR, major);
		return major;
	}

	wdev_class = class_create(THIS_MODULE, "wdev");
	if (IS_ERR(wdev_class))
		return PTR_ERR(wdev_class);

	// wdev_class->devnode = wdev_devnode;

	dev_atkbd = device_create(wdev_class, NULL, MKDEV(major, ATKBD_MINOR),
				  NULL, "atkbd");
	if (IS_ERR(dev_atkbd)) {
		pr_err("wataash_sandbox: unable to create atkbd device: %ld\n",
		       PTR_ERR(dev_atkbd));
		return PTR_ERR(dev_atkbd);
	}

	return 0;
}

// -----------------------------------------------------------------------------
// device - input

// https://www.kernel.org/doc/html/latest/input/input-programming.html

#ifdef CONFIG_X86_64

#define BUTTON_PORT 9999
#define BUTTON_IRQ 1

static struct input_dev *button_dev;

static irqreturn_t button_interrupt(int irq, void *dummy)
{
	input_report_key(button_dev, BTN_0, inb(BUTTON_PORT) & 1);
	input_sync(button_dev);
	return IRQ_HANDLED;
}

__maybe_unused // これが無いとwarning; 他の __init はどうやっている？
static int __init button_init(void)
{
	int error;

	int *button_dummy_dev_ptr = (int *)7;
	if (request_irq(BUTTON_IRQ, button_interrupt, IRQF_SHARED & 0, "button", button_dummy_dev_ptr)) {
		pr_err("button.c: Can't allocate irq %d\n", BUTTON_IRQ);
		return -EBUSY;
	}

	button_dev = input_allocate_device();
	if (!button_dev) {
		pr_err("button.c: Not enough memory\n");
		error = -ENOMEM;
		goto err_free_irq;
	}

	button_dev->evbit[0] = BIT_MASK(EV_KEY);
	button_dev->keybit[BIT_WORD(BTN_0)] = BIT_MASK(BTN_0);
	// or:
	set_bit(EV_KEY, button_dev->evbit);
	set_bit(BTN_0, button_dev->keybit);

	error = input_register_device(button_dev);
	if (error) {
		pr_err("button.c: Failed to register device\n");
		goto err_free_dev;
	}

	return 0;

err_free_dev:
	input_free_device(button_dev);
err_free_irq:
	free_irq(BUTTON_IRQ, button_interrupt);
	return error;
}

static void __exit button_exit(void)
{
	input_unregister_device(button_dev);
	free_irq(BUTTON_IRQ, button_interrupt);
}

// module_init(button_init);
// module_exit(button_exit);

#endif /* CONFIG_X86_64 */

// -----------------------------------------------------------------------------
// locking

// TODO: https://www.kernel.org/doc/html/latest/kernel-hacking/locking.html#common-examples

static void locking(void) {
}

// -----------------------------------------------------------------------------
// macros

// TODO: https://www.kernel.org/doc/html/latest/kernel-hacking/locking.html#common-examples

static void macros(void) {
	int arr[] = {42, 43};
	(void)ARRAY_SIZE(arr);
}

// -----------------------------------------------------------------------------
// rcu
// https://kumagi.hatenadiary.org/entry/20130803/1375492517 userspace RCU(QSBR)の使い方と解説
// https://www.atmarkit.co.jp/flinux/rensai/watch2009/watch04a.html 4月版 RCUの全面書き直しも！ 2.6.29は何が変わった？
// NetBSD pserialize(9)
// TODO: https://www.kernel.org/doc/Documentation/RCU/whatisRCU.txt

struct rcu_a {
	struct rcu_head rcu;
	int val;
	char *p;
};

static void rcu_cb(struct rcu_head * head) // a->rcu
{
	struct rcu_a *ap = container_of(head, struct rcu_a, rcu); // a
	// a.rcu == ap->rcu == head
	ap->val++;
	// free(ap->p)
	// ap->p = malloc(sizeof(int);
	ap->p++;
}

static void rcu_tes(void)
{
	struct rcu_a a = { .val = 1 };
	int tmp_val;
	char *tmp_p;

	call_rcu(&a.rcu, rcu_cb);
	synchronize_rcu(); // nop; no reader

	// rcu_read_[un]lock: NOP since CONFIG_PREEMPT_RCU is not enabled
	rcu_read_lock();
	// synchronize_rcu(); // prohibited? why nop? (expect block -- dead lock)
	tmp_val = a.val;
	tmp_p = a.p;
	pr_notice("%p\n", tmp_p); // -> cpu2: rcu_cb() if synchronize_rcu() above
	rcu_read_unlock();

	pr_notice("%d\n", tmp_val);

	rcu_read_lock();
	rcu_read_unlock();

	schedule_timeout_interruptible(100); // -> rcu_cb()

	rcu_read_lock();
	tmp_val = a.val;
	tmp_p = a.p;
	pr_notice("%p\n", tmp_p);
	rcu_read_unlock();

	pr_notice("%d\n", tmp_val);

	__asm__("nop");
}

static void rcu(void)
{
	rcu_tes();
	__asm__("nop");
}

// -----------------------------------------------------------------------------
// percpu_

DECLARE_PER_CPU(int, wataash_cpu_int); // should be in wataash_sandbox.h
DEFINE_PER_CPU(int, wataash_cpu_int) = 42;
// DECLARE_PER_CPU_FIRST          DEFINE_PER_CPU_FIRST
// DECLARE_PER_CPU_SHARED_ALIGNED DEFINE_PER_CPU_SHARED_ALIGNED
// DECLARE_PER_CPU_ALIGNED        DEFINE_PER_CPU_ALIGNED

static void percpu_(void)
{
	int cpu, i;

	// CONFIG_PREEMPTION disabled -- no preemption

	cpu = smp_processor_id(); // -> __this_cpu_read()
	// preemption might occur here if CONFIG_PREEMPTION enabled?
	cpu = smp_processor_id(); // might be changed?

	// Can I raise preemption here? (when CONFIG_PREEMPTION enabled, and even when CONFIG_PREEMPTION disabled?)
	// @qc-linux-preempt

	// (gdb) whatis wataash_cpu_int
	// type = int
	// (gdb) p wataash_cpu_int
	// Cannot access memory at address 0x195c4
	// (gdb) p/x (unsigned long)&wataash_cpu_int
	// $3 = 0x195c4
	// (gdb) p (int *)($gs_base + (unsigned long)&wataash_cpu_int)
	// $5 = (int *) 0xffff88807a4195c4

	i = per_cpu(wataash_cpu_int, 0);

	i = get_cpu_var(wataash_cpu_int); // -> preempt_disable()
	// use i (preemption-safe here)
	put_cpu_var(wataash_cpu_int); // -> preempt_enable()

	get_cpu(); // preempt_disable()
	// preemption-safe here
	put_cpu(); // preempt_enable()

	__asm__("nop");
}

// -----------------------------------------------------------------------------
// sysctl

unsigned int wataash_atkbd_emacs = 1;

// ref: yama_sysctl_path
static struct ctl_path wataash_sysctl_path[] = {
	{ .procname = "wataash", },
	{ },
};

static struct ctl_table wataash_sysctl_table[] = {
	{
		.procname       = "atkbd_emacs",
		.data           = &wataash_atkbd_emacs,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = proc_douintvec_minmax,
		.extra1         = SYSCTL_ZERO,
		.extra2         = SYSCTL_ONE,
	},
	{ }
};

// ref: yama_init
static void sysctl(void)
{
	if (!register_sysctl_paths(wataash_sysctl_path, wataash_sysctl_table))
		pr_err("wataash: sysctl registration failed.\n");
}

// -----------------------------------------------------------------------------
// typecheck


static void typecheck_(void) {
	typecheck(int, 1);
	typecheck(long, 1L);
	typecheck(long long, 1LL);

	// gcc emits warmings:
#if 0
	typecheck(long long, 1L);
	typecheck(unsigned int, 1);
#endif
}

// -----------------------------------------------------------------------------
// workqueue

static void workqueue(void)
{
	// TODO
	// struct workqueue_struct *tes_wq = create_workqueue("tes");
	// INIT_WORK(tes_wq, NULL);
	// queue_work(tes_wq, NULL);
}
