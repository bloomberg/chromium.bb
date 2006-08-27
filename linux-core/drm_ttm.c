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
#include <asm/tlbflush.h>

typedef struct p_mm_entry {
	struct list_head head;
	struct mm_struct *mm;
	atomic_t refcount;
} p_mm_entry_t;

typedef struct drm_val_action {
	int needs_rx_flush;
	int evicted_tt;
	int evicted_vram;
	int validated;
} drm_val_action_t;


/*
 * We may be manipulating other processes page tables, so for each TTM, keep track of 
 * which mm_structs are currently mapping the ttm so that we can take the appropriate
 * locks when we modify their page tables. A typical application is when we evict another
 * process' buffers.
 */

int drm_ttm_add_mm_to_list(drm_ttm_t * ttm, struct mm_struct *mm)
{
	p_mm_entry_t *entry, *n_entry;

	list_for_each_entry(entry, &ttm->p_mm_list, head) {
		if (mm == entry->mm) {
			atomic_inc(&entry->refcount);
			return 0;
		} else if ((unsigned long)mm < (unsigned long)entry->mm) ;
	}

	n_entry = drm_alloc(sizeof(*n_entry), DRM_MEM_TTM);
	if (!entry) {
		DRM_ERROR("Allocation of process mm pointer entry failed\n");
		return -ENOMEM;
	}
	INIT_LIST_HEAD(&n_entry->head);
	n_entry->mm = mm;
	atomic_set(&n_entry->refcount, 0);
	atomic_inc(&ttm->shared_count);
	ttm->mm_list_seq++;

	list_add_tail(&n_entry->head, &entry->head);

	return 0;
}

void drm_ttm_delete_mm(drm_ttm_t * ttm, struct mm_struct *mm)
{
	p_mm_entry_t *entry, *n;
	list_for_each_entry_safe(entry, n, &ttm->p_mm_list, head) {
		if (mm == entry->mm) {
			if (atomic_add_negative(-1, &entry->refcount)) {
				list_del(&entry->head);
				drm_free(entry, sizeof(*entry), DRM_MEM_TTM);
				atomic_dec(&ttm->shared_count);
				ttm->mm_list_seq++;
			}
			return;
		}
	}
	BUG_ON(1);
}

static void drm_ttm_lock_mm(drm_ttm_t * ttm, int mm_sem, int page_table)
{
	p_mm_entry_t *entry;

	list_for_each_entry(entry, &ttm->p_mm_list, head) {
		if (mm_sem) {
			down_write(&entry->mm->mmap_sem);
		}
		if (page_table) {
			spin_lock(&entry->mm->page_table_lock);
		}
	}
}

static void drm_ttm_unlock_mm(drm_ttm_t * ttm, int mm_sem, int page_table)
{
	p_mm_entry_t *entry;

	list_for_each_entry(entry, &ttm->p_mm_list, head) {
		if (page_table) {
			spin_unlock(&entry->mm->page_table_lock);
		}
		if (mm_sem) {
			up_write(&entry->mm->mmap_sem);
		}
	}
}

static int ioremap_vmas(drm_ttm_t * ttm, unsigned long page_offset,
			unsigned long num_pages, unsigned long aper_offset)
{
	struct list_head *list;
	int ret = 0;

	list_for_each(list, &ttm->vma_list->head) {
		drm_ttm_vma_list_t *entry =
		    list_entry(list, drm_ttm_vma_list_t, head);

		ret = io_remap_pfn_range(entry->vma,
					 entry->vma->vm_start +
					 (page_offset << PAGE_SHIFT),
					 (ttm->aperture_base >> PAGE_SHIFT) +
					 aper_offset, num_pages << PAGE_SHIFT,
					 drm_io_prot(_DRM_AGP, entry->vma));
		if (ret)
			break;
	}
	global_flush_tlb();
	return ret;
}

/*
 * Unmap all vma pages from vmas mapping this ttm.
 */

