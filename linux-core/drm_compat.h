/**
 * \file drm_compat.h
 * Backward compatability definitions for Direct Rendering Manager
 *
 * \author Rickard E. (Rik) Faith <faith@valinux.com>
 * \author Gareth Hughes <gareth@valinux.com>
 */

/*
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <asm/agp.h>
#ifndef _DRM_COMPAT_H_
#define _DRM_COMPAT_H_

#ifndef minor
#define minor(x) MINOR((x))
#endif

#ifndef MODULE_LICENSE
#define MODULE_LICENSE(x)
#endif

#ifndef preempt_disable
#define preempt_disable()
#define preempt_enable()
#endif

#ifndef pte_offset_map
#define pte_offset_map pte_offset
#define pte_unmap(pte)
#endif

#ifndef module_param
#define module_param(name, type, perm)
#endif

#ifndef list_for_each_safe
#define list_for_each_safe(pos, n, head)				\
	for (pos = (head)->next, n = pos->next; pos != (head);		\
		pos = n, n = pos->next)
#endif

#ifndef list_for_each_entry
#define list_for_each_entry(pos, head, member)				\
       for (pos = list_entry((head)->next, typeof(*pos), member),	\
                    prefetch(pos->member.next);				\
            &pos->member != (head);					\
            pos = list_entry(pos->member.next, typeof(*pos), member),	\
                    prefetch(pos->member.next))
#endif

#ifndef list_for_each_entry_safe
#define list_for_each_entry_safe(pos, n, head, member)                  \
        for (pos = list_entry((head)->next, typeof(*pos), member),      \
                n = list_entry(pos->member.next, typeof(*pos), member); \
             &pos->member != (head);                                    \
             pos = n, n = list_entry(n->member.next, typeof(*n), member))
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,19)
static inline struct page *vmalloc_to_page(void *vmalloc_addr)
{
	unsigned long addr = (unsigned long)vmalloc_addr;
	struct page *page = NULL;
	pgd_t *pgd = pgd_offset_k(addr);
	pmd_t *pmd;
	pte_t *ptep, pte;

	if (!pgd_none(*pgd)) {
		pmd = pmd_offset(pgd, addr);
		if (!pmd_none(*pmd)) {
			preempt_disable();
			ptep = pte_offset_map(pmd, addr);
			pte = *ptep;
			if (pte_present(pte))
				page = pte_page(pte);
			pte_unmap(ptep);
			preempt_enable();
		}
	}
	return page;
}
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,2)
#define down_write down
#define up_write up
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define DRM_PCI_DEV(pdev) &pdev->dev
#else
#define DRM_PCI_DEV(pdev) NULL
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
static inline unsigned iminor(struct inode *inode)
{
	return MINOR(inode->i_rdev);
}

#define old_encode_dev(x) (x)

struct drm_sysfs_class;
struct class_simple;
struct device;

#define pci_dev_put(x) do {} while (0)
#define pci_get_subsys pci_find_subsys

static inline struct class_device *DRM(sysfs_device_add) (struct drm_sysfs_class
							  * cs, dev_t dev,
							  struct device *
							  device,
							  const char *fmt,
							  ...) {
	return NULL;
}

static inline void DRM(sysfs_device_remove) (dev_t dev) {
}

static inline void DRM(sysfs_destroy) (struct drm_sysfs_class * cs) {
}

static inline struct drm_sysfs_class *DRM(sysfs_create) (struct module * owner,
							 char *name) {
	return NULL;
}

#ifndef pci_pretty_name
#define pci_pretty_name(x) x->name
#endif

struct drm_device;
static inline int radeon_create_i2c_busses(struct drm_device *dev)
{
	return 0;
};
static inline void radeon_delete_i2c_busses(struct drm_device *dev)
{
};

#endif

#ifndef __user
#define __user
#endif

#if !defined(__put_page) 
#define __put_page(p)           atomic_dec(&(p)->count)
#endif

#if !defined(__GFP_COMP)
#define __GFP_COMP 0
#endif

#ifndef REMAP_PAGE_RANGE_5_ARGS
#define DRM_RPR_ARG(vma)
#else
#define DRM_RPR_ARG(vma) vma,
#endif

#define VM_OFFSET(vma) ((vma)->vm_pgoff << PAGE_SHIFT)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)
static inline int remap_pfn_range(struct vm_area_struct *vma, unsigned long from, unsigned long pfn, unsigned long size, pgprot_t pgprot)
{
  return remap_page_range(DRM_RPR_ARG(vma) from, 
			  pfn << PAGE_SHIFT,
			  size,
			  pgprot);
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
#define mutex_lock down
#define mutex_unlock up

#define mutex semaphore

#define mutex_init(a) sema_init((a), 1)

#endif

#ifndef DEFINE_SPINLOCK
#define DEFINE_SPINLOCK(x) spinlock_t x = SPIN_LOCK_UNLOCKED
#endif

/* old architectures */
#ifdef __AMD64__
#define __x86_64__
#endif

