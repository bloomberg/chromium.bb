
/****************************************************************************
 * Copyright (C) 2003-2006 by XGI Technology, Taiwan.			
 *																			*
 * All Rights Reserved.														*
 *																			*
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the	
 * "Software"), to deal in the Software without restriction, including	
 * without limitation on the rights to use, copy, modify, merge,	
 * publish, distribute, sublicense, and/or sell copies of the Software,	
 * and to permit persons to whom the Software is furnished to do so,	
 * subject to the following conditions:					
 *																			*
 * The above copyright notice and this permission notice (including the	
 * next paragraph) shall be included in all copies or substantial	
 * portions of the Software.						
 *																			*
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,	
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF	
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND		
 * NON-INFRINGEMENT.  IN NO EVENT SHALL XGI AND/OR			
 * ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,		
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,		
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER			
 * DEALINGS IN THE SOFTWARE.												
 ***************************************************************************/
#include "xgi_types.h"
#include "xgi_linux.h"
#include "xgi_drv.h"
#include "xgi_regs.h"
#include "xgi_pcie.h"
#include "xgi_misc.h"
#include "xgi_cmdlist.h"

/* for debug */
static int xgi_temp = 1;
/*
 * global parameters
 */
static struct xgi_dev {
	u16 vendor;
	u16 device;
	const char *name;
} xgidev_list[] = {
	{
	PCI_VENDOR_ID_XGI, PCI_DEVICE_ID_XP5, "XP5"}, {
	PCI_VENDOR_ID_XGI, PCI_DEVICE_ID_XG47, "XG47"}, {
	0, 0, NULL}
};

int xgi_major = XGI_DEV_MAJOR;	/* xgi reserved major device number. */

static int xgi_num_devices = 0;

xgi_info_t xgi_devices[XGI_MAX_DEVICES];

#if defined(XGI_PM_SUPPORT_APM)
static struct pm_dev *apm_xgi_dev[XGI_MAX_DEVICES] = { 0 };
#endif

/* add one for the control device */
xgi_info_t xgi_ctl_device;
wait_queue_head_t xgi_ctl_waitqueue;

#ifdef CONFIG_PROC_FS
struct proc_dir_entry *proc_xgi;
#endif

#ifdef CONFIG_DEVFS_FS
devfs_handle_t xgi_devfs_handles[XGI_MAX_DEVICES];
#endif

struct list_head xgi_mempid_list;

/* xgi_ functions.. do not take a state device parameter  */
static int xgi_post_vbios(xgi_ioctl_post_vbios_t * info);
static void xgi_proc_create(void);
static void xgi_proc_remove_all(struct proc_dir_entry *);
static void xgi_proc_remove(void);

/* xgi_kern_ functions, interfaces used by linux kernel */
int xgi_kern_probe(struct pci_dev *, const struct pci_device_id *);

unsigned int xgi_kern_poll(struct file *, poll_table *);
int xgi_kern_ioctl(struct inode *, struct file *, unsigned int, unsigned long);
int xgi_kern_mmap(struct file *, struct vm_area_struct *);
int xgi_kern_open(struct inode *, struct file *);
int xgi_kern_release(struct inode *inode, struct file *filp);

void xgi_kern_vma_open(struct vm_area_struct *vma);
void xgi_kern_vma_release(struct vm_area_struct *vma);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 1))
struct page *xgi_kern_vma_nopage(struct vm_area_struct *vma,
				 unsigned long address, int *type);
#else
struct page *xgi_kern_vma_nopage(struct vm_area_struct *vma,
				 unsigned long address, int write_access);
#endif

int xgi_kern_read_card_info(char *, char **, off_t off, int, int *, void *);
int xgi_kern_read_status(char *, char **, off_t off, int, int *, void *);
int xgi_kern_read_pcie_info(char *, char **, off_t off, int, int *, void *);
int xgi_kern_read_version(char *, char **, off_t off, int, int *, void *);

int xgi_kern_ctl_open(struct inode *, struct file *);
int xgi_kern_ctl_close(struct inode *, struct file *);
unsigned int xgi_kern_ctl_poll(struct file *, poll_table *);

void xgi_kern_isr_bh(unsigned long);
irqreturn_t xgi_kern_isr(int, void *, struct pt_regs *);

static void xgi_lock_init(xgi_info_t * info);

#if defined(XGI_PM_SUPPORT_ACPI)
int xgi_kern_acpi_standby(struct pci_dev *, u32);
int xgi_kern_acpi_resume(struct pci_dev *);
#endif

/*
 * verify access to pci config space wasn't disabled behind our back
 * unfortunately, XFree86 enables/disables memory access in pci config space at
 * various times (such as restoring initial pci config space settings during vt
 * switches or when doing mulicard). As a result, all of our register accesses
 * are garbage at this point. add a check to see if access was disabled and
 * reenable any such access.
 */
#define XGI_CHECK_PCI_CONFIG(xgi) \
    xgi_check_pci_config(xgi, __LINE__)

static inline void xgi_check_pci_config(xgi_info_t * info, int line)
{
	unsigned short cmd, flag = 0;

	// don't do this on the control device, only the actual devices
	if (info->flags & XGI_FLAG_CONTROL)
		return;

	pci_read_config_word(info->dev, PCI_COMMAND, &cmd);
	if (!(cmd & PCI_COMMAND_MASTER)) {
		XGI_INFO("restoring bus mastering! (%d)\n", line);
		cmd |= PCI_COMMAND_MASTER;
		flag = 1;
	}

	if (!(cmd & PCI_COMMAND_MEMORY)) {
		XGI_INFO("restoring MEM access! (%d)\n", line);
		cmd |= PCI_COMMAND_MEMORY;
		flag = 1;
	}

	if (flag)
		pci_write_config_word(info->dev, PCI_COMMAND, cmd);
}

/*
 * struct pci_device_id {
 *  unsigned int vendor, device;        // Vendor and device ID or PCI_ANY_ID
 *  unsigned int subvendor, subdevice;  // Subsystem ID's or PCI_ANY_ID
 *  unsigned int class, class_mask;     // (class,subclass,prog-if) triplet
 *  unsigned long driver_data;          // Data private to the driver
 * };
 */

static struct pci_device_id xgi_dev_table[] = {
	{
	 .vendor = PCI_VENDOR_ID_XGI,
	 .device = PCI_ANY_ID,
	 .subvendor = PCI_ANY_ID,
	 .subdevice = PCI_ANY_ID,
	 .class = (PCI_CLASS_DISPLAY_VGA << 8),
	 .class_mask = ~0,
	 },
	{}
};

/*
 *  #define MODULE_DEVICE_TABLE(type,name) \
 *      MODULE_GENERIC_TABLE(type##_device,name)
 */