static int unmap_vma_pages(drm_ttm_t * ttm, unsigned long page_offset,
			   unsigned long num_pages)
{
	struct list_head *list;
	struct page **first_page = ttm->pages + page_offset;
	struct page **last_page = ttm->pages + (page_offset + num_pages);
	struct page **cur_page;
#if !defined(flush_tlb_mm) && defined(MODULE)
	int flush_tlb = 0;
#endif
	list_for_each(list, &ttm->vma_list->head) {
		drm_ttm_vma_list_t *entry =
		    list_entry(list, drm_ttm_vma_list_t, head);
		drm_clear_vma(entry->vma,
			      entry->vma->vm_start +
			      (page_offset << PAGE_SHIFT),
			      entry->vma->vm_start +
			      ((page_offset + num_pages) << PAGE_SHIFT));
#if !defined(flush_tlb_mm) && defined(MODULE)
		flush_tlb = 1;
#endif
	}
#if !defined(flush_tlb_mm) && defined(MODULE)
	if (flush_tlb)
		global_flush_tlb();
#endif

	for (cur_page = first_page; cur_page != last_page; ++cur_page) {
		if (page_mapcount(*cur_page) != 0) {
			DRM_ERROR("Mapped page detected. Map count is %d\n",
				  page_mapcount(*cur_page));
			return -1;
		}
	}
	return 0;
}

/*
 * Free all resources associated with a ttm.
 */

int drm_destroy_ttm(drm_ttm_t * ttm)
{

	int i;
	struct list_head *list, *next;
	struct page **cur_page;

	if (!ttm)
		return 0;

	if (atomic_read(&ttm->vma_count) > 0) {
		ttm->destroy = 1;
		DRM_DEBUG("VMAs are still alive. Skipping destruction.\n");
		return -EBUSY;
	} 

	if (ttm->be_list) {
		list_for_each_safe(list, next, &ttm->be_list->head) {
			drm_ttm_backend_list_t *entry =
			    list_entry(list, drm_ttm_backend_list_t, head);
			drm_destroy_ttm_region(entry);
		}

		drm_free(ttm->be_list, sizeof(*ttm->be_list), DRM_MEM_TTM);
		ttm->be_list = NULL;
	}

	if (ttm->pages) {
		for (i = 0; i < ttm->num_pages; ++i) {
			cur_page = ttm->pages + i;
			if (ttm->page_flags &&
			    (ttm->page_flags[i] & DRM_TTM_PAGE_UNCACHED) &&
			    *cur_page && !PageHighMem(*cur_page)) {
				change_page_attr(*cur_page, 1, PAGE_KERNEL);
			}
			if (*cur_page) {
				ClearPageReserved(*cur_page);
				__free_page(*cur_page);
			}
		}
		global_flush_tlb();
		vfree(ttm->pages);
		ttm->pages = NULL;
	}

	if (ttm->page_flags) {
		vfree(ttm->page_flags);
		ttm->page_flags = NULL;
	}

	if (ttm->vma_list) {
		list_for_each_safe(list, next, &ttm->vma_list->head) {
			drm_ttm_vma_list_t *entry =
			    list_entry(list, drm_ttm_vma_list_t, head);
			list_del(list);
			entry->vma->vm_private_data = NULL;
			drm_free(entry, sizeof(*entry), DRM_MEM_TTM);
		}
		drm_free(ttm->vma_list, sizeof(*ttm->vma_list), DRM_MEM_TTM);
		ttm->vma_list = NULL;
	}

	drm_free(ttm, sizeof(*ttm), DRM_MEM_TTM);

	return 0;
}

/*
 * Initialize a ttm.
 * FIXME: Avoid using vmalloc for the page- and page_flags tables?
 */

