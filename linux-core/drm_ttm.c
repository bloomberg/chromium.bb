/**************************************************************************
 *
 * Copyright (c) 2006-2007 Tungsten Graphics, Inc., Cedar Park, TX., USA
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
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
/*
 * Authors: Thomas Hellström <thomas-at-tungstengraphics-dot-com>
 */

#include "drmP.h"

#if defined( CONFIG_X86 ) && (LINUX_VERSION_CODE >= (2,6,24))
static void drm_clflush_page(struct page *page)
{
	uint8_t *page_virtual;
	unsigned int i;

	if (unlikely(page == NULL))
		return;

	page_virtual = kmap_atomic(page, KM_USER0);

	for (i=0; i < PAGE_SIZE; i += boot_cpu_data.x86_clflush_size)
		clflush(page_virtual + i);

	kunmap_atomic(page_virtual, KM_USER0);
}

static void drm_ttm_cache_flush_clflush(struct page *pages[], unsigned long num_pages)
{
	unsigned long i;

	mb();
	for (i=0; i < num_pages; ++i)
		drm_clflush_page(*pages++);
	mb();
}
#endif

static void drm_ttm_ipi_handler(void *null)
{
	flush_agp_cache();
}

void drm_ttm_cache_flush(struct page *pages[], unsigned long num_pages)
{

#if defined( CONFIG_X86 ) && (LINUX_VERSION_CODE >= (2,6,24))
	if (cpu_has_clflush) {
		drm_ttm_cache_flush_clflush(pages, num_pages);
		return;
	}
#endif
	if (on_each_cpu(drm_ttm_ipi_handler, NULL, 1, 1) != 0)
		DRM_ERROR("Timed out waiting for drm cache flush.\n");
}
EXPORT_SYMBOL(drm_ttm_cache_flush);

/**
 * Allocates storage for pointers to the pages that back the ttm.
 *
 * Uses kmalloc if possible. Otherwise falls back to vmalloc.
 */
static void drm_ttm_alloc_page_directory(struct drm_ttm *ttm)
{
	unsigned long size = ttm->num_pages * sizeof(*ttm->pages);
	ttm->pages = NULL;

	if (drm_alloc_memctl(size))
		return;

	if (size <= PAGE_SIZE)
		ttm->pages = drm_calloc(1, size, DRM_MEM_TTM);

	if (!ttm->pages) {
		ttm->pages = vmalloc_user(size);
		if (ttm->pages)
			ttm->page_flags |= DRM_TTM_PAGEDIR_VMALLOC;
	}
	if (!ttm->pages)
		drm_free_memctl(size);
}

static void drm_ttm_free_page_directory(struct drm_ttm *ttm)
{
	unsigned long size = ttm->num_pages * sizeof(*ttm->pages);

	if (ttm->page_flags & DRM_TTM_PAGEDIR_VMALLOC) {
		vfree(ttm->pages);
		ttm->page_flags &= ~DRM_TTM_PAGEDIR_VMALLOC;
	} else {
		drm_free(ttm->pages, size, DRM_MEM_TTM);
	}
	drm_free_memctl(size);
	ttm->pages = NULL;
}

static struct page *drm_ttm_alloc_page(void)
{
	struct page *page;

	if (drm_alloc_memctl(PAGE_SIZE))
		return NULL;

	page = alloc_page(GFP_KERNEL | __GFP_ZERO | GFP_DMA32);
	if (!page) {
		drm_free_memctl(PAGE_SIZE);
		return NULL;
	}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15))
	SetPageReserved(page);
#endif
	return page;
}

/*
 * Change caching policy for the linear kernel map
 * for range of pages in a ttm.
 */

static int drm_ttm_set_caching(struct drm_ttm *ttm, int noncached)
{
	int i;
	struct page **cur_page;
	int do_tlbflush = 0;

	if ((ttm->page_flags & DRM_TTM_PAGE_UNCACHED) == noncached)
		return 0;

	if (noncached)
		drm_ttm_cache_flush(ttm->pages, ttm->num_pages);

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

	DRM_FLAG_MASKED(ttm->page_flags, noncached, DRM_TTM_PAGE_UNCACHED);

	return 0;
}


static void drm_ttm_free_user_pages(struct drm_ttm *ttm)
{
	int write;
	int dirty;
	struct page *page;
	int i;

	BUG_ON(!(ttm->page_flags & DRM_TTM_PAGE_USER));
	write = ((ttm->page_flags & DRM_TTM_PAGE_WRITE) != 0);
	dirty = ((ttm->page_flags & DRM_TTM_PAGE_USER_DIRTY) != 0);

	for (i = 0; i < ttm->num_pages; ++i) {
		page = ttm->pages[i];
		if (page == NULL)
			continue;

		if (page == ttm->dummy_read_page) {
			BUG_ON(write);
			continue;
		}

		if (write && dirty && !PageReserved(page))
			set_page_dirty_lock(page);

		ttm->pages[i] = NULL;
		put_page(page);
	}
}

