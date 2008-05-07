/**
 * \file drm_vm.c
 * Memory mapping for DRM
 *
 * \author Rickard E. (Rik) Faith <faith@valinux.com>
 * \author Gareth Hughes <gareth@valinux.com>
 */

/*
 * Created: Mon Jan  4 08:58:31 1999 by faith@valinux.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
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
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "drmP.h"

#ifdef DRM_VM_NOPAGE
/**
 * \c nopage method for AGP virtual memory.
 *
 * \param vma virtual memory area.
 * \param address access address.
 * \return pointer to the page structure.
 *
 * Find the right map and if it's AGP memory find the real physical page to
 * map, get the page, increment the use count and return it.
 */
#if __OS_HAS_AGP
static __inline__ struct page *drm_do_vm_nopage(struct vm_area_struct *vma,
						unsigned long address)
{
	struct drm_file *priv = vma->vm_file->private_data;
	struct drm_device *dev = priv->minor->dev;
	struct drm_map *map = NULL;
	struct drm_map_list *r_list;
	struct drm_hash_item *hash;

	/*
	 * Find the right map
	 */
	if (!drm_core_has_AGP(dev))
		goto vm_nopage_error;

	if (!dev->agp || !dev->agp->cant_use_aperture)
		goto vm_nopage_error;

	if (drm_ht_find_item(&dev->map_hash, vma->vm_pgoff, &hash))
		goto vm_nopage_error;

	r_list = drm_hash_entry(hash, struct drm_map_list, hash);
	map = r_list->map;

	if (map && map->type == _DRM_AGP) {
		unsigned long offset = address - vma->vm_start;
		unsigned long baddr = map->offset + offset;
		struct drm_agp_mem *agpmem;
		struct page *page;

#ifdef __alpha__
		/*
		 * Adjust to a bus-relative address
		 */
		baddr -= dev->hose->mem_space->start;
#endif

		/*
		 * It's AGP memory - find the real physical page to map
		 */
		list_for_each_entry(agpmem, &dev->agp->memory, head) {
			if (agpmem->bound <= baddr &&
			    agpmem->bound + agpmem->pages * PAGE_SIZE > baddr)
				break;
		}

		if (!agpmem)
			goto vm_nopage_error;

		/*
		 * Get the page, inc the use count, and return it
		 */
		offset = (baddr - agpmem->bound) >> PAGE_SHIFT;
		page = virt_to_page(__va(agpmem->memory->memory[offset]));
		get_page(page);

#if 0
		/* page_count() not defined everywhere */
		DRM_DEBUG
		    ("baddr = 0x%lx page = 0x%p, offset = 0x%lx, count=%d\n",
		     baddr, __va(agpmem->memory->memory[offset]), offset,
		     page_count(page));
#endif

		return page;
	}
      vm_nopage_error:
	return NOPAGE_SIGBUS;	/* Disallow mremap */
}
#else				/* __OS_HAS_AGP */
static __inline__ struct page *drm_do_vm_nopage(struct vm_area_struct *vma,
						unsigned long address)
{
	return NOPAGE_SIGBUS;
}
#endif				/* __OS_HAS_AGP */

/**
 * \c nopage method for shared virtual memory.
 *
 * \param vma virtual memory area.
 * \param address access address.
 * \return pointer to the page structure.
 *
 * Get the mapping, find the real physical page to map, get the page, and
 * return it.
 */
static __inline__ struct page *drm_do_vm_shm_nopage(struct vm_area_struct *vma,
						    unsigned long address)
{
	struct drm_map *map = (struct drm_map *) vma->vm_private_data;
	unsigned long offset;
	unsigned long i;
	struct page *page;

	if (address > vma->vm_end)
		return NOPAGE_SIGBUS;	/* Disallow mremap */
	if (!map)
		return NOPAGE_SIGBUS;	/* Nothing allocated */

	offset = address - vma->vm_start;
	i = (unsigned long)map->handle + offset;
	page = vmalloc_to_page((void *)i);
	if (!page)
		return NOPAGE_SIGBUS;
	get_page(page);

	DRM_DEBUG("0x%lx\n", address);
	return page;
}

/**
 * \c nopage method for DMA virtual memory.
 *
 * \param vma virtual memory area.
 * \param address access address.
 * \return pointer to the page structure.
 *
 * Determine the page number from the page offset and get it from drm_device_dma::pagelist.
 */
static __inline__ struct page *drm_do_vm_dma_nopage(struct vm_area_struct *vma,
						    unsigned long address)
{
	struct drm_file *priv = vma->vm_file->private_data;
	struct drm_device *dev = priv->minor->dev;
	struct drm_device_dma *dma = dev->dma;
	unsigned long offset;
	unsigned long page_nr;
	struct page *page;

	if (!dma)
		return NOPAGE_SIGBUS;	/* Error */
	if (address > vma->vm_end)
		return NOPAGE_SIGBUS;	/* Disallow mremap */
	if (!dma->pagelist)
		return NOPAGE_SIGBUS;	/* Nothing allocated */

	offset = address - vma->vm_start;	/* vm_[pg]off[set] should be 0 */
	page_nr = offset >> PAGE_SHIFT;
	page = virt_to_page((dma->pagelist[page_nr] + (offset & (~PAGE_MASK))));

	get_page(page);

	DRM_DEBUG("0x%lx (page %lu)\n", address, page_nr);
	return page;
}

/**
 * \c nopage method for scatter-gather virtual memory.
 *
 * \param vma virtual memory area.
 * \param address access address.
 * \return pointer to the page structure.
 *
 * Determine the map offset from the page offset and get it from drm_sg_mem::pagelist.
 */
static __inline__ struct page *drm_do_vm_sg_nopage(struct vm_area_struct *vma,
						   unsigned long address)
{
	struct drm_map *map = (struct drm_map *) vma->vm_private_data;
	struct drm_file *priv = vma->vm_file->private_data;
	struct drm_device *dev = priv->minor->dev;
	struct drm_sg_mem *entry = dev->sg;
	unsigned long offset;
	unsigned long map_offset;
	unsigned long page_offset;
	struct page *page;

	DRM_DEBUG("\n");
	if (!entry)
		return NOPAGE_SIGBUS;	/* Error */
	if (address > vma->vm_end)
		return NOPAGE_SIGBUS;	/* Disallow mremap */
	if (!entry->pagelist)
		return NOPAGE_SIGBUS;	/* Nothing allocated */

	offset = address - vma->vm_start;
	map_offset = map->offset - (unsigned long)dev->sg->virtual;
	page_offset = (offset >> PAGE_SHIFT) + (map_offset >> PAGE_SHIFT);
	page = entry->pagelist[page_offset];
	get_page(page);

	return page;
}


struct page *drm_vm_nopage(struct vm_area_struct *vma,
			   unsigned long address, int *type)
{
	if (type)
		*type = VM_FAULT_MINOR;
	return drm_do_vm_nopage(vma, address);
}

struct page *drm_vm_shm_nopage(struct vm_area_struct *vma,
			       unsigned long address, int *type)
{
	if (type)
		*type = VM_FAULT_MINOR;
	return drm_do_vm_shm_nopage(vma, address);
}

struct page *drm_vm_dma_nopage(struct vm_area_struct *vma,
			       unsigned long address, int *type)
{
	if (type)
		*type = VM_FAULT_MINOR;
	return drm_do_vm_dma_nopage(vma, address);
}

struct page *drm_vm_sg_nopage(struct vm_area_struct *vma,
			      unsigned long address, int *type)
{
	if (type)
		*type = VM_FAULT_MINOR;
	return drm_do_vm_sg_nopage(vma, address);
}
#endif