static drm_ttm_t *drm_init_ttm(struct drm_device * dev, unsigned long size)
{

	drm_ttm_t *ttm;

	if (!dev->driver->bo_driver)
		return NULL;

	ttm = drm_calloc(1, sizeof(*ttm), DRM_MEM_TTM);
	if (!ttm)
		return NULL;

	ttm->lhandle = 0;
	atomic_set(&ttm->vma_count, 0);

	ttm->destroy = 0;
	ttm->num_pages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;

	ttm->page_flags = vmalloc(ttm->num_pages * sizeof(*ttm->page_flags));
	if (!ttm->page_flags) {
		drm_destroy_ttm(ttm);
		DRM_ERROR("Failed allocating page_flags table\n");
		return NULL;
	}
	memset(ttm->page_flags, 0, ttm->num_pages * sizeof(*ttm->page_flags));

	ttm->pages = vmalloc(ttm->num_pages * sizeof(*ttm->pages));
	if (!ttm->pages) {
		drm_destroy_ttm(ttm);
		DRM_ERROR("Failed allocating page table\n");
		return NULL;
	}
	memset(ttm->pages, 0, ttm->num_pages * sizeof(*ttm->pages));

	ttm->be_list = drm_calloc(1, sizeof(*ttm->be_list), DRM_MEM_TTM);
	if (!ttm->be_list) {
		DRM_ERROR("Alloc be regions failed\n");
		drm_destroy_ttm(ttm);
		return NULL;
	}

	INIT_LIST_HEAD(&ttm->be_list->head);
	INIT_LIST_HEAD(&ttm->p_mm_list);
	atomic_set(&ttm->shared_count, 0);
	ttm->mm_list_seq = 0;

	ttm->vma_list = drm_calloc(1, sizeof(*ttm->vma_list), DRM_MEM_TTM);
	if (!ttm->vma_list) {
		DRM_ERROR("Alloc vma list failed\n");
		drm_destroy_ttm(ttm);
		return NULL;
	}

	INIT_LIST_HEAD(&ttm->vma_list->head);

	ttm->lhandle = (unsigned long)ttm;
	ttm->dev = dev;

	return ttm;
}

/*
 * Lock the mmap_sems for processes that are mapping this ttm. 
 * This looks a bit clumsy, since we need to maintain the correct
 * locking order
 * mm->mmap_sem
 * dev->struct_sem;
 * and while we release dev->struct_sem to lock the mmap_sems, 
 * the mmap_sem list may have been updated. We need to revalidate
 * it after relocking dev->struc_sem.
 */

static int drm_ttm_lock_mmap_sem(drm_ttm_t * ttm)
{
	struct mm_struct **mm_list = NULL, **mm_list_p;
	uint32_t list_seq;
	uint32_t cur_count, shared_count;
	p_mm_entry_t *entry;
	unsigned i;

	cur_count = 0;
	list_seq = ttm->mm_list_seq;
	shared_count = atomic_read(&ttm->shared_count);

	do {
		if (shared_count > cur_count) {
			if (mm_list)
				drm_free(mm_list, sizeof(*mm_list) * cur_count,
					 DRM_MEM_TTM);
			cur_count = shared_count + 10;
			mm_list =
			    drm_alloc(sizeof(*mm_list) * cur_count, DRM_MEM_TTM);
			if (!mm_list)
				return -ENOMEM;
		}

		mm_list_p = mm_list;
		list_for_each_entry(entry, &ttm->p_mm_list, head) {
			*mm_list_p++ = entry->mm;
		}

		mutex_unlock(&ttm->dev->struct_mutex);
		mm_list_p = mm_list;
		for (i = 0; i < shared_count; ++i, ++mm_list_p) {
			down_write(&((*mm_list_p)->mmap_sem));
		}

		mutex_lock(&ttm->dev->struct_mutex);

		if (list_seq != ttm->mm_list_seq) {
			mm_list_p = mm_list;
			for (i = 0; i < shared_count; ++i, ++mm_list_p) {
				up_write(&((*mm_list_p)->mmap_sem));
			}

		}
		shared_count = atomic_read(&ttm->shared_count);

	} while (list_seq != ttm->mm_list_seq);

	if (mm_list)
		drm_free(mm_list, sizeof(*mm_list) * cur_count, DRM_MEM_TTM);

	return 0;
}

/*
 * Change caching policy for range of pages in a ttm.
 */

