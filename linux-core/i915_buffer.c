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

#include "drmP.h"
#include "i915_drm.h"
#include "i915_drv.h"


drm_ttm_backend_t *i915_create_ttm_backend_entry(drm_device_t * dev)
{
	return drm_agp_init_ttm(dev, NULL);
}

int i915_fence_types(uint32_t buffer_flags, uint32_t * class, uint32_t * type)
{
	*class = 0;
	if (buffer_flags & (DRM_BO_FLAG_READ | DRM_BO_FLAG_WRITE))
		*type = 3;
	else
		*type = 1;
	return 0;
}

int i915_invalidate_caches(drm_device_t * dev, uint32_t flags)
{
	/*
	 * FIXME: Only emit once per batchbuffer submission.
	 */

	uint32_t flush_cmd = MI_NO_WRITE_FLUSH;

	if (flags & DRM_BO_FLAG_READ)
		flush_cmd |= MI_READ_FLUSH;
	if (flags & DRM_BO_FLAG_EXE)
		flush_cmd |= MI_EXE_FLUSH;

	return i915_emit_mi_flush(dev, flush_cmd);
}

int i915_init_mem_type(drm_device_t *dev, uint32_t type, 
		       drm_mem_type_manager_t *man)
{
	switch(type) {
	case DRM_BO_MEM_LOCAL:
	case DRM_BO_MEM_TT:
		man->flags = _DRM_FLAG_MEMTYPE_MAPPABLE |
			_DRM_FLAG_MEMTYPE_CACHED;
		break;
	case DRM_BO_MEM_PRIV0:
		if (!(drm_core_has_AGP(dev) && dev->agp)) {
			DRM_ERROR("AGP is not enabled for memory type %u\n", 
				  (unsigned) type);
			return -EINVAL;
		}
		man->io_offset = dev->agp->agp_info.aper_base;
		man->io_size = dev->agp->agp_info.aper_size * 1024 * 1024;

		man->flags = _DRM_FLAG_MEMTYPE_MAPPABLE |
			_DRM_FLAG_MEMTYPE_CACHED |
			_DRM_FLAG_MEMTYPE_FIXED |
			_DRM_FLAG_NEEDS_IOREMAP;

		man->io_addr = NULL;
		break;
	default:
		DRM_ERROR("Unsupported memory type %u\n", (unsigned) type);
		return -EINVAL;
	}
	return 0;
}

uint32_t i915_evict_flags(drm_device_t *dev, uint32_t type)
{
	switch(type) {
	case DRM_BO_MEM_LOCAL:
	case DRM_BO_MEM_TT:
		return DRM_BO_FLAG_MEM_LOCAL;
	default:
		return DRM_BO_FLAG_MEM_TT;
	}
}

void i915_emit_copy_blit(drm_device_t *dev,
			 uint32_t src_offset,
			 uint32_t dst_offset,
			 uint32_t pages,
			 int direction)
{
	uint32_t cur_pages;
	uint32_t stride = PAGE_SIZE;
	drm_i915_private_t *dev_priv = dev->dev_private;
	RING_LOCALS;

	if (!dev_priv)
		return;
	
	if (direction) {
		stride = -stride;
		src_offset += (pages - 1) << PAGE_SHIFT;
		dst_offset += (pages - 1) << PAGE_SHIFT;
	}

	while(pages > 0) {
		cur_pages = pages;
		if (cur_pages > 2048)
			cur_pages = 2048;
		pages -= cur_pages;

		BEGIN_LP_RING(8);
		OUT_RING(XY_SRC_COPY_BLT_CMD | XY_SRC_COPY_BLT_WRITE_ALPHA |
			 XY_SRC_COPY_BLT_WRITE_RGB);
		OUT_RING((stride & 0xffff) | ( 0xcc << 16) | (1 << 24) | 
			 (1 << 25));
		OUT_RING(0);
		OUT_RING((cur_pages << 16) | (PAGE_SIZE >> 2));
		OUT_RING(dst_offset);
		OUT_RING(0);
		OUT_RING(stride & 0xffff);
		OUT_RING(src_offset);
		ADVANCE_LP_RING();
		dst_offset += (cur_pages << PAGE_SHIFT)*(direction ? -1 : 1);
		src_offset += (cur_pages << PAGE_SHIFT)*(direction ? -1 : 1);
	}
	return;
}
