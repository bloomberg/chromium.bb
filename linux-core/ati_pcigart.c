/**
 * \file ati_pcigart.c
 * ATI PCI GART support
 *
 * \author Gareth Hughes <gareth@valinux.com>
 */

/*
 * Created: Wed Dec 13 21:52:19 2000 by gareth@valinux.com
 *
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
 */

#include "drmP.h"

# define ATI_PCIGART_PAGE_SIZE		4096	/**< PCI GART page size */

static __inline__ void insert_page_into_table(struct drm_ati_pcigart_info *info, u32 page_base, u32 *pci_gart)
{
	switch(info->gart_reg_if) {
	case DRM_ATI_GART_IGP:
		*pci_gart = cpu_to_le32((page_base) | 0xc);
		break;
	case DRM_ATI_GART_PCIE:
		*pci_gart = cpu_to_le32((page_base >> 8) | 0xc);
		break;
	default:
	case DRM_ATI_GART_PCI:
		*pci_gart = cpu_to_le32(page_base);
		break;
	}
}

static __inline__ u32 get_page_base_from_table(struct drm_ati_pcigart_info *info, u32 *pci_gart)
{
	u32 retval;
	switch(info->gart_reg_if) {
	case DRM_ATI_GART_IGP:
		retval = *pci_gart;
		retval &= ~0xc;
		break;
	case DRM_ATI_GART_PCIE:
		retval = *pci_gart;
		retval &= ~0xc;
		retval <<= 8;
		break;
	default:
	case DRM_ATI_GART_PCI:
		retval = *pci_gart;
		break;
	}
	return retval;
}



static void *drm_ati_alloc_pcigart_table(int order)
{
	unsigned long address;
	struct page *page;
	int i;

	DRM_DEBUG("%d order\n", order);

	address = __get_free_pages(GFP_KERNEL | __GFP_COMP,
				   order);
	if (address == 0UL) {
		return NULL;
	}

	page = virt_to_page(address);

	for (i = 0; i < order; i++, page++) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15)
		get_page(page);
#endif
		SetPageReserved(page);
	}

	DRM_DEBUG("returning 0x%08lx\n", address);
	return (void *)address;
}

static void drm_ati_free_pcigart_table(void *address, int order)
{
	struct page *page;
	int i;
	int num_pages = 1 << order;
	DRM_DEBUG("\n");

	page = virt_to_page((unsigned long)address);

	for (i = 0; i < num_pages; i++, page++) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15)
		__put_page(page);
#endif
		ClearPageReserved(page);
	}

	free_pages((unsigned long)address, order);
}

int drm_ati_pcigart_cleanup(struct drm_device *dev, struct drm_ati_pcigart_info *gart_info)
{
	struct drm_sg_mem *entry = dev->sg;
	unsigned long pages;
	int i;
	int order;
	int num_pages, max_pages;

	/* we need to support large memory configurations */
	if (!entry) {
		DRM_ERROR("no scatter/gather memory!\n");
		return 0;
	}

	order = drm_order((gart_info->table_size + (PAGE_SIZE-1)) / PAGE_SIZE);
	num_pages = 1 << order;

	if (gart_info->bus_addr) {
		if (gart_info->gart_table_location == DRM_ATI_GART_MAIN) {
			pci_unmap_single(dev->pdev, gart_info->bus_addr,
					 num_pages * PAGE_SIZE,
					 PCI_DMA_TODEVICE);
		}

		max_pages = (gart_info->table_size / sizeof(u32));
		pages = (entry->pages <= max_pages)
		  ? entry->pages : max_pages;

		for (i = 0; i < pages; i++) {
			if (!entry->busaddr[i])
				break;
			pci_unmap_single(dev->pdev, entry->busaddr[i],
					 PAGE_SIZE, PCI_DMA_TODEVICE);
		}

		if (gart_info->gart_table_location == DRM_ATI_GART_MAIN)
			gart_info->bus_addr = 0;
	}


	if (gart_info->gart_table_location == DRM_ATI_GART_MAIN
	    && gart_info->addr) {

		drm_ati_free_pcigart_table(gart_info->addr, order);
		gart_info->addr = NULL;
	}

	return 1;
}
EXPORT_SYMBOL(drm_ati_pcigart_cleanup);