static int drm_set_caching(drm_ttm_t * ttm, unsigned long page_offset,
			   unsigned long num_pages, int noncached,
			   int do_tlbflush)
{
	int i, cur;
	struct page **cur_page;
	pgprot_t attr = (noncached) ? PAGE_KERNEL_NOCACHE : PAGE_KERNEL;

	for (i = 0; i < num_pages; ++i) {
		cur = page_offset + i;
		cur_page = ttm->pages + cur;
		if (*cur_page) {
			if (PageHighMem(*cur_page)) {
				if (noncached
				    && page_address(*cur_page) != NULL) {
					DRM_ERROR
					    ("Illegal mapped HighMem Page\n");
					return -EINVAL;
				}
			} else if ((ttm->page_flags[cur] &
				    DRM_TTM_PAGE_UNCACHED) != noncached) {
				DRM_MASK_VAL(ttm->page_flags[cur],
					     DRM_TTM_PAGE_UNCACHED, noncached);
				change_page_attr(*cur_page, 1, attr);
			}
		}
	}
	if (do_tlbflush)
		global_flush_tlb();
	return 0;
}

/*
 * Unbind a ttm region from the aperture.
 */

int drm_evict_ttm_region(drm_ttm_backend_list_t * entry)
{
	drm_ttm_backend_t *be = entry->be;
	drm_ttm_t *ttm = entry->owner;
	int ret;

	if (be) {
		switch (entry->state) {
		case ttm_bound:
			if (ttm && be->needs_cache_adjust(be)) {
				ret = drm_ttm_lock_mmap_sem(ttm);
				if (ret)
					return ret;
				drm_ttm_lock_mm(ttm, 0, 1);
				unmap_vma_pages(ttm, entry->page_offset,
						entry->num_pages);
				drm_ttm_unlock_mm(ttm, 0, 1);
			}
			be->unbind(entry->be);
			if (ttm && be->needs_cache_adjust(be)) {
				drm_set_caching(ttm, entry->page_offset,
						entry->num_pages, 0, 1);
				drm_ttm_unlock_mm(ttm, 1, 0);
			}
			break;
		default:
			break;
		}
	}
	entry->state = ttm_evicted;
	return 0;
}

void drm_unbind_ttm_region(drm_ttm_backend_list_t * entry)
{
	drm_evict_ttm_region(entry);
	entry->state = ttm_unbound;
}

/*
 * Destroy and clean up all resources associated with a ttm region.
 * FIXME: release pages to OS when doing this operation.
 */

void drm_destroy_ttm_region(drm_ttm_backend_list_t * entry)
{
	drm_ttm_backend_t *be = entry->be;
	drm_ttm_t *ttm = entry->owner;
	uint32_t *cur_page_flags;
	int i;

	list_del_init(&entry->head);

	drm_unbind_ttm_region(entry);
	if (be) {
		be->clear(entry->be);
		if (be->needs_cache_adjust(be)) {
			int ret = drm_ttm_lock_mmap_sem(ttm);
			drm_ttm_lock_mm(ttm, 0, 1);
			unmap_vma_pages(ttm, entry->page_offset, 
					entry->num_pages);
			drm_ttm_unlock_mm(ttm, 0, 1);
			drm_set_caching(ttm, entry->page_offset,
					entry->num_pages, 0, 1);
			if (!ret)
				drm_ttm_unlock_mm(ttm, 1, 0);
		}
		be->destroy(be);
	}
	cur_page_flags = ttm->page_flags + entry->page_offset;
	for (i = 0; i < entry->num_pages; ++i) {
		DRM_MASK_VAL(*cur_page_flags, DRM_TTM_PAGE_USED, 0);
		cur_page_flags++;
	}

	drm_free(entry, sizeof(*entry), DRM_MEM_TTM);
}

/*
 * Create a ttm region from a range of ttm pages.
 */

