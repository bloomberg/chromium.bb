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

#if defined(__ia64__)
#include <linux/efi.h>
#endif

static void drm_vm_open(struct vm_area_struct *vma);
static void drm_vm_close(struct vm_area_struct *vma);
static void drm_vm_ttm_close(struct vm_area_struct *vma);
static int drm_vm_ttm_open(struct vm_area_struct *vma);
static void drm_vm_ttm_open_wrapper(struct vm_area_struct *vma);


pgprot_t drm_io_prot(uint32_t map_type, struct vm_area_struct *vma)
{
	pgprot_t tmp = vm_get_page_prot(vma->vm_flags);

#if defined(__i386__) || defined(__x86_64__)
	if (boot_cpu_data.x86 > 3 && map_type != _DRM_AGP) {
		pgprot_val(tmp) |= _PAGE_PCD;
		pgprot_val(tmp) &= ~_PAGE_PWT;
	}
#elif defined(__powerpc__)
	pgprot_val(tmp) |= _PAGE_NO_CACHE;
	if (map->type == _DRM_REGISTERS)
		pgprot_val(tmp) |= _PAGE_GUARDED;
#endif
#if defined(__ia64__)
	if (efi_range_is_wc(vma->vm_start, vma->vm_end -
				    vma->vm_start))
		tmp = pgprot_writecombine(tmp);
	else
		tmp = pgprot_noncached(tmp);
#endif
	return tmp;
}


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
	drm_file_t *priv = vma->vm_file->private_data;
	drm_device_t *dev = priv->head->dev;
	drm_map_t *map = NULL;
	drm_map_list_t *r_list;
	drm_hash_item_t *hash;

	/*
	 * Find the right map
	 */
	if (!drm_core_has_AGP(dev))
		goto vm_nopage_error;

	if (!dev->agp || !dev->agp->cant_use_aperture)
		goto vm_nopage_error;

	if (drm_ht_find_item(&dev->map_hash, vma->vm_pgoff, &hash))
		goto vm_nopage_error;

	r_list = drm_hash_entry(hash, drm_map_list_t, hash);
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
		for (agpmem = dev->agp->memory; agpmem; agpmem = agpmem->next) {
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


static int drm_ttm_remap_bound_pfn(struct vm_area_struct *vma,
				   unsigned long address,
				   unsigned long size)
{
	unsigned long
		page_offset = (address - vma->vm_start) >> PAGE_SHIFT;
	unsigned long
		num_pages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	drm_ttm_vma_list_t *entry = (drm_ttm_vma_list_t *)
		vma->vm_private_data; 
	drm_map_t *map = entry->map;
	drm_ttm_t *ttm = (drm_ttm_t *) map->offset;
	unsigned long i, cur_pfn;
	unsigned long start = 0;
	unsigned long end = 0;
	unsigned long last_pfn = 0; 
	unsigned long start_pfn = 0;
	int bound_sequence = FALSE;
	int ret = 0;
	uint32_t cur_flags;

	for (i=page_offset; i<page_offset + num_pages; ++i) {
		cur_flags = ttm->page_flags[i];

		if (!bound_sequence && (cur_flags & DRM_TTM_PAGE_UNCACHED)) {

			start = i;
			end = i;
			last_pfn = (cur_flags & DRM_TTM_MASK_PFN) >> PAGE_SHIFT;
			start_pfn = last_pfn;
			bound_sequence = TRUE;

		} else if (bound_sequence) {

			cur_pfn = (cur_flags & DRM_TTM_MASK_PFN) >> PAGE_SHIFT;

			if ( !(cur_flags & DRM_TTM_PAGE_UNCACHED) || 
			     (cur_pfn != last_pfn + 1)) {

				ret = io_remap_pfn_range(vma, 
							 vma->vm_start + (start << PAGE_SHIFT),
							 (ttm->aperture_base >> PAGE_SHIFT) 
							 + start_pfn,
							 (end - start + 1) << PAGE_SHIFT,
							 drm_io_prot(_DRM_AGP, vma));
				
				if (ret) 
					break;

				bound_sequence = (cur_flags & DRM_TTM_PAGE_UNCACHED);
				if (!bound_sequence) 
					continue;

				start = i;
				end = i;
				last_pfn = cur_pfn;
				start_pfn = last_pfn;

			} else {
				
				end++;
				last_pfn = cur_pfn;

			}
		}
	}

	if (!ret && bound_sequence) {
		ret = io_remap_pfn_range(vma, 
					 vma->vm_start + (start << PAGE_SHIFT),
					 (ttm->aperture_base >> PAGE_SHIFT) 
					 + start_pfn,
					 (end - start + 1) << PAGE_SHIFT,
					 drm_io_prot(_DRM_AGP, vma));
	}

	if (ret) {
		DRM_ERROR("Map returned %c\n", ret);
	}
	return ret;
}
	
static __inline__ struct page *drm_do_vm_ttm_nopage(struct vm_area_struct *vma,
						    unsigned long address)
{
	drm_ttm_vma_list_t *entry = (drm_ttm_vma_list_t *)
		vma->vm_private_data; 
	drm_map_t *map;
	unsigned long page_offset;
	struct page *page;
	drm_ttm_t *ttm; 
	pgprot_t default_prot;
	uint32_t page_flags;
	drm_buffer_manager_t *bm;
	drm_device_t *dev;

	if (address > vma->vm_end)
		return NOPAGE_SIGBUS;	/* Disallow mremap */
	if (!entry)
		return NOPAGE_OOM;	/* Nothing allocated */

	map = (drm_map_t *) entry->map;
	ttm = (drm_ttm_t *) map->offset;
	
	dev = ttm->dev;
	mutex_lock(&dev->struct_mutex);

	bm = &dev->bm;
	page_offset = (address - vma->vm_start) >> PAGE_SHIFT;
	page = ttm->pages[page_offset];

	page_flags = ttm->page_flags[page_offset];

	if (!page) {
		if (bm->cur_pages >= bm->max_pages) {
	 		DRM_ERROR("Maximum locked page count exceeded\n"); 
			page = NOPAGE_OOM;
			goto out;
		}
		++bm->cur_pages;
		page = ttm->pages[page_offset] = drm_alloc_gatt_pages(0);
		if (page) {
			SetPageLocked(page);
		} else {
			page = NOPAGE_OOM;
		}
	}

	if (page_flags & DRM_TTM_PAGE_UNCACHED) {

		/*
		 * This makes sure we don't race with another 
		 * drm_ttm_remap_bound_pfn();
		 */

		if (!drm_pte_is_clear(vma, address)) {
			page = NOPAGE_RETRY;
			goto out1;
		}
		       		
		drm_ttm_remap_bound_pfn(vma, address, PAGE_SIZE);
		page = NOPAGE_RETRY;
		goto out1;
	}
	get_page(page);
	
 out1:
	default_prot = vm_get_page_prot(vma->vm_flags);	    
	vma->vm_page_prot = default_prot;

 out:
	mutex_unlock(&dev->struct_mutex);
	return page;
}

/**
 * \c nopage method for shared virtual memory.
 *
 * \param vma virtual memory area.
 * \param address access address.
 * \return pointer to the page structure.
 *
 * Get the the mapping, find the real physical page to map, get the page, and
 * return it.
 */
static __inline__ struct page *drm_do_vm_shm_nopage(struct vm_area_struct *vma,
						    unsigned long address)
{
	drm_map_t *map = (drm_map_t *) vma->vm_private_data;
	unsigned long offset;
	unsigned long i;
	struct page *page;

	if (address > vma->vm_end)
		return NOPAGE_SIGBUS;	/* Disallow mremap */
	if (!map)
		return NOPAGE_OOM;	/* Nothing allocated */

	offset = address - vma->vm_start;
	i = (unsigned long)map->handle + offset;
	page = vmalloc_to_page((void *)i);
	if (!page)
		return NOPAGE_OOM;
	get_page(page);

	DRM_DEBUG("shm_nopage 0x%lx\n", address);
	return page;
}

/**
 * \c close method for shared virtual memory.
 *
 * \param vma virtual memory area.
 *
 * Deletes map information if we are the last
 * person to close a mapping and it's not in the global maplist.
 */
static void drm_vm_shm_close(struct vm_area_struct *vma)
{
	drm_file_t *priv = vma->vm_file->private_data;
	drm_device_t *dev = priv->head->dev;
	drm_vma_entry_t *pt, *prev, *next;
	drm_map_t *map;
	drm_map_list_t *r_list;
	struct list_head *list;
	int found_maps = 0;

	DRM_DEBUG("0x%08lx,0x%08lx\n",
		  vma->vm_start, vma->vm_end - vma->vm_start);
	atomic_dec(&dev->vma_count);

	map = vma->vm_private_data;

	mutex_lock(&dev->struct_mutex);
	for (pt = dev->vmalist, prev = NULL; pt; pt = next) {
		next = pt->next;
		if (pt->vma->vm_private_data == map)
			found_maps++;
		if (pt->vma == vma) {
			if (prev) {
				prev->next = pt->next;
			} else {
				dev->vmalist = pt->next;
			}
			drm_free(pt, sizeof(*pt), DRM_MEM_VMAS);
		} else {
			prev = pt;
		}
	}
	/* We were the only map that was found */
	if (found_maps == 1 && map->flags & _DRM_REMOVABLE) {
		/* Check to see if we are in the maplist, if we are not, then
		 * we delete this mappings information.
		 */
		found_maps = 0;
		list = &dev->maplist->head;
		list_for_each(list, &dev->maplist->head) {
			r_list = list_entry(list, drm_map_list_t, head);
			if (r_list->map == map)
				found_maps++;
		}

		if (!found_maps) {
			drm_dma_handle_t dmah;

			switch (map->type) {
			case _DRM_REGISTERS:
			case _DRM_FRAME_BUFFER:
				if (drm_core_has_MTRR(dev) && map->mtrr >= 0) {
					int retcode;
					retcode = mtrr_del(map->mtrr,
							   map->offset,
							   map->size);
					DRM_DEBUG("mtrr_del = %d\n", retcode);
				}
				drm_ioremapfree(map->handle, map->size, dev);
				break;
			case _DRM_SHM:
				vfree(map->handle);
				break;
			case _DRM_AGP:
			case _DRM_SCATTER_GATHER:
				break;
			case _DRM_CONSISTENT:
				dmah.vaddr = map->handle;
				dmah.busaddr = map->offset;
				dmah.size = map->size;
				__drm_pci_free(dev, &dmah);
				break;
		        case _DRM_TTM:
				BUG_ON(1);
				break;
			}
			drm_free(map, sizeof(*map), DRM_MEM_MAPS);
		}
	}
	mutex_unlock(&dev->struct_mutex);
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
	drm_file_t *priv = vma->vm_file->private_data;
	drm_device_t *dev = priv->head->dev;
	drm_device_dma_t *dma = dev->dma;
	unsigned long offset;
	unsigned long page_nr;
	struct page *page;

	if (!dma)
		return NOPAGE_SIGBUS;	/* Error */
	if (address > vma->vm_end)
		return NOPAGE_SIGBUS;	/* Disallow mremap */
	if (!dma->pagelist)
		return NOPAGE_OOM;	/* Nothing allocated */

	offset = address - vma->vm_start;	/* vm_[pg]off[set] should be 0 */
	page_nr = offset >> PAGE_SHIFT;
	page = virt_to_page((dma->pagelist[page_nr] + (offset & (~PAGE_MASK))));

	get_page(page);

	DRM_DEBUG("dma_nopage 0x%lx (page %lu)\n", address, page_nr);
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
	drm_map_t *map = (drm_map_t *) vma->vm_private_data;
	drm_file_t *priv = vma->vm_file->private_data;
	drm_device_t *dev = priv->head->dev;
	drm_sg_mem_t *entry = dev->sg;
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
		return NOPAGE_OOM;	/* Nothing allocated */

	offset = address - vma->vm_start;
	map_offset = map->offset - (unsigned long)dev->sg->virtual;
	page_offset = (offset >> PAGE_SHIFT) + (map_offset >> PAGE_SHIFT);
	page = entry->pagelist[page_offset];
	get_page(page);

	return page;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)

static struct page *drm_vm_nopage(struct vm_area_struct *vma,
				  unsigned long address, int *type)
{
	if (type)
		*type = VM_FAULT_MINOR;
	return drm_do_vm_nopage(vma, address);
}

static struct page *drm_vm_shm_nopage(struct vm_area_struct *vma,
				      unsigned long address, int *type)
{
	if (type)
		*type = VM_FAULT_MINOR;
	return drm_do_vm_shm_nopage(vma, address);
}

static struct page *drm_vm_dma_nopage(struct vm_area_struct *vma,
				      unsigned long address, int *type)
{
	if (type)
		*type = VM_FAULT_MINOR;
	return drm_do_vm_dma_nopage(vma, address);
}

static struct page *drm_vm_sg_nopage(struct vm_area_struct *vma,
				     unsigned long address, int *type)
{
	if (type)
		*type = VM_FAULT_MINOR;
	return drm_do_vm_sg_nopage(vma, address);
}

static struct page *drm_vm_ttm_nopage(struct vm_area_struct *vma,
				     unsigned long address, int *type)
{
	if (type)
		*type = VM_FAULT_MINOR;
	return drm_do_vm_ttm_nopage(vma, address);
}


#else				/* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,0) */

static struct page *drm_vm_nopage(struct vm_area_struct *vma,
				  unsigned long address, int unused)
{
	return drm_do_vm_nopage(vma, address);
}

static struct page *drm_vm_shm_nopage(struct vm_area_struct *vma,
				      unsigned long address, int unused)
{
	return drm_do_vm_shm_nopage(vma, address);
}

static struct page *drm_vm_dma_nopage(struct vm_area_struct *vma,
				      unsigned long address, int unused)
{
	return drm_do_vm_dma_nopage(vma, address);
}

static struct page *drm_vm_sg_nopage(struct vm_area_struct *vma,
				     unsigned long address, int unused)
{
	return drm_do_vm_sg_nopage(vma, address);
}

static struct page *drm_vm_ttm_nopage(struct vm_area_struct *vma,
				     unsigned long address, int unused)
{
	return drm_do_vm_ttm_nopage(vma, address);
}


#endif

/** AGP virtual memory operations */
static struct vm_operations_struct drm_vm_ops = {
	.nopage = drm_vm_nopage,
	.open = drm_vm_open,
	.close = drm_vm_close,
};

/** Shared virtual memory operations */
static struct vm_operations_struct drm_vm_shm_ops = {
	.nopage = drm_vm_shm_nopage,
	.open = drm_vm_open,
	.close = drm_vm_shm_close,
};

/** DMA virtual memory operations */
static struct vm_operations_struct drm_vm_dma_ops = {
	.nopage = drm_vm_dma_nopage,
	.open = drm_vm_open,
	.close = drm_vm_close,
};

/** Scatter-gather virtual memory operations */
static struct vm_operations_struct drm_vm_sg_ops = {
	.nopage = drm_vm_sg_nopage,
	.open = drm_vm_open,
	.close = drm_vm_close,
};

static struct vm_operations_struct drm_vm_ttm_ops = {
	.nopage = drm_vm_ttm_nopage,
	.open = drm_vm_ttm_open_wrapper,
	.close = drm_vm_ttm_close,
};

/**
 * \c open method for shared virtual memory.
 *
 * \param vma virtual memory area.
 *
 * Create a new drm_vma_entry structure as the \p vma private data entry and
 * add it to drm_device::vmalist.
 */
static void drm_vm_open(struct vm_area_struct *vma)
{
	drm_file_t *priv = vma->vm_file->private_data;
	drm_device_t *dev = priv->head->dev;
	drm_vma_entry_t *vma_entry;

	DRM_DEBUG("0x%08lx,0x%08lx\n",
		  vma->vm_start, vma->vm_end - vma->vm_start);
	atomic_inc(&dev->vma_count);

	vma_entry = drm_alloc(sizeof(*vma_entry), DRM_MEM_VMAS);
	if (vma_entry) {
		mutex_lock(&dev->struct_mutex);
		vma_entry->vma = vma;
		vma_entry->next = dev->vmalist;
		vma_entry->pid = current->pid;
		dev->vmalist = vma_entry;
		mutex_unlock(&dev->struct_mutex);
	}
}

static int drm_vm_ttm_open(struct vm_area_struct *vma) {
  
	drm_ttm_vma_list_t *entry, *tmp_vma = 
		(drm_ttm_vma_list_t *) vma->vm_private_data;
	drm_map_t *map;
	drm_ttm_t *ttm;
	drm_file_t *priv = vma->vm_file->private_data;
	drm_device_t *dev = priv->head->dev;
	int ret = 0;

	drm_vm_open(vma);
	mutex_lock(&dev->struct_mutex);
	entry = drm_calloc(1, sizeof(*entry), DRM_MEM_VMAS);
	if (entry) {
	        *entry = *tmp_vma;
		map = (drm_map_t *) entry->map;
		ttm = (drm_ttm_t *) map->offset;
		if (!ret) {
			atomic_inc(&ttm->vma_count);
			INIT_LIST_HEAD(&entry->head);
			entry->vma = vma;
			entry->orig_protection = vma->vm_page_prot;
			list_add_tail(&entry->head, &ttm->vma_list->head);
			vma->vm_private_data = (void *) entry;
			DRM_DEBUG("Added VMA to ttm at 0x%016lx\n", 
				  (unsigned long) ttm);
		}
	} else {
		ret = -ENOMEM;
	}
	mutex_unlock(&dev->struct_mutex);
	return ret;
}

static void drm_vm_ttm_open_wrapper(struct vm_area_struct *vma) 
{
	drm_vm_ttm_open(vma);
}

/**
 * \c close method for all virtual memory types.
 *
 * \param vma virtual memory area.
 *
 * Search the \p vma private data entry in drm_device::vmalist, unlink it, and
 * free it.
 */
static void drm_vm_close(struct vm_area_struct *vma)
{
	drm_file_t *priv = vma->vm_file->private_data;
	drm_device_t *dev = priv->head->dev;
	drm_vma_entry_t *pt, *prev;

	DRM_DEBUG("0x%08lx,0x%08lx\n",
		  vma->vm_start, vma->vm_end - vma->vm_start);
	atomic_dec(&dev->vma_count);

	mutex_lock(&dev->struct_mutex);
	for (pt = dev->vmalist, prev = NULL; pt; prev = pt, pt = pt->next) {
		if (pt->vma == vma) {
			if (prev) {
				prev->next = pt->next;
			} else {
				dev->vmalist = pt->next;
			}
			drm_free(pt, sizeof(*pt), DRM_MEM_VMAS);
			break;
		}
	}
	mutex_unlock(&dev->struct_mutex);
}


static void drm_vm_ttm_close(struct vm_area_struct *vma)
{
	drm_ttm_vma_list_t *ttm_vma = 
		(drm_ttm_vma_list_t *) vma->vm_private_data;
	drm_map_t *map; 
	drm_ttm_t *ttm; 
        drm_device_t *dev;
	int ret;

	drm_vm_close(vma); 
	if (ttm_vma) {
		map = (drm_map_t *) ttm_vma->map;
		ttm = (drm_ttm_t *) map->offset;
		dev = ttm->dev;
		mutex_lock(&dev->struct_mutex);
		list_del(&ttm_vma->head);
		drm_free(ttm_vma, sizeof(*ttm_vma), DRM_MEM_VMAS);
		if (atomic_dec_and_test(&ttm->vma_count)) {
			if (ttm->destroy) {
				ret = drm_destroy_ttm(ttm);
				BUG_ON(ret);
				drm_free(map, sizeof(*map), DRM_MEM_TTM);
			}
		}
		mutex_unlock(&dev->struct_mutex);
	}
	return;
}


/**
 * mmap DMA memory.
 *
 * \param filp file pointer.
 * \param vma virtual memory area.
 * \return zero on success or a negative number on failure.
 *
 * Sets the virtual memory area operations structure to vm_dma_ops, the file
 * pointer, and calls vm_open().
 */
static int drm_mmap_dma(struct file *filp, struct vm_area_struct *vma)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev;
	drm_device_dma_t *dma;
	unsigned long length = vma->vm_end - vma->vm_start;

	lock_kernel();
	dev = priv->head->dev;
	dma = dev->dma;
	DRM_DEBUG("start = 0x%lx, end = 0x%lx, page offset = 0x%lx\n",
		  vma->vm_start, vma->vm_end, vma->vm_pgoff);

	/* Length must match exact page count */
	if (!dma || (length >> PAGE_SHIFT) != dma->page_count) {
		unlock_kernel();
		return -EINVAL;
	}
	unlock_kernel();

	vma->vm_ops = &drm_vm_dma_ops;

#if LINUX_VERSION_CODE <= 0x02040e	/* KERNEL_VERSION(2,4,14) */
	vma->vm_flags |= VM_LOCKED | VM_SHM;	/* Don't swap */
#else
	vma->vm_flags |= VM_RESERVED;	/* Don't swap */
#endif

	vma->vm_file = filp;	/* Needed for drm_vm_open() */
	drm_vm_open(vma);
	return 0;
}