int drm_ati_pcigart_init(struct drm_device *dev, struct drm_ati_pcigart_info *gart_info)
{
	struct drm_sg_mem *entry = dev->sg;
	void *address = NULL;
	unsigned long pages;
	u32 *pci_gart, page_base, bus_address = 0;
	int i, j, ret = 0;
	int order;
	int max_pages;
	int num_pages;

	if (!entry) {
		DRM_ERROR("no scatter/gather memory!\n");
		goto done;
	}

	if (gart_info->gart_table_location == DRM_ATI_GART_MAIN) {
		DRM_DEBUG("PCI: no table in VRAM: using normal RAM\n");

		order = drm_order((gart_info->table_size +
				   (PAGE_SIZE-1)) / PAGE_SIZE);
		num_pages = 1 << order;
		address = drm_ati_alloc_pcigart_table(order);
		if (!address) {
			DRM_ERROR("cannot allocate PCI GART page!\n");
			goto done;
		}

		if (!dev->pdev) {
			DRM_ERROR("PCI device unknown!\n");
			goto done;
		}

		bus_address = pci_map_single(dev->pdev, address,
					     num_pages * PAGE_SIZE,
					     PCI_DMA_TODEVICE);
		if (bus_address == 0) {
			DRM_ERROR("unable to map PCIGART pages!\n");
			order = drm_order((gart_info->table_size +
					   (PAGE_SIZE-1)) / PAGE_SIZE);
			drm_ati_free_pcigart_table(address, order);
			address = NULL;
			goto done;
		}
	} else {
		address = gart_info->addr;
		bus_address = gart_info->bus_addr;
		DRM_DEBUG("PCI: Gart Table: VRAM %08X mapped at %08lX\n",
			  bus_address, (unsigned long)address);
	}

	pci_gart = (u32 *) address;

	max_pages = (gart_info->table_size / sizeof(u32));
	pages = (entry->pages <= max_pages)
	    ? entry->pages : max_pages;

	memset(pci_gart, 0, max_pages * sizeof(u32));

	for (i = 0; i < pages; i++) {
		/* we need to support large memory configurations */
		entry->busaddr[i] = pci_map_single(dev->pdev,
						   page_address(entry->
								pagelist[i]),
						   PAGE_SIZE, PCI_DMA_TODEVICE);
		if (entry->busaddr[i] == 0) {
			DRM_ERROR("unable to map PCIGART pages!\n");
			drm_ati_pcigart_cleanup(dev, gart_info);
			address = NULL;
			bus_address = 0;
			goto done;
		}
		page_base = (u32) entry->busaddr[i];

		for (j = 0; j < (PAGE_SIZE / ATI_PCIGART_PAGE_SIZE); j++) {
			insert_page_into_table(gart_info, page_base, pci_gart);
			pci_gart++;
			page_base += ATI_PCIGART_PAGE_SIZE;
		}
	}

	ret = 1;

#if defined(__i386__) || defined(__x86_64__)
	wbinvd();
#else
	mb();
#endif

      done:
	gart_info->addr = address;
	gart_info->bus_addr = bus_address;
	return ret;
}
EXPORT_SYMBOL(drm_ati_pcigart_init);

static int ati_pcigart_needs_unbind_cache_adjust(struct drm_ttm_backend *backend)
{
	return ((backend->flags & DRM_BE_FLAG_BOUND_CACHED) ? 0 : 1);
}

static int ati_pcigart_populate(struct drm_ttm_backend *backend,
				unsigned long num_pages,
				struct page **pages)
{
	ati_pcigart_ttm_backend_t *atipci_be =
		container_of(backend, ati_pcigart_ttm_backend_t, backend);

	DRM_ERROR("%ld\n", num_pages);
	atipci_be->pages = pages;
	atipci_be->num_pages = num_pages;
	atipci_be->populated = 1;
	return 0;
}

