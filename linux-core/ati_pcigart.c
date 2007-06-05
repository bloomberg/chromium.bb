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

static void *drm_ati_alloc_pcigart_table(int order)
{
	unsigned long address;
	struct page *page;
	int i;

	DRM_DEBUG("%s: alloc %d order\n", __FUNCTION__, order);

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

	DRM_DEBUG("%s: returning 0x%08lx\n", __FUNCTION__, address);
	return (void *)address;
}

static void drm_ati_free_pcigart_table(void *address, int order)
{
	struct page *page;
	int i;
	int num_pages = 1 << order;
	DRM_DEBUG("%s\n", __FUNCTION__);

	page = virt_to_page((unsigned long)address);

	for (i = 0; i < num_pages; i++, page++) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15)
		__put_page(page);
#endif
		ClearPageReserved(page);
	}

	free_pages((unsigned long)address, order);
}

int drm_ati_pcigart_cleanup(drm_device_t *dev, drm_ati_pcigart_info *gart_info)
{
	drm_sg_mem_t *entry = dev->sg;
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

int drm_ati_pcigart_init(drm_device_t *dev, drm_ati_pcigart_info *gart_info)
{
	drm_sg_mem_t *entry = dev->sg;
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
			switch(gart_info->gart_reg_if) {
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

static int ati_pcigart_needs_unbind_cache_adjust(drm_ttm_backend_t *backend)
{
	return ((backend->flags & DRM_BE_FLAG_BOUND_CACHED) ? 0 : 1);
}

void ati_pcigart_alloc_page_array(size_t size, struct ati_pcigart_memory *mem)
{
        mem->memory = NULL;
        mem->flags = 0;

        if (size <= 2*PAGE_SIZE)
                mem->memory = kmalloc(size, GFP_KERNEL | __GFP_NORETRY);
        if (mem->memory == NULL) {
                mem->memory = vmalloc(size);
                mem->flags |= ATI_PCIGART_FLAG_VMALLOC;
        }
}

static int ati_pcigart_populate(drm_ttm_backend_t *backend,
				unsigned long num_pages,
				struct page **pages)
{
	ati_pcigart_ttm_priv *atipci_priv = (ati_pcigart_ttm_priv *)backend->private;	
	struct page **cur_page, **last_page = pages + num_pages;
	struct ati_pcigart_memory *mem;
        unsigned long alloc_size = num_pages * sizeof(struct page *);

	DRM_DEBUG("%d\n", num_pages);
	if (drm_alloc_memctl(num_pages * sizeof(void *)))
		return -1;

	mem = drm_alloc(sizeof(struct ati_pcigart_memory), DRM_MEM_MAPPINGS);
	if (!mem) {
		drm_free_memctl(num_pages * sizeof(void *));
		return -1;
	}

        ati_pcigart_alloc_page_array(alloc_size, mem);
        mem->page_count = 0;
        for (cur_page = pages; cur_page < last_page; ++cur_page) {
                mem->memory[mem->page_count++] = page_to_phys(*cur_page);
        }
	atipci_priv->mem = mem;
	return 0;
}

static int ati_pcigart_bind_ttm(drm_ttm_backend_t *backend,
				unsigned long offset,
				int cached)
{
	ati_pcigart_ttm_priv *atipci_priv = (ati_pcigart_ttm_priv *)backend->private;
        struct ati_pcigart_memory *mem = atipci_priv->mem;
        off_t j;

        j = offset;
        while (j < (pg_start + mem->page_count)) {
                j++;
        }

        for (i = 0, j = offset; i < mem->page_count; i++, j++) {
                /* write value */
        }

        /* need to traverse table and add entries */
	DRM_DEBUG("\n");
	return -1;
}

static int ati_pcigart_unbind_ttm(drm_ttm_backend_t *backend)
{
	ati_pcigart_ttm_priv *atipci_priv = (ati_pcigart_ttm_priv *)backend->private;
	
	DRM_DEBUG("\n");
	return -1;
}

static void ati_pcigart_clear_ttm(drm_ttm_backend_t *backend)
{
	ati_pcigart_ttm_priv *atipci_priv = (ati_pcigart_ttm_priv *)backend->private;
	struct ati_pcigart_memory *mem = atipci_priv->mem;

	DRM_DEBUG("\n");	
	if (mem) {
		unsigned long num_pages = mem->page_count;
		backend->unbind(backend);
		/* free test */
		drm_free(mem, sizeof(struct ati_pcigart_memory), DRM_MEM_MAPPINGS);
		drm_free_memctl(num_pages * sizeof(void *));
	}
	atipci_priv->mem = NULL;
}

static void ati_pcigart_destroy_ttm(drm_ttm_backend_t *backend)
{
	ati_pcigart_ttm_priv *atipci_priv;

	if (backend) {
		DRM_DEBUG("\n");
		atipci_priv = (ati_pcigart_ttm_priv *)backend->private;
		if (atipci_priv) {
			if (atipci_priv->mem) {
				backend->clear(backend);
			}
			drm_ctl_free(atipci_priv, sizeof(*atipci_priv), DRM_MEM_MAPPINGS);
			backend->private = NULL;
		}
		if (backend->flags & DRM_BE_FLAG_NEEDS_FREE) {
			drm_ctl_free(backend, sizeof(*backend), DRM_MEM_MAPPINGS);
		}
	}
}


drm_ttm_backend_t *ati_pcigart_init_ttm(struct drm_device *dev,
					drm_ttm_backend_t *backend)
{
	drm_ttm_backend_t *atipci_be;
	ati_pcigart_ttm_priv *atipci_priv;

	atipci_be = (backend != NULL) ? backend : 
		drm_ctl_calloc(1, sizeof (*atipci_be), DRM_MEM_MAPPINGS);

	if (!atipci_be)
		return NULL;

	atipci_priv = drm_ctl_calloc(1, sizeof(*atipci_priv), DRM_MEM_MAPPINGS);
	if (!atipci_priv) {
		drm_ctl_free(atipci_be, sizeof(*atipci_be), DRM_MEM_MAPPINGS);
		return NULL;
	}

	atipci_priv->populated = FALSE;
	atipci_be->needs_ub_cache_adjust = ati_pcigart_needs_unbind_cache_adjust;
	atipci_be->populate = ati_pcigart_populate;
	atipci_be->clear = ati_pcigart_clear_ttm;
	atipci_be->bind = ati_pcigart_bind_ttm;
	atipci_be->unbind = ati_pcigart_unbind_ttm;
	atipci_be->destroy = ati_pcigart_destroy_ttm;
	
	DRM_FLAG_MASKED(atipci_be->flags, (backend == NULL) ? DRM_BE_FLAG_NEEDS_FREE : 0, DRM_BE_FLAG_NEEDS_FREE);
	atipci_be->drm_map_type = _DRM_SCATTER_GATHER;
	return atipci_be;
}
EXPORT_SYMBOL(ati_pcigart_init_ttm);
