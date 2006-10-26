/**************************************************************************
 * 
 * Copyright 2006 Tungsten Graphics, Inc., Bismarck, ND., USA
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE 
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * 
 **************************************************************************/

#include "drmP.h"

static void drm_ttm_ipi_handler(void *null)
{
	wbinvd();
}

static void drm_ttm_cache_flush(void) 
{
	if (on_each_cpu(drm_ttm_ipi_handler, NULL, 1, 1) != 0)
		DRM_ERROR("Timed out waiting for drm cache flush.\n");
}


/*
 * Use kmalloc if possible. Otherwise fall back to vmalloc.
 */

static void *ttm_alloc(unsigned long size, int type)
{
	void *ret = NULL;

	if (drm_alloc_memctl(size))
		return NULL;
	if (size <= PAGE_SIZE) {
		ret = drm_alloc(size, type);
	}
	if (!ret) {
		ret = vmalloc(size);
	}
	if (!ret) {
		drm_free_memctl(size);
	}
	return ret;
}

static void ttm_free(void *pointer, unsigned long size, int type)
{

	if ((unsigned long)pointer >= VMALLOC_START &&
	    (unsigned long)pointer <= VMALLOC_END) {
		vfree(pointer);
	} else {
		drm_free(pointer, size, type);
	}
	drm_free_memctl(size);
}

/*
 * Unmap all vma pages from vmas mapping this ttm.
 */

static int unmap_vma_pages(drm_ttm_t * ttm)
{
	drm_device_t *dev = ttm->dev;
	loff_t offset = ((loff_t) ttm->mapping_offset) << PAGE_SHIFT;
	loff_t holelen = ((loff_t) ttm->num_pages) << PAGE_SHIFT;

#ifdef DRM_ODD_MM_COMPAT
	int ret;
	ret = drm_ttm_lock_mm(ttm);
	if (ret)
		return ret;
#endif
	unmap_mapping_range(dev->dev_mapping, offset, holelen, 1);
#ifdef DRM_ODD_MM_COMPAT
	drm_ttm_finish_unmap(ttm);
#endif
	return 0;
}

/*
 * Change caching policy for the linear kernel map 
 * for range of pages in a ttm.
 */

static int drm_set_caching(drm_ttm_t * ttm, int noncached)
{
	int i;
	struct page **cur_page;
	int do_tlbflush = 0;

	if ((ttm->page_flags & DRM_TTM_PAGE_UNCACHED) == noncached)
		return 0;

	if (noncached) 
		drm_ttm_cache_flush();

	for (i = 0; i < ttm->num_pages; ++i) {
		cur_page = ttm->pages + i;
		if (*cur_page) {
			if (!PageHighMem(*cur_page)) {
				if (noncached) {
					map_page_into_agp(*cur_page);
				} else {
					unmap_page_from_agp(*cur_page);
				}
				do_tlbflush = 1;
			}
		}
	}
	if (do_tlbflush)
		flush_agp_mappings();

	DRM_MASK_VAL(ttm->page_flags, DRM_TTM_PAGE_UNCACHED, noncached);

	return 0;
}

/*
 * Free all resources associated with a ttm.
 */

int drm_destroy_ttm(drm_ttm_t * ttm)
{

	int i;
	struct page **cur_page;
	drm_ttm_backend_t *be;

	if (!ttm)
		return 0;

	if (atomic_read(&ttm->vma_count) > 0) {
		ttm->destroy = 1;
		DRM_ERROR("VMAs are still alive. Skipping destruction.\n");
		return -EBUSY;
	}

	DRM_DEBUG("Destroying a ttm\n");

#ifdef DRM_TTM_ODD_COMPAT
	BUG_ON(!list_empty(&ttm->vma_list));
	BUG_ON(!list_empty(&ttm->p_mm_list));
#endif
	be = ttm->be;
	if (be) {
		be->destroy(be);
		ttm->be = NULL;
	}

	if (ttm->pages) {
		drm_buffer_manager_t *bm = &ttm->dev->bm;
		if (ttm->page_flags & DRM_TTM_PAGE_UNCACHED)
			drm_set_caching(ttm, 0);

		for (i = 0; i < ttm->num_pages; ++i) {
			cur_page = ttm->pages + i;
			if (*cur_page) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15))
				unlock_page(*cur_page);
#else
				ClearPageReserved(*cur_page);
#endif
				if (page_count(*cur_page) != 1) {
					DRM_ERROR("Erroneous page count. "
						  "Leaking pages.\n");
				}
				if (page_mapped(*cur_page)) {
					DRM_ERROR("Erroneous map count. "
						  "Leaking page mappings.\n");
				}

				/*
				 * End debugging.
				 */

				drm_free_gatt_pages(*cur_page, 0);
				drm_free_memctl(PAGE_SIZE);
				--bm->cur_pages;
			}
		}
		ttm_free(ttm->pages, ttm->num_pages * sizeof(*ttm->pages),
			 DRM_MEM_TTM);
		ttm->pages = NULL;
	}

	drm_ctl_free(ttm, sizeof(*ttm), DRM_MEM_TTM);
	return 0;
}