MODULE_DEVICE_TABLE(pci, xgi_dev_table);

/*
 * struct pci_driver {
 *  struct list_head node;
 *  char *name;
 *  const struct pci_device_id *id_table;   // NULL if wants all devices
 *  int  (*probe)(struct pci_dev *dev, const struct pci_device_id *id); // New device inserted
 *  void (*remove)(struct pci_dev *dev);    // Device removed (NULL if not a hot-plug capable driver)
 *  int  (*save_state)(struct pci_dev *dev, u32 state);     // Save Device Context
 *  int  (*suspend)(struct pci_dev *dev, u32 state);        // Device suspended
 *  int  (*resume)(struct pci_dev *dev);                    // Device woken up
 *  int  (*enable_wake)(struct pci_dev *dev, u32 state, int enable);   // Enable wake event
 * };
 */
static struct pci_driver xgi_pci_driver = {
	.name = "xgi",
	.id_table = xgi_dev_table,
	.probe = xgi_kern_probe,
#if defined(XGI_SUPPORT_ACPI)
	.suspend = xgi_kern_acpi_standby,
	.resume = xgi_kern_acpi_resume,
#endif
};

/*
 * find xgi devices and set initial state
 */
int xgi_kern_probe(struct pci_dev *dev, const struct pci_device_id *id_table)
{
	xgi_info_t *info;

	if ((dev->vendor != PCI_VENDOR_ID_XGI)
	    || (dev->class != (PCI_CLASS_DISPLAY_VGA << 8))) {
		return -1;
	}

	if (xgi_num_devices == XGI_MAX_DEVICES) {
		XGI_INFO("maximum device number (%d) reached!\n",
			 xgi_num_devices);
		return -1;
	}

	/* enable io, mem, and bus-mastering in pci config space */
	if (pci_enable_device(dev) != 0) {
		XGI_INFO("pci_enable_device failed, aborting\n");
		return -1;
	}

	XGI_INFO("maximum device number (%d) reached \n", xgi_num_devices);

	pci_set_master(dev);

	info = &xgi_devices[xgi_num_devices];
	info->dev = dev;
	info->vendor_id = dev->vendor;
	info->device_id = dev->device;
	info->bus = dev->bus->number;
	info->slot = PCI_SLOT((dev)->devfn);

	xgi_lock_init(info);

	info->mmio.base = XGI_PCI_RESOURCE_START(dev, 1);
	info->mmio.size = XGI_PCI_RESOURCE_SIZE(dev, 1);

	/* check IO region */
	if (!request_mem_region(info->mmio.base, info->mmio.size, "xgi")) {
		XGI_ERROR("cannot reserve MMIO memory\n");
		goto error_disable_dev;
	}

	XGI_INFO("info->mmio.base: 0x%lx \n", info->mmio.base);
	XGI_INFO("info->mmio.size: 0x%lx \n", info->mmio.size);

	info->mmio.vbase = (unsigned char *)ioremap_nocache(info->mmio.base,
							    info->mmio.size);
	if (!info->mmio.vbase) {
		release_mem_region(info->mmio.base, info->mmio.size);
		XGI_ERROR("info->mmio.vbase failed\n");
		goto error_disable_dev;
	}
	xgi_enable_mmio(info);

	//xgi_enable_ge(info);

	XGI_INFO("info->mmio.vbase: 0x%p \n", info->mmio.vbase);

	info->fb.base = XGI_PCI_RESOURCE_START(dev, 0);
	info->fb.size = XGI_PCI_RESOURCE_SIZE(dev, 0);

	XGI_INFO("info->fb.base: 0x%lx \n", info->fb.base);
	XGI_INFO("info->fb.size: 0x%lx \n", info->fb.size);

	info->fb.size = bIn3cf(0x54) * 8 * 1024 * 1024;
	XGI_INFO("info->fb.size: 0x%lx \n", info->fb.size);

	/* check frame buffer region
	   if (!request_mem_region(info->fb.base, info->fb.size, "xgi"))
	   {
	   release_mem_region(info->mmio.base, info->mmio.size);
	   XGI_ERROR("cannot reserve frame buffer memory\n");
	   goto error_disable_dev;
	   }

	   info->fb.vbase = (unsigned char *)ioremap_nocache(info->fb.base,
	   info->fb.size);

	   if (!info->fb.vbase)
	   {
	   release_mem_region(info->mmio.base, info->mmio.size);
	   release_mem_region(info->fb.base, info->fb.size);
	   XGI_ERROR("info->fb.vbase failed\n");
	   goto error_disable_dev;
	   }
	 */
	info->fb.vbase = NULL;
	XGI_INFO("info->fb.vbase: 0x%p \n", info->fb.vbase);

	info->irq = dev->irq;

	/* check common error condition */
	if (info->irq == 0) {
		XGI_ERROR("Can't find an IRQ for your XGI card!  \n");
		goto error_zero_dev;
	}
	XGI_INFO("info->irq: %lx \n", info->irq);

	//xgi_enable_dvi_interrupt(info);

	/* sanity check the IO apertures */
	if ((info->mmio.base == 0) || (info->mmio.size == 0)
	    || (info->fb.base == 0) || (info->fb.size == 0)) {
		XGI_ERROR("The IO regions for your XGI card are invalid.\n");

		if ((info->mmio.base == 0) || (info->mmio.size == 0)) {
			XGI_ERROR("mmio appears to be wrong: 0x%lx 0x%lx\n",
				  info->mmio.base, info->mmio.size);
		}

		if ((info->fb.base == 0) || (info->fb.size == 0)) {
			XGI_ERROR
			    ("frame buffer appears to be wrong: 0x%lx 0x%lx\n",
			     info->fb.base, info->fb.size);
		}

		goto error_zero_dev;
	}
	//xgi_num_devices++;

	return 0;

      error_zero_dev:
	release_mem_region(info->fb.base, info->fb.size);
	release_mem_region(info->mmio.base, info->mmio.size);

      error_disable_dev:
	pci_disable_device(dev);
	return -1;

}

/*
 * vma operations...
 * this is only called when the vmas are duplicated. this
 * appears to only happen when the process is cloned to create
 * a new process, and not when the process is threaded.
 *
 * increment the usage count for the physical pages, so when
 * this clone unmaps the mappings, the pages are not
 * deallocated under the original process.
 */
struct vm_operations_struct xgi_vm_ops = {
	.open = xgi_kern_vma_open,
	.close = xgi_kern_vma_release,
	.nopage = xgi_kern_vma_nopage,
};