#ifndef pci_pretty_name
#define pci_pretty_name(dev) ""
#endif

/* sysfs __ATTR macro */
#ifndef __ATTR
#define __ATTR(_name,_mode,_show,_store) { \
        .attr = {.name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     \
        .show   = _show,                                        \
        .store  = _store,                                       \
}
#endif

#include <linux/mm.h>
#include <asm/page.h>

#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)) && \
     (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15))) 
#define DRM_ODD_MM_COMPAT
#endif



/*
 * Flush relevant caches and clear a VMA structure so that page references 
 * will cause a page fault. Don't flush tlbs.
 */

extern void drm_clear_vma(struct vm_area_struct *vma,
			  unsigned long addr, unsigned long end);

/*
 * Return the PTE protection map entries for the VMA flags given by 
 * flags. This is a functional interface to the kernel's protection map.
 */

extern pgprot_t vm_get_page_prot(unsigned long vm_flags);

/*
 * These are similar to the current kernel gatt pages allocator, only that we
 * want a struct page pointer instead of a virtual address. This allows for pages
 * that are not in the kernel linear map.
 */

#define drm_alloc_gatt_pages(order) ({					\
			void *_virt = alloc_gatt_pages(order);		\
			((_virt) ? virt_to_page(_virt) : NULL);})
#define drm_free_gatt_pages(pages, order) free_gatt_pages(page_address(pages), order) 

#if defined(CONFIG_X86) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15))

/*
 * These are too slow in earlier kernels.
 */

extern int drm_unmap_page_from_agp(struct page *page);
extern int drm_map_page_into_agp(struct page *page);

#define map_page_into_agp drm_map_page_into_agp
#define unmap_page_from_agp drm_unmap_page_from_agp
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))

/*
 * Hopefully, real NOPAGE_RETRY functionality will be in 2.6.19. 
 * For now, just return a dummy page that we've allocated out of 
 * static space. The page will be put by do_nopage() since we've already
 * filled out the pte.
 */

struct fault_data {
	struct vm_area_struct *vma;
	unsigned long address;
	pgoff_t pgoff;
	unsigned int flags;
	
	int type;
};

extern struct page *get_nopage_retry(void);
extern void free_nopage_retry(void);

#define NOPAGE_REFAULT get_nopage_retry()

extern int vm_insert_pfn(struct vm_area_struct *vma, unsigned long addr, 
			 unsigned long pfn, pgprot_t pgprot);

extern struct page *drm_vm_ttm_nopage(struct vm_area_struct *vma,
				      unsigned long address, 
				      int *type);

extern struct page *drm_vm_ttm_fault(struct vm_area_struct *vma, 
				     struct fault_data *data);

#endif

#ifdef DRM_ODD_MM_COMPAT

struct drm_ttm;


/*
 * Add a vma to the ttm vma list, and the 
 * process mm pointer to the ttm mm list. Needs the ttm mutex.
 */

extern int drm_ttm_add_vma(struct drm_ttm * ttm, 
			   struct vm_area_struct *vma);
/*
 * Delete a vma and the corresponding mm pointer from the
 * ttm lists. Needs the ttm mutex.
 */
extern void drm_ttm_delete_vma(struct drm_ttm * ttm, 
			       struct vm_area_struct *vma);

/*
 * Attempts to lock all relevant mmap_sems for a ttm, while
 * not releasing the ttm mutex. May return -EAGAIN to avoid 
 * deadlocks. In that case the caller shall release the ttm mutex,
 * schedule() and try again.
 */

extern int drm_ttm_lock_mm(struct drm_ttm * ttm);

/*
 * Unlock all relevant mmap_sems for a ttm.
 */
extern void drm_ttm_unlock_mm(struct drm_ttm * ttm);

/*
 * If the ttm was bound to the aperture, this function shall be called
 * with all relevant mmap sems held. It deletes the flag VM_PFNMAP from all
 * vmas mapping this ttm. This is needed just after unmapping the ptes of
 * the vma, otherwise the do_nopage() function will bug :(. The function
 * releases the mmap_sems for this ttm.
 */

extern void drm_ttm_finish_unmap(struct drm_ttm *ttm);

/*
 * Remap all vmas of this ttm using io_remap_pfn_range. We cannot 
 * fault these pfns in, because the first one will set the vma VM_PFNMAP
 * flag, which will make the next fault bug in do_nopage(). The function
 * releases the mmap_sems for this ttm.
 */

extern int drm_ttm_remap_bound(struct drm_ttm *ttm);


/*
 * Remap a vma for a bound ttm. Call with the ttm mutex held and
 * the relevant mmap_sem locked.
 */
extern int drm_ttm_map_bound(struct vm_area_struct *vma);

#endif
#endif