int drm_create_ttm_region(drm_ttm_t * ttm, unsigned long page_offset,
			  unsigned long n_pages, int cached,
			  drm_ttm_backend_list_t ** region)
{
	struct page **cur_page;
	uint32_t *cur_page_flags;
	drm_ttm_backend_list_t *entry;
	drm_ttm_backend_t *be;
	int ret, i;

	if ((page_offset + n_pages) > ttm->num_pages || n_pages == 0) {
		DRM_ERROR("Region Doesn't fit ttm\n");
		return -EINVAL;
	}

	cur_page_flags = ttm->page_flags + page_offset;
	for (i = 0; i < n_pages; ++i, ++cur_page_flags) {
		if (*cur_page_flags & DRM_TTM_PAGE_USED) {
			DRM_ERROR("TTM region overlap\n");
			return -EINVAL;
		} else {
			DRM_MASK_VAL(*cur_page_flags, DRM_TTM_PAGE_USED,
				     DRM_TTM_PAGE_USED);
		}
	}

	entry = drm_calloc(1, sizeof(*entry), DRM_MEM_TTM);
	if (!entry)
		return -ENOMEM;

	be = ttm->dev->driver->bo_driver->create_ttm_backend_entry(ttm->dev, cached);
	if (!be) {
		drm_free(entry, sizeof(*entry), DRM_MEM_TTM);
		DRM_ERROR("Couldn't create backend.\n");
		return -EINVAL;
	}
	entry->state = ttm_unbound;
	entry->page_offset = page_offset;
	entry->num_pages = n_pages;
	entry->be = be;
	entry->owner = ttm;

	INIT_LIST_HEAD(&entry->head);
	list_add_tail(&entry->head, &ttm->be_list->head);

	for (i = 0; i < entry->num_pages; ++i) {
		cur_page = ttm->pages + (page_offset + i);
		if (!*cur_page) {
			*cur_page = alloc_page(GFP_KERNEL);
			if (!*cur_page) {
				DRM_ERROR("Page allocation failed\n");
				drm_destroy_ttm_region(entry);
				return -ENOMEM;
			}
			SetPageReserved(*cur_page);
		}
	}

	if ((ret = be->populate(be, n_pages, ttm->pages + page_offset))) {
		drm_destroy_ttm_region(entry);
		DRM_ERROR("Couldn't populate backend.\n");
		return ret;
	}
	ttm->aperture_base = be->aperture_base;

	*region = entry;
	return 0;
}

/*
 * Bind a ttm region. Set correct caching policy.
 */

int drm_bind_ttm_region(drm_ttm_backend_list_t * region,
			unsigned long aper_offset)
{

	int i;
	uint32_t *cur_page_flag;
	int ret = 0;
	drm_ttm_backend_t *be;
	drm_ttm_t *ttm;

	if (!region || region->state == ttm_bound)
		return -EINVAL;

	be = region->be;
	ttm = region->owner;

	if (ttm && be->needs_cache_adjust(be)) {
		ret = drm_ttm_lock_mmap_sem(ttm);
		if (ret)
			return ret;
		drm_set_caching(ttm, region->page_offset, region->num_pages,
				DRM_TTM_PAGE_UNCACHED, 1);
	} else {
		DRM_DEBUG("Binding cached\n");
	}

	if ((ret = be->bind(be, aper_offset))) {
		if (ttm && be->needs_cache_adjust(be))
			drm_ttm_unlock_mm(ttm, 1, 0);
		drm_unbind_ttm_region(region);
		DRM_ERROR("Couldn't bind backend.\n");
		return ret;
	}

	cur_page_flag = ttm->page_flags + region->page_offset;
	for (i = 0; i < region->num_pages; ++i) {
		DRM_MASK_VAL(*cur_page_flag, DRM_TTM_MASK_PFN,
			     (i + aper_offset) << PAGE_SHIFT);
		cur_page_flag++;
	}

	if (ttm && be->needs_cache_adjust(be)) {
		ioremap_vmas(ttm, region->page_offset, region->num_pages,
			     aper_offset);
		drm_ttm_unlock_mm(ttm, 1, 0);
	}

	region->state = ttm_bound;
	return 0;
}

int drm_rebind_ttm_region(drm_ttm_backend_list_t * entry,
			  unsigned long aper_offset)
{
	return drm_bind_ttm_region(entry, aper_offset);

}