void xgi_kern_vma_open(struct vm_area_struct *vma)
{
	XGI_INFO("VM: vma_open for 0x%lx - 0x%lx, offset 0x%lx\n",
		 vma->vm_start, vma->vm_end, XGI_VMA_OFFSET(vma));

	if (XGI_VMA_PRIVATE(vma)) {
		xgi_pcie_block_t *block =
		    (xgi_pcie_block_t *) XGI_VMA_PRIVATE(vma);
		XGI_ATOMIC_INC(block->use_count);
	}
}

void xgi_kern_vma_release(struct vm_area_struct *vma)
{
	XGI_INFO("VM: vma_release for 0x%lx - 0x%lx, offset 0x%lx\n",
		 vma->vm_start, vma->vm_end, XGI_VMA_OFFSET(vma));

	if (XGI_VMA_PRIVATE(vma)) {
		xgi_pcie_block_t *block =
		    (xgi_pcie_block_t *) XGI_VMA_PRIVATE(vma);
		XGI_ATOMIC_DEC(block->use_count);

		/*
		 * if use_count is down to 0, the kernel virtual mapping was freed
		 * but the underlying physical pages were not, we need to clear the
		 * bit and free the physical pages.
		 */
		if (XGI_ATOMIC_READ(block->use_count) == 0) {
			// Need TO Finish
			XGI_VMA_PRIVATE(vma) = NULL;
		}
	}
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 1))
struct page *xgi_kern_vma_nopage(struct vm_area_struct *vma,
				 unsigned long address, int *type)
{
	xgi_pcie_block_t *block = (xgi_pcie_block_t *) XGI_VMA_PRIVATE(vma);
	struct page *page = NOPAGE_SIGBUS;
	unsigned long offset = 0;
	unsigned long page_addr = 0;
/*
    XGI_INFO("VM: mmap([0x%lx-0x%lx] off=0x%lx) address: 0x%lx \n",
              vma->vm_start,
              vma->vm_end,
              XGI_VMA_OFFSET(vma),
              address);
*/
	offset = (address - vma->vm_start) + XGI_VMA_OFFSET(vma);

	offset = offset - block->bus_addr;

	offset >>= PAGE_SHIFT;

	page_addr = block->page_table[offset].virt_addr;

	if (xgi_temp) {
		XGI_INFO("block->bus_addr: 0x%lx block->hw_addr: 0x%lx"
			 "block->page_count: 0x%lx block->page_order: 0x%lx"
			 "block->page_table[0x%lx].virt_addr: 0x%lx\n",
			 block->bus_addr, block->hw_addr,
			 block->page_count, block->page_order,
			 offset, block->page_table[offset].virt_addr);
		xgi_temp = 0;
	}

	if (!page_addr)
		goto out;	/* hole or end-of-file */
	page = virt_to_page(page_addr);

	/* got it, now increment the count */
	get_page(page);
      out:
	return page;

}
#else
struct page *xgi_kern_vma_nopage(struct vm_area_struct *vma,
				 unsigned long address, int write_access)
{
	xgi_pcie_block_t *block = (xgi_pcie_block_t *) XGI_VMA_PRIVATE(vma);
	struct page *page = NOPAGE_SIGBUS;
	unsigned long offset = 0;
	unsigned long page_addr = 0;
/*
    XGI_INFO("VM: mmap([0x%lx-0x%lx] off=0x%lx) address: 0x%lx \n",
              vma->vm_start,
              vma->vm_end,
              XGI_VMA_OFFSET(vma),
              address);
*/
	offset = (address - vma->vm_start) + XGI_VMA_OFFSET(vma);

	offset = offset - block->bus_addr;

	offset >>= PAGE_SHIFT;

	page_addr = block->page_table[offset].virt_addr;

	if (xgi_temp) {
		XGI_INFO("block->bus_addr: 0x%lx block->hw_addr: 0x%lx"
			 "block->page_count: 0x%lx block->page_order: 0x%lx"
			 "block->page_table[0x%lx].virt_addr: 0x%lx\n",
			 block->bus_addr, block->hw_addr,
			 block->page_count, block->page_order,
			 offset, block->page_table[offset].virt_addr);
		xgi_temp = 0;
	}

	if (!page_addr)
		goto out;	/* hole or end-of-file */
	page = virt_to_page(page_addr);

	/* got it, now increment the count */
	get_page(page);
      out:
	return page;
}
#endif

#if 0
static struct file_operations xgi_fops = {
	/* owner:      THIS_MODULE, */
      poll:xgi_kern_poll,
      ioctl:xgi_kern_ioctl,
      mmap:xgi_kern_mmap,
      open:xgi_kern_open,
      release:xgi_kern_release,
};
#endif

static struct file_operations xgi_fops = {
	.owner = THIS_MODULE,
	.poll = xgi_kern_poll,
	.ioctl = xgi_kern_ioctl,
	.mmap = xgi_kern_mmap,
	.open = xgi_kern_open,
	.release = xgi_kern_release,
};

static xgi_file_private_t *xgi_alloc_file_private(void)
{
	xgi_file_private_t *fp;

	XGI_KMALLOC(fp, sizeof(xgi_file_private_t));
	if (!fp)
		return NULL;

	memset(fp, 0, sizeof(xgi_file_private_t));

	/* initialize this file's event queue */
	init_waitqueue_head(&fp->wait_queue);

	xgi_init_lock(fp->fp_lock);

	return fp;
}

static void xgi_free_file_private(xgi_file_private_t * fp)
{
	if (fp == NULL)
		return;

	XGI_KFREE(fp, sizeof(xgi_file_private_t));
}

