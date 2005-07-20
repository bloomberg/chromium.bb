/* ati_pcigart.h -- ATI PCI GART support -*- linux-c -*-
 * Created: Wed Dec 13 21:52:19 2000 by gareth@valinux.com
 */
/*-
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
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
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *   Gareth Hughes <gareth@valinux.com>
 *
 */

#include "drmP.h"

#define ATI_PCIGART_PAGE_SIZE		4096	/* PCI GART page size */
#define ATI_MAX_PCIGART_PAGES		8192	/* 32 MB aperture, 4K pages */
#define ATI_PCIGART_TABLE_SIZE		32768

int drm_ati_pcigart_init(drm_device_t *dev, unsigned long *addr,
			 dma_addr_t *bus_addr, int is_pcie)
{
	unsigned long pages;
	u32 *pci_gart = 0, page_base;
	int i, j;

	*addr = 0;
	*bus_addr = 0;

	if (dev->sg == NULL) {
		DRM_ERROR( "no scatter/gather memory!\n" );
		return 0;
	}

	dev->sg->dmah = drm_pci_alloc(dev, ATI_PCIGART_TABLE_SIZE, 0,
	    0xfffffffful);
	if (dev->sg->dmah == NULL) {
		DRM_ERROR("cannot allocate PCI GART table!\n");
		return 0;
	}

	*addr = (long)dev->sg->dmah->vaddr;
	*bus_addr = dev->sg->dmah->busaddr;
	pci_gart = (u32 *)dev->sg->dmah->vaddr;

	pages = DRM_MIN(dev->sg->pages, ATI_MAX_PCIGART_PAGES);

	bzero(pci_gart, ATI_PCIGART_TABLE_SIZE);

	KASSERT(PAGE_SIZE >= ATI_PCIGART_PAGE_SIZE, "page size too small");

	for ( i = 0 ; i < pages ; i++ ) {
		page_base = (u32) dev->sg->busaddr[i];

		for (j = 0; j < (PAGE_SIZE / ATI_PCIGART_PAGE_SIZE); j++) {
			if (is_pcie) {
				*pci_gart = (cpu_to_le32(page_base)>>8) | 0xc;
				DRM_DEBUG("PCIE: %d %08X %08X to %p\n", i,
				    page_base, (cpu_to_le32(page_base)>>8)|0xc,
				    pci_gart);
			} else
				*pci_gart = cpu_to_le32(page_base);
			pci_gart++;
			page_base += ATI_PCIGART_PAGE_SIZE;
		}
	}

	DRM_MEMORYBARRIER();

	return 1;
}

int drm_ati_pcigart_cleanup(drm_device_t *dev, unsigned long addr,
			    dma_addr_t bus_addr)
{
	if (dev->sg == NULL) {
		DRM_ERROR( "no scatter/gather memory!\n" );
		return 0;
	}

	drm_pci_free(dev, dev->sg->dmah);

	return 1;
}
