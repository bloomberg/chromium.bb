
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

#ifndef _XGI_LINUX_H_
#define _XGI_LINUX_H_

#include <linux/config.h>

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
#   error "This driver does not support pre-2.6 kernels!"
#endif

#if defined (CONFIG_SMP) && !defined (__SMP__)
#define __SMP__
#endif

#if defined (CONFIG_MODVERSIONS) && !defined (MODVERSIONS)
#define MODVERSIONS
#endif

#include <linux/kernel.h>	/* printk */
#include <linux/module.h>

#include <linux/init.h>		/* module_init, module_exit         */
#include <linux/types.h>	/* pic_t, size_t, __u32, etc        */
#include <linux/errno.h>	/* error codes                      */
#include <linux/list.h>		/* circular linked list             */
#include <linux/stddef.h>	/* NULL, offsetof                   */
#include <linux/wait.h>		/* wait queues                      */

#include <linux/slab.h>		/* kmalloc, kfree, etc              */
#include <linux/vmalloc.h>	/* vmalloc, vfree, etc              */

#include <linux/poll.h>		/* poll_wait                        */
#include <linux/delay.h>	/* mdelay, udelay                   */
#include <asm/msr.h>		/* rdtsc rdtscl                     */

#include <linux/sched.h>	/* suser(), capable() replacement
				   for_each_task, for_each_process  */
#ifdef for_each_process
#define XGI_SCAN_PROCESS(p) for_each_process(p)
#else
#define XGI_SCAN_PROCESS(p) for_each_task(p)
#endif

#include <linux/moduleparam.h>	/* module_param()                   */
#include <linux/smp_lock.h>	/* kernel_locked                    */
#include <asm/tlbflush.h>	/* flush_tlb(), flush_tlb_all()     */
#include <asm/kmap_types.h>	/* page table entry lookup          */

#include <linux/pci.h>		/* pci_find_class, etc              */
#include <linux/interrupt.h>	/* tasklets, interrupt helpers      */
#include <linux/timer.h>

#include <asm/system.h>		/* cli, sli, save_flags             */
#include <asm/io.h>		/* ioremap, virt_to_phys            */
#include <asm/uaccess.h>	/* access_ok                        */
#include <asm/page.h>		/* PAGE_OFFSET                      */
#include <asm/pgtable.h>	/* pte bit definitions              */

#include <linux/spinlock.h>
#include <asm/semaphore.h>
#include <linux/highmem.h>

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif

#ifdef CONFIG_DEVFS_FS
#include <linux/devfs_fs_kernel.h>
#endif

#ifdef CONFIG_KMOD
#include <linux/kmod.h>
#endif

#ifdef CONFIG_PM
#include <linux/pm.h>
#endif

#ifdef CONFIG_MTRR
#include <asm/mtrr.h>
#endif

#ifdef CONFIG_KDB
#include <linux/kdb.h>
#include <asm/kdb.h>
#endif

#if defined (CONFIG_AGP) || defined (CONFIG_AGP_MODULE)
#define AGPGART
#include <linux/agp_backend.h>
#include <linux/agpgart.h>
#endif

#ifndef MAX_ORDER
#define MAX_ORDER 11
#endif

#ifndef module_init
#define module_init(x)  int init_module(void) { return x(); }
#define module_exit(x)  void cleanup_module(void) { x(); }
#endif

#ifndef minor
#define minor(x) MINOR(x)
#endif

#ifndef IRQ_HANDLED
typedef void irqreturn_t;
#define IRQ_NONE
#define IRQ_HANDLED
#define IRQ_RETVAL(x)
#endif

#if !defined (list_for_each)
#define list_for_each(pos, head) \
    for (pos = (head)->next, prefetch(pos->next); pos != (head); \
         pos = pos->next, prefetch(pos->next))
#endif

extern struct list_head pci_devices;	/* list of all devices */
#define XGI_PCI_FOR_EACH_DEV(dev) \
    for(dev = pci_dev_g(pci_devices.next); dev != pci_dev_g(&pci_devices); dev = pci_dev_g(dev->global_list.next))

/*
 * the following macro causes problems when used in the same module
 * as module_param(); undef it so we don't accidentally mix the two
 */
#undef  MODULE_PARM

#ifdef EXPORT_NO_SYMBOLS
EXPORT_NO_SYMBOLS;
#endif

#define XGI_IS_SUSER()                 capable(CAP_SYS_ADMIN)
#define XGI_PCI_DEVICE_NAME(dev)       ((dev)->pretty_name)
#define XGI_NUM_CPUS()                 num_online_cpus()
#define XGI_CLI()                      local_irq_disable()
#define XGI_SAVE_FLAGS(eflags)         local_save_flags(eflags)
#define XGI_RESTORE_FLAGS(eflags)      local_irq_restore(eflags)
#define XGI_MAY_SLEEP()                (!in_interrupt() && !in_atomic())
#define XGI_MODULE_PARAMETER(x)        module_param(x, int, 0)


