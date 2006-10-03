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