/*
 * Destroy an anonymous ttm region.
 */

void drm_user_destroy_region(drm_ttm_backend_list_t * entry)
{
	drm_ttm_backend_t *be;
	struct page **cur_page;
	int i;

	if (!entry || entry->owner)
		return;

	be = entry->be;
	if (!be) {
		drm_free(entry, sizeof(*entry), DRM_MEM_TTM);
		return;
	}

	be->unbind(be);

	if (entry->anon_pages) {
		cur_page = entry->anon_pages;
		for (i = 0; i < entry->anon_locked; ++i) {
			if (!PageReserved(*cur_page))
				SetPageDirty(*cur_page);
			page_cache_release(*cur_page);
			cur_page++;
		}
		vfree(entry->anon_pages);
	}

	be->destroy(be);
	drm_free(entry, sizeof(*entry), DRM_MEM_TTM);
	return;
}

/*
 * Create a ttm region from an arbitrary region of user pages.
 * Since this region has no backing ttm, it's owner is set to
 * null, and it is registered with the file of the caller.
 * Gets destroyed when the file is closed. We call this an
 * anonymous ttm region.
 */

int drm_user_create_region(drm_device_t * dev, unsigned long start, int len,
			   drm_ttm_backend_list_t ** entry)
{
	drm_ttm_backend_list_t *tmp;
	drm_ttm_backend_t *be;
	int ret;

	if (len <= 0)
		return -EINVAL;
	if (!dev->driver->bo_driver->create_ttm_backend_entry)
		return -EFAULT;

	tmp = drm_calloc(1, sizeof(*tmp), DRM_MEM_TTM);

	if (!tmp)
		return -ENOMEM;

	be = dev->driver->bo_driver->create_ttm_backend_entry(dev, 1);
	tmp->be = be;

	if (!be) {
		drm_user_destroy_region(tmp);
		return -ENOMEM;
	}
	if (be->needs_cache_adjust(be)) {
		drm_user_destroy_region(tmp);
		return -EFAULT;
	}

	tmp->anon_pages = vmalloc(sizeof(*(tmp->anon_pages)) * len);

	if (!tmp->anon_pages) {
		drm_user_destroy_region(tmp);
		return -ENOMEM;
	}

	down_read(&current->mm->mmap_sem);
	ret = get_user_pages(current, current->mm, start, len, 1, 0,
			     tmp->anon_pages, NULL);
	up_read(&current->mm->mmap_sem);

	if (ret != len) {
		drm_user_destroy_region(tmp);
		DRM_ERROR("Could not lock %d pages. Return code was %d\n",
			  len, ret);
		return -EPERM;
	}
	tmp->anon_locked = len;

	ret = be->populate(be, len, tmp->anon_pages);

	if (ret) {
		drm_user_destroy_region(tmp);
		return ret;
	}

	tmp->state = ttm_unbound;
	*entry = tmp;

	return 0;
}


/*
 * dev->struct_mutex locked.
 */

static void drm_ttm_object_remove(drm_device_t *dev, drm_ttm_object_t *object)
{
	drm_map_list_t *list = &object->map_list;
	drm_local_map_t *map;

	if (list->user_token)
		drm_ht_remove_item(&dev->map_hash, &list->hash);

	map = list->map;

	if (map) {
		drm_ttm_t *ttm = (drm_ttm_t *)map->offset;
		if (ttm) {
			if (drm_destroy_ttm(ttm) != -EBUSY) {
				drm_free(map, sizeof(*map), DRM_MEM_TTM);
			}
		} else {
			drm_free(map, sizeof(*map), DRM_MEM_TTM);
		}
	}

	drm_free(object, sizeof(*object), DRM_MEM_TTM);
}

/*
 * dev->struct_mutex locked.
 */
static void drm_ttm_user_object_remove(drm_file_t *priv, drm_user_object_t *base)
{
	drm_ttm_object_t *object;
	drm_device_t *dev = priv->head->dev;

	object = drm_user_object_entry(base, drm_ttm_object_t, base);
	if (atomic_dec_and_test(&object->usage)) 
		drm_ttm_object_remove(dev, object);
}

	