static void drm_ttm_free_alloced_pages(struct drm_ttm *ttm)
{
	int i;
	struct drm_buffer_manager *bm = &ttm->dev->bm;
	struct page **cur_page;

	for (i = 0; i < ttm->num_pages; ++i) {
		cur_page = ttm->pages + i;
		if (*cur_page) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15))
			ClearPageReserved(*cur_page);
#endif
			if (page_count(*cur_page) != 1)
				DRM_ERROR("Erroneous page count. Leaking pages.\n");
			if (page_mapped(*cur_page))
				DRM_ERROR("Erroneous map count. Leaking page mappings.\n");
			__free_page(*cur_page);
			drm_free_memctl(PAGE_SIZE);
			--bm->cur_pages;
		}
	}
}

/*
 * Free all resources associated with a ttm.
 */

int drm_ttm_destroy(struct drm_ttm *ttm)
{
	struct drm_ttm_backend *be;

	if (!ttm)
		return 0;

	be = ttm->be;
	if (be) {
		be->func->destroy(be);
		ttm->be = NULL;
	}

	if (ttm->pages) {
		if (ttm->page_flags & DRM_TTM_PAGE_UNCACHED)
			drm_ttm_set_caching(ttm, 0);

		if (ttm->page_flags & DRM_TTM_PAGE_USER)
			drm_ttm_free_user_pages(ttm);
		else
			drm_ttm_free_alloced_pages(ttm);

		drm_ttm_free_page_directory(ttm);
	}

	drm_ctl_free(ttm, sizeof(*ttm), DRM_MEM_TTM);
	return 0;
}

struct page *drm_ttm_get_page(struct drm_ttm *ttm, int index)
{
	struct page *p;
	struct drm_buffer_manager *bm = &ttm->dev->bm;

	p = ttm->pages[index];
	if (!p) {
		p = drm_ttm_alloc_page();
		if (!p)
			return NULL;
		ttm->pages[index] = p;
		++bm->cur_pages;
	}
	return p;
}
EXPORT_SYMBOL(drm_ttm_get_page);

/**
 * drm_ttm_set_user:
 *
 * @ttm: the ttm to map pages to. This must always be
 * a freshly created ttm.
 *
 * @tsk: a pointer to the address space from which to map
 * pages.
 * 
 * @write: a boolean indicating that write access is desired
 *
 * start: the starting address
 *
 * Map a range of user addresses to a new ttm object. This
 * provides access to user memory from the graphics device.
 */
int drm_ttm_set_user(struct drm_ttm *ttm,
		     struct task_struct *tsk,
		     unsigned long start,
		     unsigned long num_pages)
{
	struct mm_struct *mm = tsk->mm;
	int ret;
	int write = (ttm->page_flags & DRM_TTM_PAGE_WRITE) != 0;

	BUG_ON(num_pages != ttm->num_pages);
	BUG_ON((ttm->page_flags & DRM_TTM_PAGE_USER) == 0);

	down_read(&mm->mmap_sem);
	ret = get_user_pages(tsk, mm, start, num_pages,
			     write, 0, ttm->pages, NULL);
	up_read(&mm->mmap_sem);

	if (ret != num_pages && write) {
		drm_ttm_free_user_pages(ttm);
		return -ENOMEM;
	}

	return 0;
}

/**
 * drm_ttm_populate:
 *
 * @ttm: the object to allocate pages for
 *
 * Allocate pages for all unset page entries, then
 * call the backend to create the hardware mappings
 */
int drm_ttm_populate(struct drm_ttm *ttm)
{
	struct page *page;
	unsigned long i;
	struct drm_ttm_backend *be;

	if (ttm->state != ttm_unpopulated)
		return 0;

	be = ttm->be;

	for (i = 0; i < ttm->num_pages; ++i) {
		page = drm_ttm_get_page(ttm, i);
		if (!page)
			return -ENOMEM;
	}

	be->func->populate(be, ttm->num_pages, ttm->pages, ttm->dummy_read_page);
	ttm->state = ttm_unbound;
	return 0;
}

/**
 * drm_ttm_create:
 *
 * @dev: the drm_device
 *
 * @size: The size (in bytes) of the desired object
 *
 * @page_flags: various DRM_TTM_PAGE_* flags. See drm_object.h.
 *
 * Allocate and initialize a ttm, leaving it unpopulated at this time
 */