/* Earlier 2.4.x kernels don't have pci_disable_device() */
#ifdef XGI_PCI_DISABLE_DEVICE_PRESENT
#define XGI_PCI_DISABLE_DEVICE(dev)      pci_disable_device(dev)
#else
#define XGI_PCI_DISABLE_DEVICE(dev)
#endif

/* common defines */
#define GET_MODULE_SYMBOL(mod,sym)    (const void *) inter_module_get(sym)
#define PUT_MODULE_SYMBOL(sym)        inter_module_put((char *) sym)

#define XGI_GET_PAGE_STRUCT(phys_page) virt_to_page(__va(phys_page))
#define XGI_VMA_OFFSET(vma)            (((vma)->vm_pgoff) << PAGE_SHIFT)
#define XGI_VMA_PRIVATE(vma)           ((vma)->vm_private_data)

#define XGI_DEVICE_NUMBER(x)           minor((x)->i_rdev)
#define XGI_IS_CONTROL_DEVICE(x)       (minor((x)->i_rdev) == 255)

#define XGI_PCI_RESOURCE_START(dev, bar) ((dev)->resource[bar].start)
#define XGI_PCI_RESOURCE_SIZE(dev, bar)  ((dev)->resource[bar].end - (dev)->resource[bar].start + 1)

#define XGI_PCI_BUS_NUMBER(dev)        (dev)->bus->number
#define XGI_PCI_SLOT_NUMBER(dev)       PCI_SLOT((dev)->devfn)

#ifdef XGI_PCI_GET_CLASS_PRESENT
#define XGI_PCI_DEV_PUT(dev)                    pci_dev_put(dev)
#define XGI_PCI_GET_DEVICE(vendor,device,from)  pci_get_device(vendor,device,from)
#define XGI_PCI_GET_SLOT(bus,devfn)             pci_get_slot(pci_find_bus(0,bus),devfn)
#define XGI_PCI_GET_CLASS(class,from)           pci_get_class(class,from)
#else
#define XGI_PCI_DEV_PUT(dev)
#define XGI_PCI_GET_DEVICE(vendor,device,from)  pci_find_device(vendor,device,from)
#define XGI_PCI_GET_SLOT(bus,devfn)             pci_find_slot(bus,devfn)
#define XGI_PCI_GET_CLASS(class,from)           pci_find_class(class,from)
#endif

/*
 * acpi support has been back-ported to the 2.4 kernel, but the 2.4 driver
 * model is not sufficient for full acpi support. it may work in some cases,
 * but not enough for us to officially support this configuration.
 */
#if defined(CONFIG_ACPI)
#define XGI_PM_SUPPORT_ACPI
#endif

#if defined(CONFIG_APM) || defined(CONFIG_APM_MODULE)
#define XGI_PM_SUPPORT_APM
#endif

#if defined(CONFIG_DEVFS_FS)
typedef void *devfs_handle_t;
#define XGI_DEVFS_REGISTER(_name, _minor) \
    ({ \
        devfs_handle_t __handle = NULL; \
        if (devfs_mk_cdev(MKDEV(XGI_DEV_MAJOR, _minor), \
                          S_IFCHR | S_IRUGO | S_IWUGO, _name) == 0) \
        { \
            __handle = (void *) 1; /* XXX Fix me! (boolean) */ \
        } \
        __handle; \
    })
/*
#define XGI_DEVFS_REMOVE_DEVICE(i) devfs_remove("xgi%d", i)
*/
#define XGI_DEVFS_REMOVE_CONTROL() devfs_remove("xgi_ctl")
#define XGI_DEVFS_REMOVE_DEVICE(i) devfs_remove("xgi")
#endif				/* defined(CONFIG_DEVFS_FS) */

#define XGI_REGISTER_CHRDEV(x...)    register_chrdev(x)
#define XGI_UNREGISTER_CHRDEV(x...)  unregister_chrdev(x)

#if defined(XGI_REMAP_PFN_RANGE_PRESENT)
#define XGI_REMAP_PAGE_RANGE(from, offset, x...) \
    remap_pfn_range(vma, from, ((offset) >> PAGE_SHIFT), x)
#elif defined(XGI_REMAP_PAGE_RANGE_5)
#define XGI_REMAP_PAGE_RANGE(x...)      remap_page_range(vma, x)
#elif defined(XGI_REMAP_PAGE_RANGE_4)
#define XGI_REMAP_PAGE_RANGE(x...)      remap_page_range(x)
#else
#warning "xgi_configure.sh failed, assuming remap_page_range(5)!"
#define XGI_REMAP_PAGE_RANGE(x...)      remap_page_range(vma, x)
#endif