static int ati_pcigart_bind_ttm(struct drm_ttm_backend *backend,
				struct drm_bo_mem_reg *bo_mem)
{
	ati_pcigart_ttm_backend_t *atipci_be =
		container_of(backend, ati_pcigart_ttm_backend_t, backend);
        off_t j;
	int i;
	struct drm_ati_pcigart_info *info = atipci_be->gart_info;
	u32 *pci_gart;
	u32 page_base;
	unsigned long offset = bo_mem->mm_node->start;
	pci_gart = info->addr;

	DRM_ERROR("Offset is %08lX\n", bo_mem->mm_node->start);
        j = offset;
        while (j < (offset + atipci_be->num_pages)) {
		if (get_page_base_from_table(info, pci_gart+j))
			return -EBUSY;
                j++;
        }

        for (i = 0, j = offset; i < atipci_be->num_pages; i++, j++) {
		struct page *cur_page = atipci_be->pages[i];
                /* write value */
		page_base = page_to_phys(cur_page);
		insert_page_into_table(info, page_base, pci_gart + j);
        }

#if defined(__i386__) || defined(__x86_64__)
	wbinvd();
#else
	mb();
#endif

	atipci_be->gart_flush_fn(atipci_be->dev);

	atipci_be->bound = 1;
	atipci_be->offset = offset;
        /* need to traverse table and add entries */
	DRM_DEBUG("\n");
	return 0;
}

static int ati_pcigart_unbind_ttm(struct drm_ttm_backend *backend)
{
	ati_pcigart_ttm_backend_t *atipci_be =
		container_of(backend, ati_pcigart_ttm_backend_t, backend);
	struct drm_ati_pcigart_info *info = atipci_be->gart_info;	
	unsigned long offset = atipci_be->offset;
	int i;
	off_t j;
	u32 *pci_gart = info->addr;

	DRM_DEBUG("\n");

	if (atipci_be->bound != 1)
		return -EINVAL;

	for (i = 0, j = offset; i < atipci_be->num_pages; i++, j++) {
		*(pci_gart + j) = 0;
	}
	atipci_be->gart_flush_fn(atipci_be->dev);
	atipci_be->bound = 0;
	atipci_be->offset = 0;
	return 0;
}

static void ati_pcigart_clear_ttm(struct drm_ttm_backend *backend)
{
	ati_pcigart_ttm_backend_t *atipci_be =
		container_of(backend, ati_pcigart_ttm_backend_t, backend);

	DRM_DEBUG("\n");	
	if (atipci_be->pages) {
		backend->func->unbind(backend);
		atipci_be->pages = NULL;

	}
	atipci_be->num_pages = 0;
}

static void ati_pcigart_destroy_ttm(struct drm_ttm_backend *backend)
{
	ati_pcigart_ttm_backend_t *atipci_be;
	if (backend) {
		DRM_DEBUG("\n");
		atipci_be = container_of(backend, ati_pcigart_ttm_backend_t, backend);
		if (atipci_be) {
			if (atipci_be->pages) {
				backend->func->clear(backend);
			}
			drm_ctl_free(atipci_be, sizeof(*atipci_be), DRM_MEM_TTM);
		}
	}
}

static struct drm_ttm_backend_func ati_pcigart_ttm_backend = 
{
	.needs_ub_cache_adjust = ati_pcigart_needs_unbind_cache_adjust,
	.populate = ati_pcigart_populate,
	.clear = ati_pcigart_clear_ttm,
	.bind = ati_pcigart_bind_ttm,
	.unbind = ati_pcigart_unbind_ttm,
	.destroy =  ati_pcigart_destroy_ttm,
};

struct drm_ttm_backend *ati_pcigart_init_ttm(struct drm_device *dev, struct drm_ati_pcigart_info *info, void (*gart_flush_fn)(struct drm_device *dev))
{
	ati_pcigart_ttm_backend_t *atipci_be;

	atipci_be = drm_ctl_calloc(1, sizeof (*atipci_be), DRM_MEM_TTM);
	if (!atipci_be)
		return NULL;
	
	atipci_be->populated = 0;
	atipci_be->backend.func = &ati_pcigart_ttm_backend;
	atipci_be->gart_info = info;
	atipci_be->gart_flush_fn = gart_flush_fn;
	atipci_be->dev = dev;

	return &atipci_be->backend;
}
EXPORT_SYMBOL(ati_pcigart_init_ttm);