struct drm_ttm *drm_ttm_create(struct drm_device *dev, unsigned long size,
			       uint32_t page_flags, struct page *dummy_read_page)
{
	struct drm_bo_driver *bo_driver = dev->driver->bo_driver;
	struct drm_ttm *ttm;

	if (!bo_driver)
		return NULL;

	ttm = drm_ctl_calloc(1, sizeof(*ttm), DRM_MEM_TTM);
	if (!ttm)
		return NULL;

	ttm->dev = dev;
	atomic_set(&ttm->vma_count, 0);

	ttm->destroy = 0;
	ttm->num_pages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;

	ttm->page_flags = page_flags;

	ttm->dummy_read_page = dummy_read_page;

	/*
	 * Account also for AGP module memory usage.
	 */

	drm_ttm_alloc_page_directory(ttm);
	if (!ttm->pages) {
		drm_ttm_destroy(ttm);
		DRM_ERROR("Failed allocating page table\n");
		return NULL;
	}
	ttm->be = bo_driver->create_ttm_backend_entry(dev);
	if (!ttm->be) {
		drm_ttm_destroy(ttm);
		DRM_ERROR("Failed creating ttm backend entry\n");
		return NULL;
	}
	ttm->state = ttm_unpopulated;
	return ttm;
}

/**
 * drm_ttm_evict:
 *
 * @ttm: the object to be unbound from the aperture.
 *
 * Transition a ttm from bound to evicted, where it
 * isn't present in the aperture, but various caches may
 * not be consistent.
 */
void drm_ttm_evict(struct drm_ttm *ttm)
{
	struct drm_ttm_backend *be = ttm->be;
	int ret;

	if (ttm->state == ttm_bound) {
		ret = be->func->unbind(be);
		BUG_ON(ret);
	}

	ttm->state = ttm_evicted;
}

/**
 * drm_ttm_fixup_caching:
 *
 * @ttm: the object to set unbound
 *
 * XXX this function is misnamed. Transition a ttm from evicted to
 * unbound, flushing caches as appropriate.
 */
void drm_ttm_fixup_caching(struct drm_ttm *ttm)
{

	if (ttm->state == ttm_evicted) {
		struct drm_ttm_backend *be = ttm->be;
		if (be->func->needs_ub_cache_adjust(be))
			drm_ttm_set_caching(ttm, 0);
		ttm->state = ttm_unbound;
	}
}

/**
 * drm_ttm_unbind:
 *
 * @ttm: the object to unbind from the graphics device
 *
 * Unbind an object from the aperture. This removes the mappings
 * from the graphics device and flushes caches if necessary.
 */
void drm_ttm_unbind(struct drm_ttm *ttm)
{
	if (ttm->state == ttm_bound)
		drm_ttm_evict(ttm);

	drm_ttm_fixup_caching(ttm);
}

/**
 * drm_ttm_bind:
 *
 * @ttm: the ttm object to bind to the graphics device
 *
 * @bo_mem: the aperture memory region which will hold the object
 *
 * Bind a ttm object to the aperture. This ensures that the necessary
 * pages are allocated, flushes CPU caches as needed and marks the
 * ttm as DRM_TTM_PAGE_USER_DIRTY to indicate that it may have been
 * modified by the GPU
 */
int drm_ttm_bind(struct drm_ttm *ttm, struct drm_bo_mem_reg *bo_mem)
{
	struct drm_bo_driver *bo_driver = ttm->dev->driver->bo_driver;
	int ret = 0;
	struct drm_ttm_backend *be;

	if (!ttm)
		return -EINVAL;
	if (ttm->state == ttm_bound)
		return 0;

	be = ttm->be;

	ret = drm_ttm_populate(ttm);
	if (ret)
		return ret;

	if (ttm->state == ttm_unbound && !(bo_mem->flags & DRM_BO_FLAG_CACHED))
		drm_ttm_set_caching(ttm, DRM_TTM_PAGE_UNCACHED);
	else if ((bo_mem->flags & DRM_BO_FLAG_CACHED_MAPPED) &&
		   bo_driver->ttm_cache_flush)
		bo_driver->ttm_cache_flush(ttm);

	ret = be->func->bind(be, bo_mem);
	if (ret) {
		ttm->state = ttm_evicted;
		DRM_ERROR("Couldn't bind backend.\n");
		return ret;
	}

	ttm->state = ttm_bound;
	if (ttm->page_flags & DRM_TTM_PAGE_USER)
		ttm->page_flags |= DRM_TTM_PAGE_USER_DIRTY;
	return 0;
}
EXPORT_SYMBOL(drm_ttm_bind);