#if defined(pmd_offset_map)
#define XGI_PMD_OFFSET(addres, pg_dir, pg_mid_dir) \
    { \
        pg_mid_dir = pmd_offset_map(pg_dir, address); \
    }
#define XGI_PMD_UNMAP(pg_mid_dir) \
    { \
        pmd_unmap(pg_mid_dir); \
    }
#else
#define XGI_PMD_OFFSET(addres, pg_dir, pg_mid_dir) \
    { \
        pg_mid_dir = pmd_offset(pg_dir, address); \
    }
#define XGI_PMD_UNMAP(pg_mid_dir)
#endif

#define XGI_PMD_PRESENT(pg_mid_dir) \
    ({ \
        if ((pg_mid_dir) && (pmd_none(*pg_mid_dir))) \
        { \
            XGI_PMD_UNMAP(pg_mid_dir); \
            pg_mid_dir = NULL; \
        } \
        pg_mid_dir != NULL; \
    })

#if defined(pte_offset_atomic)
#define XGI_PTE_OFFSET(addres, pg_mid_dir, pte) \
    { \
        pte = pte_offset_atomic(pg_mid_dir, address); \
        XGI_PMD_UNMAP(pg_mid_dir); \
    }
#define XGI_PTE_UNMAP(pte) \
    { \
        pte_kunmap(pte); \
    }
#elif defined(pte_offset)
#define XGI_PTE_OFFSET(addres, pg_mid_dir, pte) \
    { \
        pte = pte_offset(pg_mid_dir, address); \
        XGI_PMD_UNMAP(pg_mid_dir); \
    }
#define XGI_PTE_UNMAP(pte)
#else
#define XGI_PTE_OFFSET(addres, pg_mid_dir, pte) \
    { \
        pte = pte_offset_map(pg_mid_dir, address); \
        XGI_PMD_UNMAP(pg_mid_dir); \
    }
#define XGI_PTE_UNMAP(pte) \
    { \
        pte_unmap(pte); \
    }
#endif

#define XGI_PTE_PRESENT(pte) \
    ({ \
        if (pte) \
        { \
            if (!pte_present(*pte)) \
            { \
                XGI_PTE_UNMAP(pte); pte = NULL; \
            } \
        } \
        pte != NULL; \
    })

#define XGI_PTE_VALUE(pte) \
    ({ \
        unsigned long __pte_value = pte_val(*pte); \
        XGI_PTE_UNMAP(pte); \
        __pte_value; \
    })

#define XGI_PAGE_ALIGN(addr)             (((addr) + PAGE_SIZE - 1) / PAGE_SIZE)
#define XGI_MASK_OFFSET(addr)            ((addr) & (PAGE_SIZE - 1))

#if !defined (pgprot_noncached)
static inline pgprot_t pgprot_noncached(pgprot_t old_prot)
{
	pgprot_t new_prot = old_prot;
	if (boot_cpu_data.x86 > 3)
		new_prot = __pgprot(pgprot_val(old_prot) | _PAGE_PCD);
	return new_prot;
}
#endif

#if defined(XGI_BUILD_XGI_PAT_SUPPORT) && !defined (pgprot_writecombined)
/* Added define for write combining page, only valid if pat enabled. */
#define _PAGE_WRTCOMB _PAGE_PWT
#define __PAGE_KERNEL_WRTCOMB \
    (_PAGE_PRESENT | _PAGE_RW | _PAGE_DIRTY | _PAGE_WRTCOMB | _PAGE_ACCESSED)
#define PAGE_KERNEL_WRTCOMB MAKE_GLOBAL(__PAGE_KERNEL_WRTCOMB)

static inline pgprot_t pgprot_writecombined(pgprot_t old_prot)
{
	pgprot_t new_prot = old_prot;
	if (boot_cpu_data.x86 > 3) {
		pgprot_val(old_prot) &= ~(_PAGE_PCD | _PAGE_PWT);
		new_prot = __pgprot(pgprot_val(old_prot) | _PAGE_WRTCOMB);
	}
	return new_prot;
}
#endif

#if !defined(page_to_pfn)
#define page_to_pfn(page)  ((page) - mem_map)
#endif

#define XGI_VMALLOC(ptr, size) \
    { \
        (ptr) = vmalloc_32(size); \
    }

#define XGI_VFREE(ptr, size) \
    { \
        vfree((void *) (ptr)); \
    }

#define XGI_IOREMAP(ptr, physaddr, size) \
    { \
        (ptr) = ioremap(physaddr, size); \
    }

#define XGI_IOREMAP_NOCACHE(ptr, physaddr, size) \
    { \
        (ptr) = ioremap_nocache(physaddr, size); \
    }