unsigned long drm_core_get_map_ofs(drm_map_t * map)
{
	return map->offset;
}
EXPORT_SYMBOL(drm_core_get_map_ofs);

unsigned long drm_core_get_reg_ofs(struct drm_device *dev)
{
#ifdef __alpha__
	return dev->hose->dense_mem_base - dev->hose->mem_space->start;
#else
	return 0;
#endif
}
EXPORT_SYMBOL(drm_core_get_reg_ofs);

/**
 * mmap DMA memory.
 *
 * \param filp file pointer.
 * \param vma virtual memory area.
 * \return zero on success or a negative number on failure.
 *
 * If the virtual memory area has no offset associated with it then it's a DMA
 * area, so calls mmap_dma(). Otherwise searches the map in drm_device::maplist,
 * checks that the restricted flag is not set, sets the virtual memory operations
 * according to the mapping type and remaps the pages. Finally sets the file
 * pointer and calls vm_open().
 */
int drm_mmap(struct file *filp, struct vm_area_struct *vma)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->head->dev;
	drm_map_t *map = NULL;
	unsigned long offset = 0;
	drm_hash_item_t *hash;

	DRM_DEBUG("start = 0x%lx, end = 0x%lx, page offset = 0x%lx\n",
		  vma->vm_start, vma->vm_end, vma->vm_pgoff);

	if (!priv->authenticated)
		return -EACCES;

	/* We check for "dma". On Apple's UniNorth, it's valid to have
	 * the AGP mapped at physical address 0
	 * --BenH.
	 */
	if (!vma->vm_pgoff
#if __OS_HAS_AGP
	    && (!dev->agp
		|| dev->agp->agp_info.device->vendor != PCI_VENDOR_ID_APPLE)
#endif
	    )
		return drm_mmap_dma(filp, vma);

	if (drm_ht_find_item(&dev->map_hash, vma->vm_pgoff , &hash)) {
		DRM_ERROR("Could not find map\n");
		return -EINVAL;
	}

	map = drm_hash_entry(hash,drm_map_list_t, hash)->map;

	if (!map || ((map->flags & _DRM_RESTRICTED) && !capable(CAP_SYS_ADMIN)))
		return -EPERM;

	/* Check for valid size. */
	if (map->size < vma->vm_end - vma->vm_start)
		return -EINVAL;

	if (!capable(CAP_SYS_ADMIN) && (map->flags & _DRM_READ_ONLY)) {
		vma->vm_flags &= ~(VM_WRITE | VM_MAYWRITE);
#if defined(__i386__) || defined(__x86_64__)
		pgprot_val(vma->vm_page_prot) &= ~_PAGE_RW;
#else
		/* Ye gads this is ugly.  With more thought
		   we could move this up higher and use
		   `protection_map' instead.  */
		vma->vm_page_prot =
		    __pgprot(pte_val
			     (pte_wrprotect
			      (__pte(pgprot_val(vma->vm_page_prot)))));
#endif
	}

	switch (map->type) {
	case _DRM_AGP:
		if (drm_core_has_AGP(dev) && dev->agp->cant_use_aperture) {
			/*
			 * On some platforms we can't talk to bus dma address from the CPU, so for
			 * memory of type DRM_AGP, we'll deal with sorting out the real physical
			 * pages and mappings in nopage()
			 */
#if defined(__powerpc__)
			pgprot_val(vma->vm_page_prot) |= _PAGE_NO_CACHE;
#endif
			vma->vm_ops = &drm_vm_ops;
			break;
		}
		/* fall through to _DRM_FRAME_BUFFER... */
	case _DRM_FRAME_BUFFER:
	case _DRM_REGISTERS:
		offset = dev->driver->get_reg_ofs(dev);
		vma->vm_flags |= VM_IO;	/* not in core dump */
		vma->vm_page_prot = drm_io_prot(map->type, vma);
#ifdef __sparc__
		if (io_remap_pfn_range(vma, vma->vm_start,
					(map->offset + offset) >>PAGE_SHIFT,
					vma->vm_end - vma->vm_start,
					vma->vm_page_prot))
#else
		if (remap_pfn_range(vma, vma->vm_start,
				     (map->offset + offset) >> PAGE_SHIFT,
				     vma->vm_end - vma->vm_start,
				     vma->vm_page_prot))
#endif
			return -EAGAIN;
		DRM_DEBUG("   Type = %d; start = 0x%lx, end = 0x%lx,"
			  " offset = 0x%lx\n",
			  map->type,
			  vma->vm_start, vma->vm_end, map->offset + offset);
		vma->vm_ops = &drm_vm_ops;
		break;
	case _DRM_CONSISTENT:
		/* Consistent memory is really like shared memory. But
		 * it's allocated in a different way, so avoid nopage */
		if (remap_pfn_range(vma, vma->vm_start,
		    page_to_pfn(virt_to_page(map->handle)),
		    vma->vm_end - vma->vm_start, vma->vm_page_prot))
			return -EAGAIN;
	/* fall through to _DRM_SHM */
	case _DRM_SHM:
		vma->vm_ops = &drm_vm_shm_ops;
		vma->vm_private_data = (void *)map;
		/* Don't let this area swap.  Change when
		   DRM_KERNEL advisory is supported. */
#if LINUX_VERSION_CODE <= 0x02040e	/* KERNEL_VERSION(2,4,14) */
		vma->vm_flags |= VM_LOCKED;
#else
		vma->vm_flags |= VM_RESERVED;
#endif
		break;
	case _DRM_SCATTER_GATHER:
		vma->vm_ops = &drm_vm_sg_ops;
		vma->vm_private_data = (void *)map;
#if LINUX_VERSION_CODE <= 0x02040e	/* KERNEL_VERSION(2,4,14) */
		vma->vm_flags |= VM_LOCKED;
#else
		vma->vm_flags |= VM_RESERVED;
#endif
		break;
	case _DRM_TTM: {
		drm_ttm_vma_list_t tmp_vma;
		tmp_vma.orig_protection = vma->vm_page_prot;
		tmp_vma.map = map;
		vma->vm_ops = &drm_vm_ttm_ops;
		vma->vm_private_data = (void *) &tmp_vma;
		vma->vm_file = filp;
		vma->vm_flags |= VM_RESERVED | VM_IO;
		if (drm_ttm_remap_bound_pfn(vma,
					    vma->vm_start,
					    vma->vm_end - vma->vm_start))
			return -EAGAIN;
		if (drm_vm_ttm_open(vma))
		        return -EAGAIN;
		return 0;
	}
	default:
		return -EINVAL;	/* This should never happen. */
	}
#if LINUX_VERSION_CODE <= 0x02040e	/* KERNEL_VERSION(2,4,14) */
	vma->vm_flags |= VM_LOCKED | VM_SHM;	/* Don't swap */
#else
	vma->vm_flags |= VM_RESERVED;	/* Don't swap */
#endif

	vma->vm_file = filp;	/* Needed for drm_vm_open() */
	drm_vm_open(vma);
	return 0;
}
EXPORT_SYMBOL(drm_mmap);