/*
 * Create a ttm and add it to the drm book-keeping. 
 * dev->struct_mutex locked.
 */

int drm_ttm_object_create(drm_device_t *dev, unsigned long size, 
			  uint32_t flags, drm_ttm_object_t **ttm_object)
{
	drm_ttm_object_t *object;
	drm_map_list_t *list;
	drm_map_t *map;
	drm_ttm_t *ttm;

	object = drm_calloc(1, sizeof(*object), DRM_MEM_TTM);
	if (!object) 
		return -ENOMEM;
	object->flags = flags;
	list = &object->map_list;
	
	list->map = drm_calloc(1, sizeof(*map), DRM_MEM_TTM);
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

	map->offset = ttm->lhandle;
	map->type = _DRM_TTM;
	map->flags = _DRM_REMOVABLE;
	map->size = size;
	map->handle = (void *)object;
		
	if (drm_ht_just_insert_please(&dev->map_hash, &list->hash, 
				      (unsigned long) map->handle, 
				      32 - PAGE_SHIFT - 3, PAGE_SHIFT,
				      DRM_MAP_HASH_OFFSET)) {
		drm_ttm_object_remove(dev, object);
		return -ENOMEM;
	}

	list->user_token = list->hash.key;
	object->base.remove = drm_ttm_user_object_remove;
	object->base.type = drm_ttm_type;
	object->base.ref_struct_locked = NULL;
	object->base.unref = NULL;

	atomic_set(&object->usage, 1);
	return 0;
}


int drm_ttm_ioctl(drm_file_t *priv, drm_ttm_arg_t __user *data)
{
	drm_ttm_arg_t arg;
	drm_device_t *dev = priv->head->dev;
	drm_ttm_object_t *entry;
	drm_user_object_t *uo;
	unsigned long size;
	int ret;

	DRM_COPY_FROM_USER_IOCTL(arg, (void __user *)data, sizeof(arg));
	
	switch(arg.op) {
	case drm_ttm_create:
		mutex_lock(&dev->struct_mutex);
		size = combine_64(arg.size_lo, arg.size_hi);
		ret = drm_ttm_object_create(dev, size, arg.flags, &entry);
		if (ret) {
			mutex_unlock(&dev->struct_mutex);
			return ret;
		}
		ret = drm_add_user_object(priv, &entry->base, 
					  arg.flags & DRM_TTM_FLAG_SHAREABLE);
		if (ret) {
			drm_ttm_object_remove(dev, entry);
			mutex_unlock(&dev->struct_mutex);
			return ret;
		}
		arg.handle = entry->base.hash.key;
		arg.user_token = entry->map_list.user_token;
		mutex_unlock(&dev->struct_mutex);
		break;
	case drm_ttm_reference:
		ret = drm_user_object_ref(priv, arg.handle, drm_ttm_type, &uo);
		if (ret)
			return ret;
		mutex_lock(&dev->struct_mutex);
		uo = drm_lookup_user_object(priv, arg.handle);
		entry = drm_user_object_entry(uo, drm_ttm_object_t, base);
		arg.user_token = entry->map_list.user_token;
		mutex_unlock(&dev->struct_mutex);
		break;
	case drm_ttm_unreference:
		return drm_user_object_unref(priv, arg.handle, drm_ttm_type);
	case drm_ttm_destroy:
		mutex_lock(&dev->struct_mutex);
		uo = drm_lookup_user_object(priv, arg.handle);
		if (!uo || (uo->type != drm_ttm_type) || uo->owner != priv) {
			mutex_unlock(&dev->struct_mutex);
			return -EINVAL;
		}
		ret = drm_remove_user_object(priv, uo);
		mutex_unlock(&dev->struct_mutex);
		return ret;
	}
	DRM_COPY_TO_USER_IOCTL((void __user *)data, arg, sizeof(arg));
	return 0;
}
