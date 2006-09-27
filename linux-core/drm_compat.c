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
#include <asm/pgtable.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>

#ifdef MODULE
void pgd_clear_bad(pgd_t * pgd)
{
	pgd_ERROR(*pgd);
	pgd_clear(pgd);
}

void pud_clear_bad(pud_t * pud)
{
	pud_ERROR(*pud);
	pud_clear(pud);
}

void pmd_clear_bad(pmd_t * pmd)
{
	pmd_ERROR(*pmd);
	pmd_clear(pmd);
}
#endif

static inline void change_pte_range(struct mm_struct *mm, pmd_t * pmd,
				    unsigned long addr, unsigned long end)
{
	pte_t *pte;
	struct page *page;
	unsigned long pfn;

	pte = pte_offset_map(pmd, addr);
	do {
		if (pte_present(*pte)) {
			pte_t ptent;
			pfn = pte_pfn(*pte);
			ptent = *pte;
			ptep_get_and_clear(mm, addr, pte);
			if (pfn_valid(pfn)) {
				page = pfn_to_page(pfn);
				if (atomic_add_negative(-1, &page->_mapcount)) {
					if (page_test_and_clear_dirty(page))
						set_page_dirty(page);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
					dec_zone_page_state(page, NR_FILE_MAPPED);
#else
					dec_page_state(nr_mapped);
#endif			
				}

				put_page(page);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15)
				dec_mm_counter(mm, file_rss);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,12)
				dec_mm_counter(mm, rss);
#else
				--mm->rss;
#endif
			}
		}
	} while (pte++, addr += PAGE_SIZE, addr != end);
	pte_unmap(pte - 1);
}

static inline void change_pmd_range(struct mm_struct *mm, pud_t * pud,
				    unsigned long addr, unsigned long end)
{
	pmd_t *pmd;
	unsigned long next;

	pmd = pmd_offset(pud, addr);
	do {
		next = pmd_addr_end(addr, end);
		if (pmd_none_or_clear_bad(pmd))
			continue;
		change_pte_range(mm, pmd, addr, next);
	} while (pmd++, addr = next, addr != end);
}

static inline void change_pud_range(struct mm_struct *mm, pgd_t * pgd,
				    unsigned long addr, unsigned long end)
{
	pud_t *pud;
	unsigned long next;

	pud = pud_offset(pgd, addr);
	do {
		next = pud_addr_end(addr, end);
		if (pud_none_or_clear_bad(pud))
			continue;
		change_pmd_range(mm, pud, addr, next);
	} while (pud++, addr = next, addr != end);
}

/*
 * This function should be called with all relevant spinlocks held.
 */

#if 1
void drm_clear_vma(struct vm_area_struct *vma,
		   unsigned long addr, unsigned long end)
{
	struct mm_struct *mm = vma->vm_mm;
	pgd_t *pgd;
	unsigned long next;
#if defined(flush_tlb_mm) || !defined(MODULE)
	unsigned long start = addr;
#endif
	BUG_ON(addr >= end);
	pgd = pgd_offset(mm, addr);
	flush_cache_range(vma, addr, end);
	do {
		next = pgd_addr_end(addr, end);
		if (pgd_none_or_clear_bad(pgd))
			continue;
		change_pud_range(mm, pgd, addr, next);
	} while (pgd++, addr = next, addr != end);
#if defined(flush_tlb_mm) || !defined(MODULE)
	flush_tlb_range(vma, addr, end);
#endif
}
#else

void drm_clear_vma(struct vm_area_struct *vma,
		   unsigned long addr, unsigned long end)
{
	struct mm_struct *mm = vma->vm_mm;

	spin_unlock(&mm->page_table_lock);
	(void) zap_page_range(vma, addr, end - addr, NULL);
	spin_lock(&mm->page_table_lock);
}
#endif

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
