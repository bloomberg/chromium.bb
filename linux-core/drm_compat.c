/**************************************************************************
 * 
 * This kernel module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 **************************************************************************/
/*
 * This code provides access to unexported mm kernel features. It is necessary
 * to use the new DRM memory manager code with kernels that don't support it
 * directly.
 *
 * Authors: Thomas Hellstrom <thomas-at-tungstengraphics-dot-com>
 *          Linux kernel mm subsystem authors. 
 *          (Most code taken from there).
 */

#include "drmP.h"

#if defined(CONFIG_X86) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15))
int drm_map_page_into_agp(struct page *page)
{
        int i;
        i = change_page_attr(page, 1, PAGE_KERNEL_NOCACHE);
        /* Caller's responsibility to call global_flush_tlb() for
         * performance reasons */
        return i;
}

int drm_unmap_page_from_agp(struct page *page)
{
        int i;
        i = change_page_attr(page, 1, PAGE_KERNEL);
        /* Caller's responsibility to call global_flush_tlb() for
         * performance reasons */
        return i;
}
#endif


pgprot_t vm_get_page_prot(unsigned long vm_flags)
{
#ifdef MODULE
	static pgprot_t drm_protection_map[16] = {
		__P000, __P001, __P010, __P011, __P100, __P101, __P110, __P111,
		__S000, __S001, __S010, __S011, __S100, __S101, __S110, __S111
	};

	return drm_protection_map[vm_flags & 0x0F];
#else
	extern pgprot_t protection_map[];
	return protection_map[vm_flags & 0x0F];
#endif
};

int drm_pte_is_clear(struct vm_area_struct *vma,
		     unsigned long addr)
{
	struct mm_struct *mm = vma->vm_mm;
	int ret = 1;
	pte_t *pte;
	pmd_t *pmd;
	pud_t *pud;
	pgd_t *pgd;
	

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15))
	spin_lock(&mm->page_table_lock);
#else
	spinlock_t ptl;
#endif
	
	pgd = pgd_offset(mm, addr);
	if (pgd_none(*pgd))
		goto unlock;
	pud = pud_offset(pgd, addr);
        if (pud_none(*pud))
		goto unlock;
	pmd = pmd_offset(pud, addr);
	if (pmd_none(*pmd))
		goto unlock;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15))
	pte = pte_offset_map(pmd, addr);
#else 
	pte = pte_offset_map_lock(mm, pmd, addr, &ptl);
#endif
	if (!pte)
		goto unlock;
	ret = pte_none(*pte);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15))
	pte_unmap(pte);
 unlock:	
	spin_unlock(&mm->page_table_lock);
#else
	pte_unmap_unlock(pte, ptl);
 unlock:
#endif
	return ret;
}
	

static struct {
	spinlock_t lock;
	struct page *dummy_page;
	atomic_t present;
} drm_np_retry = 
{SPIN_LOCK_UNLOCKED, NOPAGE_OOM, ATOMIC_INIT(0)};

struct page * get_nopage_retry(void)
{
	if (atomic_read(&drm_np_retry.present) == 0) {
		struct page *page = alloc_page(GFP_KERNEL);
		if (!page)
			return NOPAGE_OOM;
		spin_lock(&drm_np_retry.lock);
		drm_np_retry.dummy_page = page;
		atomic_set(&drm_np_retry.present,1);
		spin_unlock(&drm_np_retry.lock);
	}
	get_page(drm_np_retry.dummy_page);
	return drm_np_retry.dummy_page;
}

void free_nopage_retry(void)
{
	if (atomic_read(&drm_np_retry.present) == 1) {
		spin_lock(&drm_np_retry.lock);
		__free_page(drm_np_retry.dummy_page);
		drm_np_retry.dummy_page = NULL;
		atomic_set(&drm_np_retry.present, 0);
		spin_unlock(&drm_np_retry.lock);
	}
}
