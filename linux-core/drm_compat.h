/**
 * \file drmP.h 
 * Private header for Direct Rendering Manager
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

/***********************************************************************/
/** \name Backward compatibility section */
/*@{*/

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
static inline struct page * vmalloc_to_page(void * vmalloc_addr)
{
	unsigned long addr = (unsigned long) vmalloc_addr;
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

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
static inline unsigned iminor(struct inode *inode)
{
        return MINOR(inode->i_rdev);
}

#define old_encode_dev(x) (x)

struct class_simple;
struct device;

#define pci_dev_put(x) do {} while (0)
#define pci_get_subsys pci_find_subsys

#define class_simple_device_add(...) do {} while (0)

static inline void class_simple_device_remove(dev_t dev){};

static inline void class_simple_destroy(struct class_simple *cs){};

static inline struct class_simple *class_simple_create(struct module *owner, char *name) { return (struct class_simple *)owner; }

#ifndef pci_pretty_name
#define pci_pretty_name(x) x->name
#endif

#endif

#ifndef __user
#define __user
#endif

#ifndef __put_page
#define __put_page(p)           atomic_dec(&(p)->count)
#endif

#ifndef REMAP_PAGE_RANGE_5_ARGS
#define DRM_RPR_ARG(vma)
#else
#define DRM_RPR_ARG(vma) vma,
#endif

#define VM_OFFSET(vma) ((vma)->vm_pgoff << PAGE_SHIFT)

/*@}*/

#endif
