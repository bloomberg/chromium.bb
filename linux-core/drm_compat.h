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

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27))
#define current_euid() (current->euid)
#else
#include <linux/cred.h>
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

#ifndef list_for_each_entry_safe_reverse
#define list_for_each_entry_safe_reverse(pos, n, head, member)          \
        for (pos = list_entry((head)->prev, typeof(*pos), member),      \
                n = list_entry(pos->member.prev, typeof(*pos), member); \
             &pos->member != (head);                                    \
             pos = n, n = list_entry(n->member.prev, typeof(*n), member))
#endif

#include <linux/mm.h>
#include <asm/page.h>

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

/* fixme when functions are upstreamed - upstreamed for 2.6.23 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23))
#define DRM_IDR_COMPAT_FN
#define DRM_NO_FAULT
extern unsigned long drm_bo_vm_nopfn(struct vm_area_struct *vma,
				     unsigned long address);
#endif
#ifdef DRM_IDR_COMPAT_FN
int idr_for_each(struct idr *idp,
		 int (*fn)(int id, void *p, void *data), void *data);
void idr_remove_all(struct idr *idp);
#endif


#if !defined(flush_agp_mappings)
#define flush_agp_mappings() do {} while(0)
#endif

#ifndef DMA_BIT_MASK
#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : (1ULL<<(n)) - 1)
#endif

#ifndef VM_CAN_NONLINEAR
#define DRM_VM_NOPAGE 1
#endif

#ifdef DRM_VM_NOPAGE

extern struct page *drm_vm_nopage(struct vm_area_struct *vma,
				  unsigned long address, int *type);

extern struct page *drm_vm_shm_nopage(struct vm_area_struct *vma,
				      unsigned long address, int *type);

extern struct page *drm_vm_dma_nopage(struct vm_area_struct *vma,
				      unsigned long address, int *type);

extern struct page *drm_vm_sg_nopage(struct vm_area_struct *vma,
				     unsigned long address, int *type);
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
#define drm_core_ioremap_wc drm_core_ioremap
#endif

#ifndef OS_HAS_GEM
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,27))
#define OS_HAS_GEM 1
#else
#define OS_HAS_GEM 0
#endif
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27))
#define set_page_locked SetPageLocked
#elif (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,27))
/*
 * The kernel provides __set_page_locked, which uses the non-atomic
 * __set_bit function. Let's use the atomic set_bit just in case.
 */
static inline void set_page_locked(struct page *page)
{
	set_bit(PG_locked, &page->flags);
}
#endif

#endif