int xgi_kern_open(struct inode *inode, struct file *filp)
{
	xgi_info_t *info = NULL;
	int dev_num;
	int result = 0, status;

	/*
	 * the type and num values are only valid if we are not using devfs.
	 * However, since we use them to retrieve the device pointer, we
	 * don't need them with devfs as filp->private_data is already
	 * initialized
	 */
	filp->private_data = xgi_alloc_file_private();
	if (filp->private_data == NULL)
		return -ENOMEM;

	XGI_INFO("filp->private_data %p\n", filp->private_data);
	/*
	 * for control device, just jump to its open routine
	 * after setting up the private data
	 */
	if (XGI_IS_CONTROL_DEVICE(inode))
		return xgi_kern_ctl_open(inode, filp);

	/* what device are we talking about? */
	dev_num = XGI_DEVICE_NUMBER(inode);
	if (dev_num >= XGI_MAX_DEVICES) {
		xgi_free_file_private(filp->private_data);
		filp->private_data = NULL;
		return -ENODEV;
	}

	info = &xgi_devices[dev_num];

	XGI_INFO("Jong-xgi_kern_open on device %d\n", dev_num);

	xgi_down(info->info_sem);
	XGI_CHECK_PCI_CONFIG(info);

	XGI_INFO_FROM_FP(filp) = info;

	/*
	 * map the memory and allocate isr on first open
	 */

	if (!(info->flags & XGI_FLAG_OPEN)) {
		XGI_INFO("info->flags & XGI_FLAG_OPEN \n");

		if (info->device_id == 0) {
			XGI_INFO("open of nonexistent device %d\n", dev_num);
			result = -ENXIO;
			goto failed;
		}

		/* initialize struct irqaction */
		status = request_irq(info->irq, xgi_kern_isr,
				     SA_INTERRUPT | SA_SHIRQ, "xgi",
				     (void *)info);
		if (status != 0) {
			if (info->irq && (status == -EBUSY)) {
				XGI_ERROR
				    ("Tried to get irq %d, but another driver",
				     (unsigned int)info->irq);
				XGI_ERROR("has it and is not sharing it.\n");
			}
			XGI_ERROR("isr request failed 0x%x\n", status);
			result = -EIO;
			goto failed;
		}

		/*
		 * #define DECLARE_TASKLET(name, func, data) \
		 * struct tasklet_struct name = { NULL, 0, ATOMIC_INIT(0), func, data }
		 */
		info->tasklet.func = xgi_kern_isr_bh;
		info->tasklet.data = (unsigned long)info;
		tasklet_enable(&info->tasklet);

		/* Alloc 1M bytes for cmdbuffer which is flush2D batch array */
		xgi_cmdlist_initialize(info, 0x100000);

		info->flags |= XGI_FLAG_OPEN;
	}

	XGI_ATOMIC_INC(info->use_count);

      failed:
	xgi_up(info->info_sem);

	if ((result) && filp->private_data) {
		xgi_free_file_private(filp->private_data);
		filp->private_data = NULL;
	}

	return result;
}

int xgi_kern_release(struct inode *inode, struct file *filp)
{
	xgi_info_t *info = XGI_INFO_FROM_FP(filp);

	XGI_CHECK_PCI_CONFIG(info);

	/*
	 * for control device, just jump to its open routine
	 * after setting up the private data
	 */
	if (XGI_IS_CONTROL_DEVICE(inode))
		return xgi_kern_ctl_close(inode, filp);

	XGI_INFO("Jong-xgi_kern_release on device %d\n",
		 XGI_DEVICE_NUMBER(inode));

	xgi_down(info->info_sem);
	if (XGI_ATOMIC_DEC_AND_TEST(info->use_count)) {

		/*
		 * The usage count for this device has dropped to zero, it can be shut
		 * down safely; disable its interrupts.
		 */

		/*
		 * Disable this device's tasklet to make sure that no bottom half will
		 * run with undefined device state.
		 */
		tasklet_disable(&info->tasklet);

		/*
		 * Free the IRQ, which may block until all pending interrupt processing
		 * has completed.
		 */
		free_irq(info->irq, (void *)info);

		xgi_cmdlist_cleanup(info);

		/* leave INIT flag alone so we don't reinit every time */
		info->flags &= ~XGI_FLAG_OPEN;
	}

	xgi_up(info->info_sem);

	if (FILE_PRIVATE(filp)) {
		xgi_free_file_private(FILE_PRIVATE(filp));
		FILE_PRIVATE(filp) = NULL;
	}

	return 0;
}

int xgi_kern_mmap(struct file *filp, struct vm_area_struct *vma)
{
	//struct inode        *inode = INODE_FROM_FP(filp);
	xgi_info_t *info = XGI_INFO_FROM_FP(filp);
	xgi_pcie_block_t *block;
	int pages = 0;
	unsigned long prot;

	XGI_INFO("Jong-VM: mmap([0x%lx-0x%lx] off=0x%lx)\n",
		 vma->vm_start, vma->vm_end, XGI_VMA_OFFSET(vma));

	XGI_CHECK_PCI_CONFIG(info);

	if (XGI_MASK_OFFSET(vma->vm_start)
	    || XGI_MASK_OFFSET(vma->vm_end)) {
		XGI_ERROR("VM: bad mmap range: %lx - %lx\n",
			  vma->vm_start, vma->vm_end);
		return -ENXIO;
	}

	pages = (vma->vm_end - vma->vm_start) >> PAGE_SHIFT;

	vma->vm_ops = &xgi_vm_ops;

	/* XGI IO(reg) space */
	if (IS_IO_OFFSET
	    (info, XGI_VMA_OFFSET(vma), vma->vm_end - vma->vm_start)) {
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		if (XGI_REMAP_PAGE_RANGE(vma->vm_start,
					 XGI_VMA_OFFSET(vma),
					 vma->vm_end - vma->vm_start,
					 vma->vm_page_prot))
			return -EAGAIN;

		/* mark it as IO so that we don't dump it on core dump */
		vma->vm_flags |= VM_IO;
		XGI_INFO("VM: mmap io space \n");
	}
	/* XGI fb space */
	/* Jong 06/14/2006; moved behind PCIE or modify IS_FB_OFFSET */
	else if (IS_FB_OFFSET
		 (info, XGI_VMA_OFFSET(vma), vma->vm_end - vma->vm_start)) {
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		if (XGI_REMAP_PAGE_RANGE(vma->vm_start,
					 XGI_VMA_OFFSET(vma),
					 vma->vm_end - vma->vm_start,
					 vma->vm_page_prot))
			return -EAGAIN;

		// mark it as IO so that we don't dump it on core dump
		vma->vm_flags |= VM_IO;
		XGI_INFO("VM: mmap fb space \n");
	}
	/* PCIE allocator */
	/* XGI_VMA_OFFSET(vma) is offset based on pcie.base (HW address space) */
	else if (IS_PCIE_OFFSET
		 (info, XGI_VMA_OFFSET(vma), vma->vm_end - vma->vm_start)) {
		xgi_down(info->pcie_sem);

		block =
		    (xgi_pcie_block_t *) xgi_find_pcie_block(info,
							     XGI_VMA_OFFSET
							     (vma));

		if (block == NULL) {
			XGI_ERROR("couldn't find pre-allocated PCIE memory!\n");
			xgi_up(info->pcie_sem);
			return -EAGAIN;
		}

		if (block->page_count != pages) {
			XGI_ERROR
			    ("pre-allocated PCIE memory has wrong number of pages!\n");
			xgi_up(info->pcie_sem);
			return -EAGAIN;
		}

		vma->vm_private_data = block;
		XGI_ATOMIC_INC(block->use_count);
		xgi_up(info->pcie_sem);

		/*
		 * prevent the swapper from swapping it out
		 * mark the memory i/o so the buffers aren't
		 * dumped on core dumps */
		vma->vm_flags |= (VM_LOCKED | VM_IO);

		/* un-cached */
		prot = pgprot_val(vma->vm_page_prot);
		/* 
		   if (boot_cpu_data.x86 > 3)
		   prot |= _PAGE_PCD | _PAGE_PWT;
		 */
		vma->vm_page_prot = __pgprot(prot);

		XGI_INFO("VM: mmap pcie space \n");
	}
#if 0
	else if (IS_FB_OFFSET
		 (info, XGI_VMA_OFFSET(vma), vma->vm_end - vma->vm_start)) {
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		if (XGI_REMAP_PAGE_RANGE(vma->vm_start,
					 XGI_VMA_OFFSET(vma),
					 vma->vm_end - vma->vm_start,
					 vma->vm_page_prot))
			return -EAGAIN;

		// mark it as IO so that we don't dump it on core dump
		vma->vm_flags |= VM_IO;
		XGI_INFO("VM: mmap fb space \n");
	}
#endif
	else {
		vma->vm_flags |= (VM_IO | VM_LOCKED);
		XGI_ERROR("VM: mmap wrong range \n");
	}

	vma->vm_file = filp;

	return 0;
}

