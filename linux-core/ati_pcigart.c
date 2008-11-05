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
# define ATI_PCIGART_PAGE_MASK		(~(ATI_PCIGART_PAGE_SIZE-1))

#define ATI_PCIE_WRITE 0x4
#define ATI_PCIE_READ 0x8

static __inline__ void gart_insert_page_into_table(struct drm_ati_pcigart_info *gart_info, dma_addr_t addr, volatile u32 *pci_gart)
{
	u32 page_base;

	page_base = (u32)addr & ATI_PCIGART_PAGE_MASK;
	switch(gart_info->gart_reg_if) {
	case DRM_ATI_GART_IGP:
		page_base |= (upper_32_bits(addr) & 0xff) << 4;
		page_base |= 0xc;
		break;
	case DRM_ATI_GART_PCIE:
		page_base >>= 8;
		page_base |= (upper_32_bits(addr) & 0xff) << 24;
		page_base |= ATI_PCIE_READ | ATI_PCIE_WRITE;
		break;
	default:
	case DRM_ATI_GART_PCI:
		break;
	}
	*pci_gart = cpu_to_le32(page_base);
}

static __inline__ dma_addr_t gart_get_page_from_table(struct drm_ati_pcigart_info *gart_info, volatile u32 *pci_gart)
{
	dma_addr_t retval;
	switch(gart_info->gart_reg_if) {
	case DRM_ATI_GART_IGP:
		retval = (*pci_gart & ATI_PCIGART_PAGE_MASK);
		retval += (((*pci_gart & 0xf0) >> 4) << 16) << 16;
		break;
	case DRM_ATI_GART_PCIE:
		retval = (*pci_gart & ~0xc);
		retval <<= 8;
		break;
	case DRM_ATI_GART_PCI:
		retval = *pci_gart;
		break;
	}
	
	return retval;
}

int drm_ati_alloc_pcigart_table(struct drm_device *dev,
				struct drm_ati_pcigart_info *gart_info)
{
	gart_info->table_handle = drm_pci_alloc(dev, gart_info->table_size,
						PAGE_SIZE,
						gart_info->table_mask);
	if (gart_info->table_handle == NULL)
		return -ENOMEM;

#ifdef CONFIG_X86
	/* IGPs only exist on x86 in any case */
	if (gart_info->gart_reg_if == DRM_ATI_GART_IGP)
		set_memory_uc((unsigned long)gart_info->table_handle->vaddr, gart_info->table_size >> PAGE_SHIFT);
#endif

	memset(gart_info->table_handle->vaddr, 0, gart_info->table_size);
	return 0;
}
EXPORT_SYMBOL(drm_ati_alloc_pcigart_table);

static void drm_ati_free_pcigart_table(struct drm_device *dev,
				       struct drm_ati_pcigart_info *gart_info)
{
#ifdef CONFIG_X86
	/* IGPs only exist on x86 in any case */
	if (gart_info->gart_reg_if == DRM_ATI_GART_IGP)
		set_memory_wb((unsigned long)gart_info->table_handle->vaddr, gart_info->table_size >> PAGE_SHIFT);
#endif
	drm_pci_free(dev, gart_info->table_handle);
	gart_info->table_handle = NULL;
}

int drm_ati_pcigart_cleanup(struct drm_device *dev, struct drm_ati_pcigart_info *gart_info)
{
	struct drm_sg_mem *entry = dev->sg;
	unsigned long pages;
	int i;
	int max_pages;

	/* we need to support large memory configurations */
	if (!entry) {
		return 0;
	}

	if (gart_info->bus_addr) {

		max_pages = (gart_info->table_size / sizeof(u32));
		pages = (entry->pages <= max_pages)
		  ? entry->pages : max_pages;

		for (i = 0; i < pages; i++) {
			if (!entry->busaddr[i])
				break;
			pci_unmap_page(dev->pdev, entry->busaddr[i],
					 PAGE_SIZE, PCI_DMA_TODEVICE);
		}

		if (gart_info->gart_table_location == DRM_ATI_GART_MAIN)
			gart_info->bus_addr = 0;
	}


	if (gart_info->gart_table_location == DRM_ATI_GART_MAIN
	    && gart_info->table_handle) {

		drm_ati_free_pcigart_table(dev, gart_info);
	}

	return 1;
}
EXPORT_SYMBOL(drm_ati_pcigart_cleanup);