static int drm_ttm_populate(drm_ttm_t * ttm)
{
	struct page *page;
	unsigned long i;
	drm_buffer_manager_t *bm;
	drm_ttm_backend_t *be;

	if (ttm->state != ttm_unpopulated)
		return 0;

	bm = &ttm->dev->bm;
	be = ttm->be;
	for (i = 0; i < ttm->num_pages; ++i) {
		page = ttm->pages[i];
		if (!page) {
			if (drm_alloc_memctl(PAGE_SIZE)) {
				return -ENOMEM;
			}
			page = drm_alloc_gatt_pages(0);
			if (!page) {
				drm_free_memctl(PAGE_SIZE);
				return -ENOMEM;
			}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15))
			SetPageLocked(page);
#else
			SetPageReserved(page);
#endif
			ttm->pages[i] = page;
			++bm->cur_pages;
		}
	}
	be->populate(be, ttm->num_pages, ttm->pages);
	ttm->state = ttm_unbound;
	return 0;
}

/*
 * Initialize a ttm.
 */

static drm_ttm_t *drm_init_ttm(struct drm_device *dev, unsigned long size)
{
	drm_bo_driver_t *bo_driver = dev->driver->bo_driver;
	drm_ttm_t *ttm;

	if (!bo_driver)
		return NULL;

	ttm = drm_ctl_calloc(1, sizeof(*ttm), DRM_MEM_TTM);
	if (!ttm)
		return NULL;

#ifdef DRM_ODD_MM_COMPAT
	INIT_LIST_HEAD(&ttm->p_mm_list);
	INIT_LIST_HEAD(&ttm->vma_list);
#endif

	ttm->dev = dev;
	atomic_set(&ttm->vma_count, 0);

	ttm->destroy = 0;
	ttm->num_pages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;

	ttm->page_flags = 0;

	/*
	 * Account also for AGP module memory usage.
	 */

	ttm->pages = ttm_alloc(ttm->num_pages * sizeof(*ttm->pages),
			       DRM_MEM_TTM);
	if (!ttm->pages) {
		drm_destroy_ttm(ttm);
		DRM_ERROR("Failed allocating page table\n");
		return NULL;
	}
	memset(ttm->pages, 0, ttm->num_pages * sizeof(*ttm->pages));
	ttm->be = bo_driver->create_ttm_backend_entry(dev);
	if (!ttm->be) {
		drm_destroy_ttm(ttm);
		DRM_ERROR("Failed creating ttm backend entry\n");
		return NULL;
	}
	ttm->state = ttm_unpopulated;
	return ttm;
}

/*
 * Unbind a ttm region from the aperture.
 */

int drm_evict_ttm(drm_ttm_t * ttm)
{
	drm_ttm_backend_t *be = ttm->be;
	int ret;

	switch (ttm->state) {
	case ttm_bound:
		if (be->needs_ub_cache_adjust(be)) {
			ret = unmap_vma_pages(ttm);
			if (ret) {
				return ret;
			}
		}
		be->unbind(be);
		break;
	default:
		break;
	}
	ttm->state = ttm_evicted;
	return 0;
}

void drm_fixup_ttm_caching(drm_ttm_t * ttm)
{

	if (ttm->state == ttm_evicted) {
		drm_ttm_backend_t *be = ttm->be;
		if (be->needs_ub_cache_adjust(be)) {
			drm_set_caching(ttm, 0);
		}
		ttm->state = ttm_unbound;
	}
}

int drm_unbind_ttm(drm_ttm_t * ttm)
{
	int ret = 0;

	if (ttm->state == ttm_bound)
		ret = drm_evict_ttm(ttm);

	if (ret)
		return ret;

	drm_fixup_ttm_caching(ttm);
	return 0;
}