unsigned int xgi_kern_poll(struct file *filp, struct poll_table_struct *wait)
{
	xgi_file_private_t *fp;
	xgi_info_t *info;
	unsigned int mask = 0;
	unsigned long eflags;

	info = XGI_INFO_FROM_FP(filp);

	if (info->device_number == XGI_CONTROL_DEVICE_NUMBER)
		return xgi_kern_ctl_poll(filp, wait);

	fp = XGI_GET_FP(filp);

	if (!(filp->f_flags & O_NONBLOCK)) {
		/* add us to the list */
		poll_wait(filp, &fp->wait_queue, wait);
	}

	xgi_lock_irqsave(fp->fp_lock, eflags);

	/* wake the user on any event */
	if (fp->num_events) {
		XGI_INFO("Hey, an event occured!\n");
		/*
		 * trigger the client, when they grab the event,
		 * we'll decrement the event count
		 */
		mask |= (POLLPRI | POLLIN);
	}
	xgi_unlock_irqsave(fp->fp_lock, eflags);

	return mask;
}

int xgi_kern_ioctl(struct inode *inode, struct file *filp,
		   unsigned int cmd, unsigned long arg)
{
	xgi_info_t *info;
	xgi_mem_alloc_t *alloc = NULL;

	int status = 0;
	void *arg_copy;
	int arg_size;
	int err = 0;

	info = XGI_INFO_FROM_FP(filp);

	XGI_INFO("Jong-ioctl(0x%x, 0x%x, 0x%lx, 0x%x)\n", _IOC_TYPE(cmd),
		 _IOC_NR(cmd), arg, _IOC_SIZE(cmd));
	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != XGI_IOCTL_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > XGI_IOCTL_MAXNR)
		return -ENOTTY;

	/*
	 * the direction is a bitmask, and VERIFY_WRITE catches R/W
	 * transfers. `Type' is user-oriented, while
	 * access_ok is kernel-oriented, so the concept of "read" and
	 * "write" is reversed
	 */
	if (_IOC_DIR(cmd) & _IOC_READ) {
		err = !access_ok(VERIFY_WRITE, (void *)arg, _IOC_SIZE(cmd));
	} else if (_IOC_DIR(cmd) & _IOC_WRITE) {
		err = !access_ok(VERIFY_READ, (void *)arg, _IOC_SIZE(cmd));
	}
	if (err)
		return -EFAULT;

	XGI_CHECK_PCI_CONFIG(info);

	arg_size = _IOC_SIZE(cmd);
	XGI_KMALLOC(arg_copy, arg_size);
	if (arg_copy == NULL) {
		XGI_ERROR("failed to allocate ioctl memory\n");
		return -ENOMEM;
	}

	/* Jong 05/25/2006 */
	/* copy_from_user(arg_copy, (void *)arg, arg_size); */
	if (copy_from_user(arg_copy, (void *)arg, arg_size)) {
		XGI_ERROR("failed to copyin ioctl data\n");
		XGI_INFO("Jong-copy_from_user-fail! \n");
	} else
		XGI_INFO("Jong-copy_from_user-OK! \n");

	alloc = (xgi_mem_alloc_t *) arg_copy;
	XGI_INFO("Jong-succeeded in copy_from_user 0x%lx, 0x%x bytes.\n", arg,
		 arg_size);

	switch (_IOC_NR(cmd)) {
	case XGI_ESC_DEVICE_INFO:
		XGI_INFO("Jong-xgi_ioctl_get_device_info \n");
		xgi_get_device_info(info, (struct xgi_chip_info_s *)arg_copy);
		break;
	case XGI_ESC_POST_VBIOS:
		XGI_INFO("Jong-xgi_ioctl_post_vbios \n");
		break;
	case XGI_ESC_FB_ALLOC:
		XGI_INFO("Jong-xgi_ioctl_fb_alloc \n");
		xgi_fb_alloc(info, (struct xgi_mem_req_s *)arg_copy, alloc);
		break;
	case XGI_ESC_FB_FREE:
		XGI_INFO("Jong-xgi_ioctl_fb_free \n");
		xgi_fb_free(info, *(unsigned long *)arg_copy);
		break;
	case XGI_ESC_MEM_COLLECT:
		XGI_INFO("Jong-xgi_ioctl_mem_collect \n");
		xgi_mem_collect(info, (unsigned int *)arg_copy);
		break;
	case XGI_ESC_PCIE_ALLOC:
		XGI_INFO("Jong-xgi_ioctl_pcie_alloc \n");
		xgi_pcie_alloc(info, ((xgi_mem_req_t *) arg_copy)->size,
			       ((xgi_mem_req_t *) arg_copy)->owner, alloc);
		break;
	case XGI_ESC_PCIE_FREE:
		XGI_INFO("Jong-xgi_ioctl_pcie_free: bus_addr = 0x%lx \n",
			 *((unsigned long *)arg_copy));
		xgi_pcie_free(info, *((unsigned long *)arg_copy));
		break;
	case XGI_ESC_PCIE_CHECK:
		XGI_INFO("Jong-xgi_pcie_heap_check \n");
		xgi_pcie_heap_check();
		break;
	case XGI_ESC_GET_SCREEN_INFO:
		XGI_INFO("Jong-xgi_get_screen_info \n");
		xgi_get_screen_info(info, (struct xgi_screen_info_s *)arg_copy);
		break;
	case XGI_ESC_PUT_SCREEN_INFO:
		XGI_INFO("Jong-xgi_put_screen_info \n");
		xgi_put_screen_info(info, (struct xgi_screen_info_s *)arg_copy);
		break;
	case XGI_ESC_MMIO_INFO:
		XGI_INFO("Jong-xgi_ioctl_get_mmio_info \n");
		xgi_get_mmio_info(info, (struct xgi_mmio_info_s *)arg_copy);
		break;
	case XGI_ESC_GE_RESET:
		XGI_INFO("Jong-xgi_ioctl_ge_reset \n");
		xgi_ge_reset(info);
		break;
	case XGI_ESC_SAREA_INFO:
		XGI_INFO("Jong-xgi_ioctl_sarea_info \n");
		xgi_sarea_info(info, (struct xgi_sarea_info_s *)arg_copy);
		break;
	case XGI_ESC_DUMP_REGISTER:
		XGI_INFO("Jong-xgi_ioctl_dump_register \n");
		xgi_dump_register(info);
		break;
	case XGI_ESC_DEBUG_INFO:
		XGI_INFO("Jong-xgi_ioctl_restore_registers \n");
		xgi_restore_registers(info);
		//xgi_write_pcie_mem(info, (struct xgi_mem_req_s *) arg_copy);
		//xgi_read_pcie_mem(info, (struct xgi_mem_req_s *) arg_copy);
		break;
	case XGI_ESC_SUBMIT_CMDLIST:
		XGI_INFO("Jong-xgi_ioctl_submit_cmdlist \n");
		xgi_submit_cmdlist(info, (xgi_cmd_info_t *) arg_copy);
		break;
	case XGI_ESC_TEST_RWINKERNEL:
		XGI_INFO("Jong-xgi_test_rwinkernel \n");
		xgi_test_rwinkernel(info, *(unsigned long *)arg_copy);
		break;
	case XGI_ESC_STATE_CHANGE:
		XGI_INFO("Jong-xgi_state_change \n");
		xgi_state_change(info, (xgi_state_info_t *) arg_copy);
		break;
	case XGI_ESC_CPUID:
		XGI_INFO("Jong-XGI_ESC_CPUID \n");
		xgi_get_cpu_id((struct cpu_info_s *)arg_copy);
		break;
	default:
		XGI_INFO("Jong-xgi_ioctl_default \n");
		status = -EINVAL;
		break;
	}

	if (copy_to_user((void *)arg, arg_copy, arg_size)) {
		XGI_ERROR("failed to copyout ioctl data\n");
		XGI_INFO("Jong-copy_to_user-fail! \n");
	} else
		XGI_INFO("Jong-copy_to_user-OK! \n");

	XGI_KFREE(arg_copy, arg_size);
	return status;
}

