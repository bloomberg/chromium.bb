/*
 * Copyright (c) Red Hat Inc.

 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors: Dave Airlie <airlied@redhat.com>
 */

/* simple list based uncached page allocator 
 * - Add chunks of 1MB to the allocator at a time.
 * - Use page->lru to keep a free list
 * - doesn't track currently in use pages
 * 
 *  TODO: Add shrinker support
 */

#include "drmP.h"
#include <asm/agp.h>

static struct list_head uncached_free_list;

static struct mutex uncached_mutex;
static int uncached_inited;
static int total_uncached_pages;

/* add 1MB at a time */
#define NUM_PAGES_TO_ADD 256

static void drm_uncached_page_put(struct page *page)
{
	unmap_page_from_agp(page);
	put_page(page);
	__free_page(page);
}

int drm_uncached_add_pages_locked(int num_pages)
{
	struct page *page;
	int i;

	DRM_DEBUG("adding uncached memory %ld\n", num_pages * PAGE_SIZE);
	for (i = 0; i < num_pages; i++) {

		page = alloc_page(GFP_KERNEL | __GFP_ZERO | GFP_DMA32);
		if (!page) {
			DRM_ERROR("unable to get page %d\n", i);
			return i;
		}

		get_page(page);
#ifdef CONFIG_X86
		set_memory_wc((unsigned long)page_address(page), 1);
#else
		map_page_into_agp(page);
#endif

		list_add(&page->lru, &uncached_free_list);
		total_uncached_pages++;
	}
	return i;
}

struct page *drm_get_uncached_page(void)
{
	struct page *page = NULL;
	int ret;

	mutex_lock(&uncached_mutex);
	if (list_empty(&uncached_free_list)) {
		ret = drm_uncached_add_pages_locked(NUM_PAGES_TO_ADD);
		if (ret == 0)
			return NULL;
	}

	page = list_first_entry(&uncached_free_list, struct page, lru);
	list_del(&page->lru);

	mutex_unlock(&uncached_mutex);
	return page;
}

void drm_put_uncached_page(struct page *page)
{
	mutex_lock(&uncached_mutex);
	list_add(&page->lru, &uncached_free_list);
	mutex_unlock(&uncached_mutex);
}

void drm_uncached_release_all_pages(void)
{
	struct page *page, *tmp;

	list_for_each_entry_safe(page, tmp, &uncached_free_list, lru) {
		list_del(&page->lru);
		drm_uncached_page_put(page);
	}
}

int drm_uncached_init(void)
{

	if (uncached_inited)
		return 0;

	INIT_LIST_HEAD(&uncached_free_list);

	mutex_init(&uncached_mutex);
	uncached_inited = 1;
	return 0;

}

void drm_uncached_fini(void)
{
	if (!uncached_inited)
		return;
	
	uncached_inited = 0;
	drm_uncached_release_all_pages();
}

