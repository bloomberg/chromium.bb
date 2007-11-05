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

/* older kernels had different irq args */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))
#undef DRM_IRQ_ARGS
#define DRM_IRQ_ARGS		int irq, void *arg, struct pt_regs *regs

typedef _Bool bool;
enum {
        false   = 0,
        true    = 1
};

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

#ifndef __user
#define __user
#endif

#if !defined(__put_page) 
#define __put_page(p)           atomic_dec(&(p)->count)
#endif

#if !defined(__GFP_COMP)
#define __GFP_COMP 0
#endif

#if !defined(IRQF_SHARED)
#define IRQF_SHARED SA_SHIRQ
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)
static inline int remap_pfn_range(struct vm_area_struct *vma, unsigned long from, unsigned long pfn, unsigned long size, pgprot_t pgprot)
{
  return remap_page_range(vma, from, 
			  pfn << PAGE_SHIFT,
			  size,
			  pgprot);
}

static __inline__ void *kcalloc(size_t nmemb, size_t size, int flags)
{
	void *addr;

	addr = kmalloc(size * nmemb, flags);
	if (addr != NULL)
		memset((void *)addr, 0, size * nmemb);

	return addr;
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

/* sysfs __ATTR macro */
#ifndef __ATTR
#define __ATTR(_name,_mode,_show,_store) { \
        .attr = {.name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     \
        .show   = _show,                                        \
        .store  = _store,                                       \
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
#define vmalloc_user(_size) ({void * tmp = vmalloc(_size);   \
      if (tmp) memset(tmp, 0, _size);			     \
      (tmp);})
#endif

#ifndef list_for_each_entry_safe_reverse
#define list_for_each_entry_safe_reverse(pos, n, head, member)          \
        for (pos = list_entry((head)->prev, typeof(*pos), member),      \
                n = list_entry(pos->member.prev, typeof(*pos), member); \
             &pos->member != (head);                                    \
             pos = n, n = list_entry(n->member.prev, typeof(*n), member))
#endif

#include <linux/mm.h>
#include <asm/page.h>

#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)) && \
     (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15)))
#define DRM_ODD_MM_COMPAT
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21))
#define DRM_FULL_MM_COMPAT
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

#ifndef GFP_DMA32
#define GFP_DMA32 GFP_KERNEL
#endif
#ifndef __GFP_DMA32
#define __GFP_DMA32 GFP_KERNEL
#endif

#if defined(CONFIG_X86) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15))

/*
 * These are too slow in earlier kernels.
 */

extern int drm_unmap_page_from_agp(struct page *page);
extern int drm_map_page_into_agp(struct page *page);

#define map_page_into_agp drm_map_page_into_agp
#define unmap_page_from_agp drm_unmap_page_from_agp
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15))
extern struct page *get_nopage_retry(void);
extern void free_nopage_retry(void);

#define NOPAGE_REFAULT get_nopage_retry()
#endif


#ifndef DRM_FULL_MM_COMPAT

/*
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

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))
extern struct page *drm_bo_vm_nopage(struct vm_area_struct *vma,
				     unsigned long address, 
				     int *type);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)) && \
  !defined(DRM_FULL_MM_COMPAT)
extern unsigned long drm_bo_vm_nopfn(struct vm_area_struct *vma,
				     unsigned long address);
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)) */
#endif /* ndef DRM_FULL_MM_COMPAT */

#ifdef DRM_ODD_MM_COMPAT

struct drm_buffer_object;


/*
 * Add a vma to the ttm vma list, and the 
 * process mm pointer to the ttm mm list. Needs the ttm mutex.
 */

extern int drm_bo_add_vma(struct drm_buffer_object * bo, 
			   struct vm_area_struct *vma);
/*
 * Delete a vma and the corresponding mm pointer from the
 * ttm lists. Needs the ttm mutex.
 */
extern void drm_bo_delete_vma(struct drm_buffer_object * bo, 
			      struct vm_area_struct *vma);

/*
 * Attempts to lock all relevant mmap_sems for a ttm, while
 * not releasing the ttm mutex. May return -EAGAIN to avoid 
 * deadlocks. In that case the caller shall release the ttm mutex,
 * schedule() and try again.
 */

extern int drm_bo_lock_kmm(struct drm_buffer_object * bo);

/*
 * Unlock all relevant mmap_sems for a ttm.
 */
extern void drm_bo_unlock_kmm(struct drm_buffer_object * bo);

/*
 * If the ttm was bound to the aperture, this function shall be called
 * with all relevant mmap sems held. It deletes the flag VM_PFNMAP from all
 * vmas mapping this ttm. This is needed just after unmapping the ptes of
 * the vma, otherwise the do_nopage() function will bug :(. The function
 * releases the mmap_sems for this ttm.
 */

extern void drm_bo_finish_unmap(struct drm_buffer_object *bo);

/*
 * Remap all vmas of this ttm using io_remap_pfn_range. We cannot 
 * fault these pfns in, because the first one will set the vma VM_PFNMAP
 * flag, which will make the next fault bug in do_nopage(). The function
 * releases the mmap_sems for this ttm.
 */

extern int drm_bo_remap_bound(struct drm_buffer_object *bo);


/*
 * Remap a vma for a bound ttm. Call with the ttm mutex held and
 * the relevant mmap_sem locked.
 */
extern int drm_bo_map_bound(struct vm_area_struct *vma);

#endif

/* fixme when functions are upstreamed - upstreamed for 2.6.23 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23))
#define DRM_IDR_COMPAT_FN
#endif
#ifdef DRM_IDR_COMPAT_FN
int idr_for_each(struct idr *idp,
		 int (*fn)(int id, void *p, void *data), void *data);
void idr_remove_all(struct idr *idp);
#endif


#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18))
void *idr_replace(struct idr *idp, void *ptr, int id);
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))
typedef _Bool                   bool;
#endif

#endif