/*
 * xgi control driver operations defined here
 */
int xgi_kern_ctl_open(struct inode *inode, struct file *filp)
{
	xgi_info_t *info = &xgi_ctl_device;

	int rc = 0;

	XGI_INFO("Jong-xgi_kern_ctl_open\n");

	xgi_down(info->info_sem);
	info->device_number = XGI_CONTROL_DEVICE_NUMBER;

	/* save the xgi info in file->private_data */
	filp->private_data = info;

	if (XGI_ATOMIC_READ(info->use_count) == 0) {
		init_waitqueue_head(&xgi_ctl_waitqueue);
	}

	info->flags |= XGI_FLAG_OPEN + XGI_FLAG_CONTROL;

	XGI_ATOMIC_INC(info->use_count);
	xgi_up(info->info_sem);

	return rc;
}

int xgi_kern_ctl_close(struct inode *inode, struct file *filp)
{
	xgi_info_t *info = XGI_INFO_FROM_FP(filp);

	XGI_INFO("Jong-xgi_kern_ctl_close\n");

	xgi_down(info->info_sem);
	if (XGI_ATOMIC_DEC_AND_TEST(info->use_count)) {
		info->flags = 0;
	}
	xgi_up(info->info_sem);

	if (FILE_PRIVATE(filp)) {
		xgi_free_file_private(FILE_PRIVATE(filp));
		FILE_PRIVATE(filp) = NULL;
	}

	return 0;
}

unsigned int xgi_kern_ctl_poll(struct file *filp, poll_table * wait)
{
	//xgi_info_t  *info = XGI_INFO_FROM_FP(filp);;
	unsigned int ret = 0;

	if (!(filp->f_flags & O_NONBLOCK)) {
		poll_wait(filp, &xgi_ctl_waitqueue, wait);
	}

	return ret;
}

/*
 * xgi proc system
 */
static u8 xgi_find_pcie_capability(struct pci_dev *dev)
{
	u16 status;
	u8 cap_ptr, cap_id;

	pci_read_config_word(dev, PCI_STATUS, &status);
	status &= PCI_STATUS_CAP_LIST;
	if (!status)
		return 0;

	switch (dev->hdr_type) {
	case PCI_HEADER_TYPE_NORMAL:
	case PCI_HEADER_TYPE_BRIDGE:
		pci_read_config_byte(dev, PCI_CAPABILITY_LIST, &cap_ptr);
		break;
	default:
		return 0;
	}

	do {
		cap_ptr &= 0xFC;
		pci_read_config_byte(dev, cap_ptr + PCI_CAP_LIST_ID, &cap_id);
		pci_read_config_byte(dev, cap_ptr + PCI_CAP_LIST_NEXT,
				     &cap_ptr);
	} while (cap_ptr && cap_id != 0xFF);

	return 0;
}

static struct pci_dev *xgi_get_pci_device(xgi_info_t * info)
{
	struct pci_dev *dev;

	dev = XGI_PCI_GET_DEVICE(info->vendor_id, info->device_id, NULL);
	while (dev) {
		if (XGI_PCI_SLOT_NUMBER(dev) == info->slot
		    && XGI_PCI_BUS_NUMBER(dev) == info->bus)
			return dev;
		dev = XGI_PCI_GET_DEVICE(info->vendor_id, info->device_id, dev);
	}

	return NULL;
}

int xgi_kern_read_card_info(char *page, char **start, off_t off,
			    int count, int *eof, void *data)
{
	struct pci_dev *dev;
	char *type;
	int len = 0;

	xgi_info_t *info;
	info = (xgi_info_t *) data;

	dev = xgi_get_pci_device(info);
	if (!dev)
		return 0;

	type = xgi_find_pcie_capability(dev) ? "PCIE" : "PCI";
	len += sprintf(page + len, "Card Type: \t %s\n", type);

	XGI_PCI_DEV_PUT(dev);
	return len;
}

int xgi_kern_read_version(char *page, char **start, off_t off,
			  int count, int *eof, void *data)
{
	int len = 0;

	len += sprintf(page + len, "XGI version: %s\n", "1.0");
	len += sprintf(page + len, "GCC version:  %s\n", "3.0");

	return len;
}