int drm_ati_pcigart_init(struct drm_device *dev, struct drm_ati_pcigart_info *gart_info)
{
	struct drm_sg_mem *entry = dev->sg;
	void *address = NULL;
	unsigned long pages;
	u32 *pci_gart;
	dma_addr_t bus_address = 0;
	int i, j, ret = 0;
	int max_pages;
	dma_addr_t entry_addr;


	if (gart_info->gart_table_location == DRM_ATI_GART_MAIN && gart_info->table_handle == NULL) {
		DRM_DEBUG("PCI: no table in VRAM: using normal RAM\n");

		ret = drm_ati_alloc_pcigart_table(dev, gart_info);
		if (ret) {
			DRM_ERROR("cannot allocate PCI GART page!\n");
			goto done;
		}
	}

	if (gart_info->gart_table_location == DRM_ATI_GART_MAIN) {
		address = gart_info->table_handle->vaddr;
		bus_address = gart_info->table_handle->busaddr;
	} else {
		address = gart_info->addr;
		bus_address = gart_info->bus_addr;
	}

	if (!entry) {
		DRM_ERROR("no scatter/gather memory!\n");
		goto done;
	}

	pci_gart = (u32 *) address;

	max_pages = (gart_info->table_size / sizeof(u32));
	pages = (entry->pages <= max_pages)
	    ? entry->pages : max_pages;

	for (i = 0; i < pages; i++) {
		/* we need to support large memory configurations */
		entry->busaddr[i] = pci_map_page(dev->pdev, entry->pagelist[i],
						 0, PAGE_SIZE, PCI_DMA_TODEVICE);
		if (entry->busaddr[i] == 0) {
			DRM_ERROR("unable to map PCIGART pages!\n");
			drm_ati_pcigart_cleanup(dev, gart_info);
			address = NULL;
			bus_address = 0;
			goto done;
		}

		entry_addr = entry->busaddr[i];
		for (j = 0; j < (PAGE_SIZE / ATI_PCIGART_PAGE_SIZE); j++) {
			gart_insert_page_into_table(gart_info, entry_addr, pci_gart);
			pci_gart++;
			entry_addr += ATI_PCIGART_PAGE_SIZE;
		}
	}

	ret = 1;

	mb();

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
				struct page **pages,
				struct page *dummy_read_page)
{
	struct ati_pcigart_ttm_backend *atipci_be =
		container_of(backend, struct ati_pcigart_ttm_backend, backend);

	atipci_be->pages = pages;
	atipci_be->num_pages = num_pages;
	atipci_be->populated = 1;
	return 0;
}

static int ati_pcigart_bind_ttm(struct drm_ttm_backend *backend,
				struct drm_bo_mem_reg *bo_mem)
{
	struct ati_pcigart_ttm_backend *atipci_be =
		container_of(backend, struct ati_pcigart_ttm_backend, backend);
        off_t j;
	int i;
	struct drm_ati_pcigart_info *info = atipci_be->gart_info;
	u32 *pci_gart;
	dma_addr_t offset = bo_mem->mm_node->start;
	dma_addr_t page_base;

	pci_gart = info->addr;

        j = offset;
        while (j < (offset + atipci_be->num_pages)) {
		if (gart_get_page_from_table(info, pci_gart + j))
			return -EBUSY;
                j++;
        }

        for (i = 0, j = offset; i < atipci_be->num_pages; i++, j++) {
		struct page *cur_page = atipci_be->pages[i];
                /* write value */
		page_base = page_to_phys(cur_page);
		gart_insert_page_into_table(info, page_base, pci_gart + j);
        }

	mb();

	atipci_be->gart_flush_fn(atipci_be->dev);

	atipci_be->bound = 1;
	atipci_be->offset = offset;
        /* need to traverse table and add entries */
	DRM_DEBUG("\n");
	return 0;
}

static int ati_pcigart_unbind_ttm(struct drm_ttm_backend *backend)
{
	struct ati_pcigart_ttm_backend *atipci_be =
		container_of(backend, struct ati_pcigart_ttm_backend, backend);
	struct drm_ati_pcigart_info *info = atipci_be->gart_info;	
	unsigned long offset = atipci_be->offset;
	int i;
	off_t j;
	u32 *pci_gart = info->addr;

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
	struct ati_pcigart_ttm_backend *atipci_be =
		container_of(backend, struct ati_pcigart_ttm_backend, backend);

	DRM_DEBUG("\n");	
	if (atipci_be->pages) {
		backend->func->unbind(backend);
		atipci_be->pages = NULL;

	}
	atipci_be->num_pages = 0;
}

static void ati_pcigart_destroy_ttm(struct drm_ttm_backend *backend)
{
	struct ati_pcigart_ttm_backend *atipci_be;
	if (backend) {
		DRM_DEBUG("\n");
		atipci_be = container_of(backend, struct ati_pcigart_ttm_backend, backend);
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
	struct ati_pcigart_ttm_backend *atipci_be;

	atipci_be = drm_ctl_calloc(1, sizeof (*atipci_be), DRM_MEM_TTM);
	if (!atipci_be)
		return NULL;
	
	atipci_be->populated = 0;
	atipci_be->backend.func = &ati_pcigart_ttm_backend;
//	atipci_be->backend.mem_type = DRM_BO_MEM_TT;
	atipci_be->gart_info = info;
	atipci_be->gart_flush_fn = gart_flush_fn;
	atipci_be->dev = dev;

	return &atipci_be->backend;
}
EXPORT_SYMBOL(ati_pcigart_init_ttm);
