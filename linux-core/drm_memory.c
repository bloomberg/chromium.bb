/**
 * \file drm_memory.c
 * Memory management wrappers for DRM
 *
 * \author Rickard E. (Rik) Faith <faith@valinux.com>
 * \author Gareth Hughes <gareth@valinux.com>
 */

/*
 * Created: Thu Feb  4 14:00:34 1999 by faith@valinux.com
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

#include <linux/highmem.h>
#include "drmP.h"

static struct {
	spinlock_t lock;
	uint64_t cur_used;
	uint64_t emer_used;
	uint64_t low_threshold;
	uint64_t high_threshold;
	uint64_t emer_threshold;
} drm_memctl = {
	.lock = SPIN_LOCK_UNLOCKED
};

static inline size_t drm_size_align(size_t size)
{
	size_t tmpSize = 4;
	if (size > PAGE_SIZE)
		return PAGE_ALIGN(size);

	while (tmpSize < size)
		tmpSize <<= 1;

	return (size_t) tmpSize;
}

int drm_alloc_memctl(size_t size)
{
        int ret = 0;
	unsigned long a_size = drm_size_align(size);
	unsigned long new_used;

	spin_lock(&drm_memctl.lock);
	new_used = drm_memctl.cur_used + a_size;
	if (likely(new_used < drm_memctl.high_threshold)) {
		drm_memctl.cur_used = new_used;
		goto out;
	}

	/*
	 * Allow small allocations from root-only processes to
	 * succeed until the emergency threshold is reached.
	 */

	new_used += drm_memctl.emer_used;
	if (unlikely(!DRM_SUSER(DRM_CURPROC) ||
		     (a_size > 16*PAGE_SIZE) ||
		     (new_used > drm_memctl.emer_threshold))) {
		ret = -ENOMEM;
		goto out;
	}

	drm_memctl.cur_used = drm_memctl.high_threshold;
	drm_memctl.emer_used = new_used - drm_memctl.high_threshold;
out:
	spin_unlock(&drm_memctl.lock);
	return ret;
}
EXPORT_SYMBOL(drm_alloc_memctl);


void drm_free_memctl(size_t size)
{
	unsigned long a_size = drm_size_align(size);

	spin_lock(&drm_memctl.lock);
	if (likely(a_size >= drm_memctl.emer_used)) {
		a_size -= drm_memctl.emer_used;
		drm_memctl.emer_used = 0;
	} else {
		drm_memctl.emer_used -= a_size;
		a_size = 0;
	}
	drm_memctl.cur_used -= a_size;
	spin_unlock(&drm_memctl.lock);
}
EXPORT_SYMBOL(drm_free_memctl);

void drm_query_memctl(uint64_t *cur_used,
		      uint64_t *emer_used,
		      uint64_t *low_threshold,
		      uint64_t *high_threshold,
		      uint64_t *emer_threshold)
{
	spin_lock(&drm_memctl.lock);
	*cur_used = drm_memctl.cur_used;
	*emer_used = drm_memctl.emer_used;
	*low_threshold = drm_memctl.low_threshold;
	*high_threshold = drm_memctl.high_threshold;
	*emer_threshold = drm_memctl.emer_threshold;
	spin_unlock(&drm_memctl.lock);
}
EXPORT_SYMBOL(drm_query_memctl);

void drm_init_memctl(size_t p_low_threshold,
		     size_t p_high_threshold,
		     size_t unit_size)
{
	spin_lock(&drm_memctl.lock);
	drm_memctl.emer_used = 0;
	drm_memctl.cur_used = 0;
	drm_memctl.low_threshold = p_low_threshold * unit_size;
	drm_memctl.high_threshold = p_high_threshold * unit_size;
	drm_memctl.emer_threshold = (drm_memctl.high_threshold >> 4) +
		drm_memctl.high_threshold;
	spin_unlock(&drm_memctl.lock);
}


#ifndef DEBUG_MEMORY

/** No-op. */
void drm_mem_init(void)
{
}

/**
 * Called when "/proc/dri/%dev%/mem" is read.
 *
 * \param buf output buffer.
 * \param start start of output data.
 * \param offset requested start offset.
 * \param len requested number of bytes.
 * \param eof whether there is no more data to return.
 * \param data private data.
 * \return number of written bytes.
 *
 * No-op.
 */
int drm_mem_info(char *buf, char **start, off_t offset,
		 int len, int *eof, void *data)
{
	return 0;
}

/** Wrapper around kmalloc() */
void *drm_calloc(size_t nmemb, size_t size, int area)
{
	return kcalloc(nmemb, size, GFP_KERNEL);
}
EXPORT_SYMBOL(drm_calloc);

/** Wrapper around kmalloc() and kfree() */
void *drm_realloc(void *oldpt, size_t oldsize, size_t size, int area)
{
	void *pt;

	if (!(pt = kmalloc(size, GFP_KERNEL)))
		return NULL;
	if (oldpt && oldsize) {
		memcpy(pt, oldpt, oldsize);
		kfree(oldpt);
	}
	return pt;
}

/**
 * Allocate pages.
 *
 * \param order size order.
 * \param area memory area. (Not used.)
 * \return page address on success, or zero on failure.
 *
 * Allocate and reserve free pages.
 */