int xgi_kern_read_pcie_info(char *page, char **start, off_t off,
			    int count, int *eof, void *data)
{
	return 0;
}

int xgi_kern_read_status(char *page, char **start, off_t off,
			 int count, int *eof, void *data)
{
	return 0;
}

static void xgi_proc_create(void)
{
#ifdef CONFIG_PROC_FS

	struct pci_dev *dev;
	int i = 0;
	char name[6];

	struct proc_dir_entry *entry;
	struct proc_dir_entry *proc_xgi_pcie, *proc_xgi_cards;

	xgi_info_t *info;
	xgi_info_t *xgi_max_devices;

	/* world readable directory */
	int flags = S_IFDIR | S_IRUGO | S_IXUGO;

	proc_xgi = create_proc_entry("xgi", flags, proc_root_driver);
	if (!proc_xgi)
		goto failed;

	proc_xgi_cards = create_proc_entry("cards", flags, proc_xgi);
	if (!proc_xgi_cards)
		goto failed;

	proc_xgi_pcie = create_proc_entry("pcie", flags, proc_xgi);
	if (!proc_xgi_pcie)
		goto failed;

	/*
	 * Set the module owner to ensure that the reference
	 * count reflects accesses to the proc files.
	 */
	proc_xgi->owner = THIS_MODULE;
	proc_xgi_cards->owner = THIS_MODULE;
	proc_xgi_pcie->owner = THIS_MODULE;

	xgi_max_devices = xgi_devices + XGI_MAX_DEVICES;
	for (info = xgi_devices; info < xgi_max_devices; info++) {
		if (info->device_id == 0)
			break;

		/* world readable file */
		flags = S_IFREG | S_IRUGO;

		dev = xgi_get_pci_device(info);
		if (!dev)
			break;

		sprintf(name, "%d", i++);
		entry = create_proc_entry(name, flags, proc_xgi_cards);
		if (!entry) {
			XGI_PCI_DEV_PUT(dev);
			goto failed;
		}

		entry->data = info;
		entry->read_proc = xgi_kern_read_card_info;
		entry->owner = THIS_MODULE;

		if (xgi_find_pcie_capability(dev)) {
			entry =
			    create_proc_entry("status", flags, proc_xgi_pcie);
			if (!entry) {
				XGI_PCI_DEV_PUT(dev);
				goto failed;
			}

			entry->data = info;
			entry->read_proc = xgi_kern_read_status;
			entry->owner = THIS_MODULE;

			entry = create_proc_entry("card", flags, proc_xgi_pcie);
			if (!entry) {
				XGI_PCI_DEV_PUT(dev);
				goto failed;
			}

			entry->data = info;
			entry->read_proc = xgi_kern_read_pcie_info;
			entry->owner = THIS_MODULE;
		}

		XGI_PCI_DEV_PUT(dev);
	}

	entry = create_proc_entry("version", flags, proc_xgi);
	if (!entry)
		goto failed;

	entry->read_proc = xgi_kern_read_version;
	entry->owner = THIS_MODULE;

	entry = create_proc_entry("host-bridge", flags, proc_xgi_pcie);
	if (!entry)
		goto failed;

	entry->data = NULL;
	entry->read_proc = xgi_kern_read_pcie_info;
	entry->owner = THIS_MODULE;

	return;

      failed:
	XGI_ERROR("failed to create /proc entries!\n");
	xgi_proc_remove_all(proc_xgi);
#endif
}

#ifdef CONFIG_PROC_FS
static void xgi_proc_remove_all(struct proc_dir_entry *entry)
{
	while (entry) {
		struct proc_dir_entry *next = entry->next;
		if (entry->subdir)
			xgi_proc_remove_all(entry->subdir);
		remove_proc_entry(entry->name, entry->parent);
		if (entry == proc_xgi)
			break;
		entry = next;
	}
}
#endif

static void xgi_proc_remove(void)
{
#ifdef CONFIG_PROC_FS
	xgi_proc_remove_all(proc_xgi);
#endif
}

/*
 * driver receives an interrupt if someone waiting, then hand it off.
 */
irqreturn_t xgi_kern_isr(int irq, void *dev_id, struct pt_regs *regs)
{
	xgi_info_t *info = (xgi_info_t *) dev_id;
	u32 need_to_run_bottom_half = 0;

	//XGI_INFO("xgi_kern_isr \n");

	//XGI_CHECK_PCI_CONFIG(info);

	//xgi_dvi_irq_handler(info);

	if (need_to_run_bottom_half) {
		tasklet_schedule(&info->tasklet);
	}

	return IRQ_HANDLED;
}

void xgi_kern_isr_bh(unsigned long data)
{
	xgi_info_t *info = (xgi_info_t *) data;

	XGI_INFO("xgi_kern_isr_bh \n");

	//xgi_dvi_irq_handler(info);

	XGI_CHECK_PCI_CONFIG(info);
}

static void xgi_lock_init(xgi_info_t * info)
{
	if (info == NULL)
		return;

	spin_lock_init(&info->info_lock);

	sema_init(&info->info_sem, 1);
	sema_init(&info->fb_sem, 1);
	sema_init(&info->pcie_sem, 1);

	XGI_ATOMIC_SET(info->use_count, 0);
}

static void xgi_dev_init(xgi_info_t * info)
{
	struct pci_dev *pdev = NULL;
	struct xgi_dev *dev;
	int found = 0;
	u16 pci_cmd;

	XGI_INFO("Enter xgi_dev_init \n");

	//XGI_PCI_FOR_EACH_DEV(pdev)
	{
		for (dev = xgidev_list; dev->vendor; dev++) {
			if ((dev->vendor == pdev->vendor)
			    && (dev->device == pdev->device)) {
				XGI_INFO("dev->vendor = pdev->vendor= %x \n",
					 dev->vendor);
				XGI_INFO("dev->device = pdev->device= %x \n",
					 dev->device);

				xgi_devices[found].device_id = pdev->device;

				pci_read_config_byte(pdev, PCI_REVISION_ID,
						     &xgi_devices[found].
						     revision_id);

				XGI_INFO("PCI_REVISION_ID= %x \n",
					 xgi_devices[found].revision_id);

				pci_read_config_word(pdev, PCI_COMMAND,
						     &pci_cmd);

				XGI_INFO("PCI_COMMAND = %x \n", pci_cmd);

				break;
			}
		}
	}
}

/*
 * Export to Linux Kernel
 */