#define XGI_IOUNMAP(ptr, size) \
    { \
        iounmap(ptr); \
    }

/*
 * only use this because GFP_KERNEL may sleep..
 * GFP_ATOMIC is ok, it won't sleep
 */
#define XGI_KMALLOC(ptr, size) \
    { \
        (ptr) = kmalloc(size, GFP_KERNEL); \
    }

#define XGI_KMALLOC_ATOMIC(ptr, size) \
    { \
        (ptr) = kmalloc(size, GFP_ATOMIC); \
    }

#define XGI_KFREE(ptr, size) \
    { \
        kfree((void *) (ptr)); \
    }

#define XGI_GET_FREE_PAGES(ptr, order) \
    { \
        (ptr) = __get_free_pages(GFP_KERNEL, order); \
    }

#define XGI_FREE_PAGES(ptr, order) \
    { \
        free_pages(ptr, order); \
    }

typedef struct xgi_pte_s {
	unsigned long phys_addr;
	unsigned long virt_addr;
} xgi_pte_t;

/*
 * AMD Athlon processors expose a subtle bug in the Linux
 * kernel, that may lead to AGP memory corruption. Recent
 * kernel versions had a workaround for this problem, but
 * 2.4.20 is the first kernel to address it properly. The
 * page_attr API provides the means to solve the problem.
 */
#if defined(XGI_CHANGE_PAGE_ATTR_PRESENT)
static inline void XGI_SET_PAGE_ATTRIB_UNCACHED(xgi_pte_t * page_ptr)
{
	struct page *page = virt_to_page(__va(page_ptr->phys_addr));
	change_page_attr(page, 1, PAGE_KERNEL_NOCACHE);
}
static inline void XGI_SET_PAGE_ATTRIB_CACHED(xgi_pte_t * page_ptr)
{
	struct page *page = virt_to_page(__va(page_ptr->phys_addr));
	change_page_attr(page, 1, PAGE_KERNEL);
}
#else
#define XGI_SET_PAGE_ATTRIB_UNCACHED(page_list)
#define XGI_SET_PAGE_ATTRIB_CACHED(page_list)
#endif

/* add for SUSE 9, Jill*/
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 4)
#define XGI_INC_PAGE_COUNT(page)    atomic_inc(&(page)->count)
#define XGI_DEC_PAGE_COUNT(page)    atomic_dec(&(page)->count)
#define XGI_PAGE_COUNT(page)		atomic_read(&(page)->count)
#define XGI_SET_PAGE_COUNT(page,v) 	atomic_set(&(page)->count, v)
#else
#define XGI_INC_PAGE_COUNT(page)    atomic_inc(&(page)->_count)
#define XGI_DEC_PAGE_COUNT(page)    atomic_dec(&(page)->_count)
#define XGI_PAGE_COUNT(page)		atomic_read(&(page)->_count)
#define XGI_SET_PAGE_COUNT(page,v) 	atomic_set(&(page)->_count, v)
#endif
#define XGILockPage(page)           SetPageLocked(page)
#define XGIUnlockPage(page)         ClearPageLocked(page)

/*
 * hide a pointer to struct xgi_info_t in a file-private info
 */

typedef struct {
	void *info;
	U32 num_events;
	spinlock_t fp_lock;
	wait_queue_head_t wait_queue;
} xgi_file_private_t;

#define FILE_PRIVATE(filp)      ((filp)->private_data)

#define XGI_GET_FP(filp)        ((xgi_file_private_t *) FILE_PRIVATE(filp))

/* for the card devices */
#define XGI_INFO_FROM_FP(filp)  (XGI_GET_FP(filp)->info)

#define INODE_FROM_FP(filp) ((filp)->f_dentry->d_inode)

#define XGI_ATOMIC_SET(data,val)         atomic_set(&(data), (val))
#define XGI_ATOMIC_INC(data)             atomic_inc(&(data))
#define XGI_ATOMIC_DEC(data)             atomic_dec(&(data))
#define XGI_ATOMIC_DEC_AND_TEST(data)    atomic_dec_and_test(&(data))
#define XGI_ATOMIC_READ(data)            atomic_read(&(data))

/*
 * lock-related functions that should only be called from this file
 */
#define xgi_init_lock(lock)             spin_lock_init(&lock)
#define xgi_lock(lock)                  spin_lock(&lock)
#define xgi_unlock(lock)                spin_unlock(&lock)
#define xgi_down(lock)                  down(&lock)
#define xgi_up(lock)                    up(&lock)

#define xgi_lock_irqsave(lock,flags)    spin_lock_irqsave(&lock,flags)
#define xgi_unlock_irqsave(lock,flags)  spin_unlock_irqrestore(&lock,flags)

#endif
