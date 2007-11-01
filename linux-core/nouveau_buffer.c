/*
 * Copyright 2005 Stephane Marchesin.
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
/*
 * Authors: Jeremy Kolb <jkolb@brandeis.edu>
 */

#include "drmP.h"
#include "nouveau_drm.h"
#include "nouveau_drv.h"

#ifdef NOUVEAU_HAVE_BUFFER

struct drm_ttm_backend *nouveau_create_ttm_backend_entry(struct drm_device * dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	return drm_agp_init_ttm(dev);
}

int nouveau_fence_types(struct drm_buffer_object *bo,
			uint32_t *fclass,
			uint32_t *type)
{
	*fclass = 0;

	if (bo->mem.mask & (DRM_BO_FLAG_READ | DRM_BO_FLAG_WRITE))
		*type = 3;
	else
		*type = 1;
	return 0;

}
int nouveau_invalidate_caches(struct drm_device *dev, uint64_t buffer_flags)
{
	/* We'll do this from user space. */
	return 0;
}

int nouveau_init_mem_type(struct drm_device *dev,
			  uint32_t type,
			  struct drm_mem_type_manager *man)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	switch (type) {
		case DRM_BO_MEM_LOCAL:
			man->flags = _DRM_FLAG_MEMTYPE_MAPPABLE |
				_DRM_FLAG_MEMTYPE_CACHED;
			man->drm_bus_maptype = 0;
			break;

		case DRM_BO_MEM_VRAM:
			man->flags = _DRM_FLAG_MEMTYPE_FIXED |
				_DRM_FLAG_MEMTYPE_MAPPABLE |
				_DRM_FLAG_NEEDS_IOREMAP;
			man->io_addr = NULL;
			man->drm_bus_maptype = _DRM_FRAME_BUFFER;
			man->io_offset = drm_get_resource_start(dev, 0);
			man->io_size = drm_get_resource_len(dev, 0);
			break;

		case DRM_BO_MEM_TT:
			if (!(drm_core_has_AGP(dev) && dev->agp)) {
				DRM_ERROR("AGP is not enabled for memory type %u\n",
						(unsigned)type);
				return -EINVAL;
			}

			man->io_offset = dev->agp->agp_info.aper_base;
			man->io_size = dev->agp->agp_info.aper_size * 1024 * 1024;
			man->io_addr = NULL;
			man->flags = _DRM_FLAG_MEMTYPE_MAPPABLE |
				_DRM_FLAG_MEMTYPE_CSELECT | _DRM_FLAG_NEEDS_IOREMAP;
			man->drm_bus_maptype = _DRM_AGP;
			break;

		default:
			DRM_ERROR("Unsupported memory type %u\n", (unsigned)type);
			return -EINVAL;
	}
	return 0;
}

uint32_t nouveau_evict_mask(struct drm_buffer_object *bo)
{
	switch (bo->mem.mem_type) {
		case DRM_BO_MEM_LOCAL:
		case DRM_BO_MEM_TT:
			return DRM_BO_FLAG_MEM_LOCAL;
		case DRM_BO_MEM_VRAM:
			if (bo->mem.num_pages > 128)
				return DRM_BO_MEM_TT;
			else
				return DRM_BO_MEM_LOCAL;
		default:
			return DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_CACHED;
	}

}

int nouveau_move(struct drm_buffer_object *bo,
		 int evict,
		 int no_wait,
		 struct drm_bo_mem_reg *new_mem)
{
	struct drm_bo_mem_reg *old_mem = &bo->mem;

	if (old_mem->mem_type == DRM_BO_MEM_LOCAL) {
		return drm_bo_move_memcpy(bo, evict, no_wait, new_mem);
	else if (new_mem->mem_type == DRM_BO_MEM_LOCAL) {
		return drm_bo_move_memcpy(bo, evict, no_wait, new_mem);
	}
	else {
		return drm_bo_move_memcpy(bo, evict, no_wait, new_mem);
	}
	return 0;
}

void nouveau_flush_ttm(struct drm_ttm *ttm)
{

}

#endif