int drm_bind_ttm(drm_ttm_t * ttm, int cached, unsigned long aper_offset)
{

	int ret = 0;
	drm_ttm_backend_t *be;

	if (!ttm)
		return -EINVAL;
	if (ttm->state == ttm_bound)
		return 0;

	be = ttm->be;

	ret = drm_ttm_populate(ttm);
	if (ret)
		return ret;
	if (ttm->state == ttm_unbound && !cached) {
		ret = unmap_vma_pages(ttm);
		if (ret)
			return ret;

		drm_set_caching(ttm, DRM_TTM_PAGE_UNCACHED);
	}
#ifdef DRM_ODD_MM_COMPAT
	else if (ttm->state == ttm_evicted && !cached) {
		ret = drm_ttm_lock_mm(ttm);
		if (ret)
			return ret;
	}
#endif
	if ((ret = be->bind(be, aper_offset, cached))) {
		ttm->state = ttm_evicted;
#ifdef DRM_ODD_MM_COMPAT
		if (be->needs_ub_cache_adjust(be))
			drm_ttm_unlock_mm(ttm);
#endif
		DRM_ERROR("Couldn't bind backend.\n");
		return ret;
	}

	ttm->aper_offset = aper_offset;
	ttm->state = ttm_bound;

#ifdef DRM_ODD_MM_COMPAT
	if (be->needs_ub_cache_adjust(be)) {
		ret = drm_ttm_remap_bound(ttm);
		if (ret)
			return ret;
	}
#endif

	return 0;
}

/*
 * dev->struct_mutex locked.
 */
static void drm_ttm_object_remove(drm_device_t * dev, drm_ttm_object_t * object)
{
	drm_map_list_t *list = &object->map_list;
	drm_local_map_t *map;

	if (list->user_token)
		drm_ht_remove_item(&dev->map_hash, &list->hash);

	if (list->file_offset_node) {
		drm_mm_put_block(list->file_offset_node);
		list->file_offset_node = NULL;
	}

	map = list->map;

	if (map) {
		drm_ttm_t *ttm = (drm_ttm_t *) map->offset;
		if (ttm) {
			if (drm_destroy_ttm(ttm) != -EBUSY) {
				drm_ctl_free(map, sizeof(*map), DRM_MEM_TTM);
			}
		} else {
			drm_ctl_free(map, sizeof(*map), DRM_MEM_TTM);
		}
	}

	drm_ctl_free(object, sizeof(*object), DRM_MEM_TTM);
}

void drm_ttm_object_deref_locked(drm_device_t * dev, drm_ttm_object_t * to)
{
	if (atomic_dec_and_test(&to->usage)) {
		drm_ttm_object_remove(dev, to);
	}
}

void drm_ttm_object_deref_unlocked(drm_device_t * dev, drm_ttm_object_t * to)
{
	if (atomic_dec_and_test(&to->usage)) {
		mutex_lock(&dev->struct_mutex);
		if (atomic_read(&to->usage) == 0)
			drm_ttm_object_remove(dev, to);
		mutex_unlock(&dev->struct_mutex);
	}
}

/*
 * Create a ttm and add it to the drm book-keeping. 
 * dev->struct_mutex locked.
 */

int drm_ttm_object_create(drm_device_t * dev, unsigned long size,
			  uint32_t flags, drm_ttm_object_t ** ttm_object)
{
	drm_ttm_object_t *object;
	drm_map_list_t *list;
	drm_local_map_t *map;
	drm_ttm_t *ttm;

	object = drm_ctl_calloc(1, sizeof(*object), DRM_MEM_TTM);
	if (!object)
		return -ENOMEM;
	object->flags = flags;
	list = &object->map_list;

	list->map = drm_ctl_calloc(1, sizeof(*map), DRM_MEM_TTM);
	if (!list->map) {
		drm_ttm_object_remove(dev, object);
		return -ENOMEM;
	}
	map = list->map;

	ttm = drm_init_ttm(dev, size);
	if (!ttm) {
		DRM_ERROR("Could not create ttm\n");
		drm_ttm_object_remove(dev, object);
		return -ENOMEM;
	}

	map->offset = (unsigned long)ttm;
	map->type = _DRM_TTM;
	map->flags = _DRM_REMOVABLE;
	map->size = ttm->num_pages * PAGE_SIZE;
	map->handle = (void *)object;

	/*
	 * Add a one-page "hole" to the block size to avoid the mm subsystem
	 * merging vmas.
	 * FIXME: Is this really needed?
	 */

	list->file_offset_node = drm_mm_search_free(&dev->offset_manager,
						    ttm->num_pages + 1, 0, 0);
	if (!list->file_offset_node) {
		drm_ttm_object_remove(dev, object);
		return -ENOMEM;
	}
	list->file_offset_node = drm_mm_get_block(list->file_offset_node,
						  ttm->num_pages + 1, 0);

	list->hash.key = list->file_offset_node->start;

	if (drm_ht_insert_item(&dev->map_hash, &list->hash)) {
		drm_ttm_object_remove(dev, object);
		return -ENOMEM;
	}

	list->user_token = ((drm_u64_t) list->hash.key) << PAGE_SHIFT;
	ttm->mapping_offset = list->hash.key;
	atomic_set(&object->usage, 1);
	*ttm_object = object;
	return 0;
}