static int __init xgi_init_module(void)
{
	xgi_info_t *info = &xgi_devices[xgi_num_devices];
	int i, result;

	XGI_INFO("Jong-xgi kernel driver %s initializing\n", XGI_DRV_VERSION);
	//SET_MODULE_OWNER(&xgi_fops);

	memset(xgi_devices, 0, sizeof(xgi_devices));

	if (pci_register_driver(&xgi_pci_driver) < 0) {
		pci_unregister_driver(&xgi_pci_driver);
		XGI_ERROR("no XGI graphics adapter found\n");
		return -ENODEV;
	}

	XGI_INFO("Jong-xgi_devices[%d].fb.base.: 0x%lx \n", xgi_num_devices,
		 xgi_devices[xgi_num_devices].fb.base);
	XGI_INFO("Jong-xgi_devices[%d].fb.size.: 0x%lx \n", xgi_num_devices,
		 xgi_devices[xgi_num_devices].fb.size);

/* Jong 07/27/2006; test for ubuntu */
/*
#ifdef CONFIG_DEVFS_FS

    XGI_INFO("Jong-Use devfs \n");
    do
    {
        xgi_devfs_handles[0] = XGI_DEVFS_REGISTER("xgi", 0);
        if (xgi_devfs_handles[0] == NULL)
        {
            result = -ENOMEM;
            XGI_ERROR("devfs register failed\n");
            goto failed;
        }
    } while(0);
	#else *//* no devfs, do it the "classic" way  */

	XGI_INFO("Jong-Use non-devfs \n");
	/*
	 * Register your major, and accept a dynamic number. This is the
	 * first thing to do, in order to avoid releasing other module's
	 * fops in scull_cleanup_module()
	 */
	result = XGI_REGISTER_CHRDEV(xgi_major, "xgi", &xgi_fops);
	if (result < 0) {
		XGI_ERROR("register chrdev failed\n");
		pci_unregister_driver(&xgi_pci_driver);
		return result;
	}
	if (xgi_major == 0)
		xgi_major = result;	/* dynamic */

	/* #endif *//* CONFIG_DEVFS_FS */

	XGI_INFO("Jong-major number %d\n", xgi_major);

	/* instantiate tasklets */
	for (i = 0; i < XGI_MAX_DEVICES; i++) {
		/*
		 * We keep one tasklet per card to avoid latency issues with more
		 * than one device; no two instances of a single tasklet are ever
		 * executed concurrently.
		 */
		XGI_ATOMIC_SET(xgi_devices[i].tasklet.count, 1);
	}

	/* init the xgi control device */
	{
		xgi_info_t *info_ctl = &xgi_ctl_device;
		xgi_lock_init(info_ctl);
	}

	/* Init the resource manager */
	INIT_LIST_HEAD(&xgi_mempid_list);
	if (!xgi_fb_heap_init(info)) {
		XGI_ERROR("xgi_fb_heap_init() failed\n");
		result = -EIO;
		goto failed;
	}

	/* Init the resource manager */
	if (!xgi_pcie_heap_init(info)) {
		XGI_ERROR("xgi_pcie_heap_init() failed\n");
		result = -EIO;
		goto failed;
	}

	/* create /proc/driver/xgi */
	xgi_proc_create();

#if defined(DEBUG)
	inter_module_register("xgi_devices", THIS_MODULE, xgi_devices);
#endif

	return 0;

      failed:
#ifdef CONFIG_DEVFS_FS
	XGI_DEVFS_REMOVE_CONTROL();
	XGI_DEVFS_REMOVE_DEVICE(xgi_num_devices);
#endif

	if (XGI_UNREGISTER_CHRDEV(xgi_major, "xgi") < 0)
		XGI_ERROR("unregister xgi chrdev failed\n");

	for (i = 0; i < xgi_num_devices; i++) {
		if (xgi_devices[i].dev) {
			release_mem_region(xgi_devices[i].fb.base,
					   xgi_devices[i].fb.size);
			release_mem_region(xgi_devices[i].mmio.base,
					   xgi_devices[i].mmio.size);
		}
	}

	pci_unregister_driver(&xgi_pci_driver);
	return result;

	return 1;
}

void __exit xgi_exit_module(void)
{
	int i;

#ifdef CONFIG_DEVFS_FS
	XGI_DEVFS_REMOVE_DEVICE(xgi_num_devices);
#endif

	if (XGI_UNREGISTER_CHRDEV(xgi_major, "xgi") < 0)
		XGI_ERROR("unregister xgi chrdev failed\n");

	XGI_INFO("Jong-unregister xgi chrdev scceeded\n");
	for (i = 0; i < XGI_MAX_DEVICES; i++) {
		if (xgi_devices[i].dev) {
			/* clean up the flush2D batch array */
			xgi_cmdlist_cleanup(&xgi_devices[i]);

			if (xgi_devices[i].fb.vbase != NULL) {
				iounmap((void *)xgi_devices[i].fb.vbase);
				xgi_devices[i].fb.vbase = NULL;
			}
			if (xgi_devices[i].mmio.vbase != NULL) {
				iounmap((void *)xgi_devices[i].mmio.vbase);
				xgi_devices[i].mmio.vbase = NULL;
			}
			//release_mem_region(xgi_devices[i].fb.base, xgi_devices[i].fb.size);
			//XGI_INFO("release frame buffer mem region scceeded\n");

			release_mem_region(xgi_devices[i].mmio.base,
					   xgi_devices[i].mmio.size);
			XGI_INFO("release MMIO mem region scceeded\n");

			xgi_fb_heap_cleanup(&xgi_devices[i]);
			XGI_INFO("xgi_fb_heap_cleanup scceeded\n");

			xgi_pcie_heap_cleanup(&xgi_devices[i]);
			XGI_INFO("xgi_pcie_heap_cleanup scceeded\n");

			XGI_PCI_DISABLE_DEVICE(xgi_devices[i].dev);
		}
	}

	pci_unregister_driver(&xgi_pci_driver);

	/* remove /proc/driver/xgi */
	xgi_proc_remove();

#if defined(DEBUG)
	inter_module_unregister("xgi_devices");
#endif
}

module_init(xgi_init_module);
module_exit(xgi_exit_module);

#if defined(XGI_PM_SUPPORT_ACPI)
int xgi_acpi_event(struct pci_dev *dev, u32 state)
{
	return 1;
}

int xgi_kern_acpi_standby(struct pci_dev *dev, u32 state)
{
	return 1;
}

int xgi_kern_acpi_resume(struct pci_dev *dev)
{
	return 1;
}
#endif

MODULE_AUTHOR("Andrea Zhang <andrea_zhang@macrosynergy.com>");
MODULE_DESCRIPTION("xgi kernel driver for xgi cards");
MODULE_LICENSE("GPL");