unsigned long drm_alloc_pages(int order, int area)
{
	unsigned long address;
	unsigned long bytes = PAGE_SIZE << order;
	unsigned long addr;
	unsigned int sz;

	address = __get_free_pages(GFP_KERNEL, order);
	if (!address)
		return 0;

	/* Zero */
	memset((void *)address, 0, bytes);

	/* Reserve */
	for (addr = address, sz = bytes;
	     sz > 0; addr += PAGE_SIZE, sz -= PAGE_SIZE) {
		SetPageReserved(virt_to_page(addr));
	}

	return address;
}

/**
 * Free pages.
 *
 * \param address address of the pages to free.
 * \param order size order.
 * \param area memory area. (Not used.)
 *
 * Unreserve and free pages allocated by alloc_pages().
 */
void drm_free_pages(unsigned long address, int order, int area)
{
	unsigned long bytes = PAGE_SIZE << order;
	unsigned long addr;
	unsigned int sz;

	if (!address)
		return;

	/* Unreserve */
	for (addr = address, sz = bytes;
	     sz > 0; addr += PAGE_SIZE, sz -= PAGE_SIZE) {
		ClearPageReserved(virt_to_page(addr));
	}

	free_pages(address, order);
}

#if __OS_HAS_AGP
static void *agp_remap(unsigned long offset, unsigned long size,
			      struct drm_device * dev)
{
	unsigned long *phys_addr_map, i, num_pages =
	    PAGE_ALIGN(size) / PAGE_SIZE;
	struct drm_agp_mem *agpmem;
	struct page **page_map;
	void *addr;

	size = PAGE_ALIGN(size);

#ifdef __alpha__
	offset -= dev->hose->mem_space->start;
#endif

	list_for_each_entry(agpmem, &dev->agp->memory, head)
		if (agpmem->bound <= offset
		    && (agpmem->bound + (agpmem->pages << PAGE_SHIFT)) >=
		    (offset + size))
			break;
	if (!agpmem)
		return NULL;

	/*
	 * OK, we're mapping AGP space on a chipset/platform on which memory accesses by
	 * the CPU do not get remapped by the GART.  We fix this by using the kernel's
	 * page-table instead (that's probably faster anyhow...).
	 */
	/* note: use vmalloc() because num_pages could be large... */
	page_map = vmalloc(num_pages * sizeof(struct page *));
	if (!page_map)
		return NULL;

	phys_addr_map =
	    agpmem->memory->memory + (offset - agpmem->bound) / PAGE_SIZE;
	for (i = 0; i < num_pages; ++i)
		page_map[i] = pfn_to_page(phys_addr_map[i] >> PAGE_SHIFT);
	addr = vmap(page_map, num_pages, VM_IOREMAP, PAGE_AGP);
	vfree(page_map);

	return addr;
}

/** Wrapper around agp_allocate_memory() */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,11)
DRM_AGP_MEM *drm_alloc_agp(struct drm_device *dev, int pages, u32 type)
{
	return drm_agp_allocate_memory(pages, type);
}
#else
DRM_AGP_MEM *drm_alloc_agp(struct drm_device *dev, int pages, u32 type)
{
	return drm_agp_allocate_memory(dev->agp->bridge, pages, type);
}
#endif

/** Wrapper around agp_free_memory() */
int drm_free_agp(DRM_AGP_MEM * handle, int pages)
{
	return drm_agp_free_memory(handle) ? 0 : -EINVAL;
}
EXPORT_SYMBOL(drm_free_agp);

/** Wrapper around agp_bind_memory() */
int drm_bind_agp(DRM_AGP_MEM * handle, unsigned int start)
{
	return drm_agp_bind_memory(handle, start);
}

/** Wrapper around agp_unbind_memory() */
int drm_unbind_agp(DRM_AGP_MEM * handle)
{
	return drm_agp_unbind_memory(handle);
}
EXPORT_SYMBOL(drm_unbind_agp);

#else  /* __OS_HAS_AGP*/
static void *agp_remap(unsigned long offset, unsigned long size,
		       struct drm_device * dev)
{
	return NULL;
}
#endif				/* agp */
#else
static void *agp_remap(unsigned long offset, unsigned long size,
		       struct drm_device * dev)
{
	return NULL;
}
#endif				/* debug_memory */

void drm_core_ioremap(struct drm_map *map, struct drm_device *dev)
{
	if (drm_core_has_AGP(dev) &&
	    dev->agp && dev->agp->cant_use_aperture && map->type == _DRM_AGP)
		map->handle = agp_remap(map->offset, map->size, dev);
	else
		map->handle = ioremap(map->offset, map->size);
}
EXPORT_SYMBOL_GPL(drm_core_ioremap);

void drm_core_ioremapfree(struct drm_map *map, struct drm_device *dev)
{
	if (!map->handle || !map->size)
		return;

	if (drm_core_has_AGP(dev) &&
	    dev->agp && dev->agp->cant_use_aperture && map->type == _DRM_AGP)
		vunmap(map->handle);
	else
		iounmap(map->handle);
}
EXPORT_SYMBOL_GPL(drm_core_ioremapfree);
