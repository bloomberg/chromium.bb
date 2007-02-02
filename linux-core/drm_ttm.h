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
/*
 * Authors: Thomas Hellström <thomas-at-tungstengraphics-dot-com>
 */

#ifndef _DRM_TTM_H
#define _DRM_TTM_H
#define DRM_HAS_TTM

/*
 * The backend GART interface. (In our case AGP). Any similar type of device (PCIE?)
 * needs only to implement these functions to be usable with the "TTM" interface.
 * The AGP backend implementation lives in drm_agpsupport.c 
 * basically maps these calls to available functions in agpgart. Each drm device driver gets an
 * additional function pointer that creates these types, 
 * so that the device can choose the correct aperture.
 * (Multiple AGP apertures, etc.) 
 * Most device drivers will let this point to the standard AGP implementation.
 */

#define DRM_BE_FLAG_NEEDS_FREE     0x00000001
#define DRM_BE_FLAG_BOUND_CACHED   0x00000002
#define DRM_BE_FLAG_CMA            0x00000004 /* Don't map through aperture */

typedef struct drm_ttm_backend {
	unsigned long aperture_base;
	void *private;
	uint32_t flags;
	uint32_t drm_map_type;
	int (*needs_ub_cache_adjust) (struct drm_ttm_backend * backend);
	int (*populate) (struct drm_ttm_backend * backend,
			 unsigned long num_pages, struct page ** pages);
	void (*clear) (struct drm_ttm_backend * backend);
	int (*bind) (struct drm_ttm_backend * backend,
		     unsigned long offset, int cached);
	int (*unbind) (struct drm_ttm_backend * backend);
	void (*destroy) (struct drm_ttm_backend * backend);
} drm_ttm_backend_t;

typedef struct drm_ttm {
	struct page **pages;
	uint32_t page_flags;
	unsigned long num_pages;
	unsigned long aper_offset;
	atomic_t vma_count;
	struct drm_device *dev;
	int destroy;
	uint32_t mapping_offset;
	drm_ttm_backend_t *be;
	enum {
		ttm_bound,
		ttm_evicted,
		ttm_unbound,
		ttm_unpopulated,
	} state;

} drm_ttm_t;


extern drm_ttm_t *drm_ttm_init(struct drm_device *dev, unsigned long size);
extern struct page *drm_ttm_alloc_page(void);
extern int drm_bind_ttm(drm_ttm_t * ttm, int cached, unsigned long aper_offset);
extern void drm_ttm_unbind(drm_ttm_t * ttm);
extern void drm_ttm_evict(drm_ttm_t * ttm);
extern void drm_ttm_fixup_caching(drm_ttm_t * ttm);

/*
 * Destroy a ttm. The user normally calls drmRmMap or a similar IOCTL to do this, 
 * which calls this function iff there are no vmas referencing it anymore. Otherwise it is called
 * when the last vma exits.
 */

extern int drm_destroy_ttm(drm_ttm_t * ttm);

#define DRM_MASK_VAL(dest, mask, val)			\
  (dest) = ((dest) & ~(mask)) | ((val) & (mask));

#define DRM_TTM_MASK_FLAGS ((1 << PAGE_SHIFT) - 1)
#define DRM_TTM_MASK_PFN (0xFFFFFFFFU - DRM_TTM_MASK_FLAGS)

/*
 * Page flags.
 */

#define DRM_TTM_PAGE_UNCACHED 0x01
#define DRM_TTM_PAGE_USED     0x02
#define DRM_TTM_PAGE_BOUND    0x04
#define DRM_TTM_PAGE_PRESENT  0x08
#define DRM_TTM_PAGE_VMALLOC  0x10

#endif
