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
#include "radeon_drm.h"
#include "radeon_drv.h"

drm_ttm_backend_t *radeon_create_ttm_backend_entry(drm_device_t * dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;

	if(dev_priv->flags & RADEON_IS_AGP)
		return drm_agp_init_ttm(dev);
	else
		return ati_pcigart_init_ttm(dev, &dev_priv->gart_info, radeon_gart_flush);
}

int radeon_fence_types(drm_buffer_object_t *bo, uint32_t * class, uint32_t * type)
{
	*class = 0;
	if (bo->mem.flags & (DRM_BO_FLAG_READ | DRM_BO_FLAG_WRITE))
		*type = 3;
	else
		*type = 1;
	return 0;
}

int radeon_invalidate_caches(drm_device_t * dev, uint32_t flags)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	RING_LOCALS;

	BEGIN_RING(4);
	RADEON_FLUSH_CACHE();
	RADEON_FLUSH_ZCACHE();
	ADVANCE_RING();
	return 0;
}

uint32_t radeon_evict_mask(drm_buffer_object_t *bo)
{
	switch (bo->mem.mem_type) {
	case DRM_BO_MEM_LOCAL:
	case DRM_BO_MEM_TT:
		return DRM_BO_FLAG_MEM_LOCAL;
	default:
		return DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_CACHED;
	}
}

int radeon_init_mem_type(drm_device_t * dev, uint32_t type,
			 drm_mem_type_manager_t * man)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;

	switch (type) {
	case DRM_BO_MEM_LOCAL:
		man->flags = _DRM_FLAG_MEMTYPE_MAPPABLE |
		    _DRM_FLAG_MEMTYPE_CACHED;
		man->drm_bus_maptype = 0;
		break;
	case DRM_BO_MEM_TT:
		if (dev_priv->flags & RADEON_IS_AGP) {
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
		} else {
			man->io_offset = dev_priv->gart_vm_start;
			man->io_size = dev_priv->gart_size;
			man->io_addr = NULL;
			man->flags = _DRM_FLAG_MEMTYPE_CSELECT | _DRM_FLAG_MEMTYPE_MAPPABLE;
			man->drm_bus_maptype = _DRM_SCATTER_GATHER;
		}
		break;
	default:
		DRM_ERROR("Unsupported memory type %u\n", (unsigned)type);
		return -EINVAL;
	}
	return 0;
}

int radeon_move(drm_buffer_object_t * bo,
		int evict, int no_wait, drm_bo_mem_reg_t * new_mem)
{
	drm_bo_mem_reg_t *old_mem = &bo->mem;

	DRM_DEBUG("\n");
	if (old_mem->mem_type == DRM_BO_MEM_LOCAL) {
		return drm_bo_move_memcpy(bo, evict, no_wait, new_mem);
	}
	return 0;
}

