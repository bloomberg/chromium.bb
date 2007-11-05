/**************************************************************************
 *
 * Copyright (c) 2007 Tungsten Graphics, Inc., Cedar Park, TX., USA,
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
#include "via_drm.h"
#include "via_drv.h"

struct drm_ttm_backend *via_create_ttm_backend_entry(struct drm_device * dev)
{
	return drm_agp_init_ttm(dev);
}

int via_fence_types(struct drm_buffer_object *bo, uint32_t * fclass,
		    uint32_t * type)
{
	*type = 3;
	return 0;
}

int via_invalidate_caches(struct drm_device * dev, uint64_t flags)
{
	/*
	 * FIXME: Invalidate texture caches here.
	 */

	return 0;
}


static int via_vram_info(struct drm_device *dev,
			 unsigned long *offset,
			 unsigned long *size)
{
	struct pci_dev *pdev = dev->pdev;
	unsigned long flags;

	int ret = -EINVAL;
	int i;
	for (i=0; i<6; ++i) {
		flags = pci_resource_flags(pdev, i);
		if ((flags & (IORESOURCE_MEM | IORESOURCE_PREFETCH)) ==
		    (IORESOURCE_MEM | IORESOURCE_PREFETCH)) {
			ret = 0;
			break;
		}
	}

	if (ret) {
		DRM_ERROR("Could not find VRAM PCI resource\n");
		return ret;
	}

	*offset = pci_resource_start(pdev, i);
	*size = pci_resource_end(pdev, i) - *offset + 1;
	return 0;
}

int via_init_mem_type(struct drm_device * dev, uint32_t type,
		       struct drm_mem_type_manager * man)
{
	switch (type) {
	case DRM_BO_MEM_LOCAL:
		/* System memory */

		man->flags = _DRM_FLAG_MEMTYPE_MAPPABLE |
			_DRM_FLAG_MEMTYPE_CACHED;
		man->drm_bus_maptype = 0;
		break;

	case DRM_BO_MEM_TT:
		/* Dynamic agpgart memory */

		if (!(drm_core_has_AGP(dev) && dev->agp)) {
			DRM_ERROR("AGP is not enabled for memory type %u\n",
				  (unsigned)type);
			return -EINVAL;
		}
		man->io_offset = dev->agp->agp_info.aper_base;
		man->io_size = dev->agp->agp_info.aper_size * 1024 * 1024;
		man->io_addr = NULL;
		man->flags = _DRM_FLAG_MEMTYPE_MAPPABLE | _DRM_FLAG_NEEDS_IOREMAP;

		/* Only to get pte protection right. */

		man->drm_bus_maptype = _DRM_AGP;
		break;

	case DRM_BO_MEM_VRAM:
		/* "On-card" video ram */

		man->flags = _DRM_FLAG_MEMTYPE_MAPPABLE | _DRM_FLAG_NEEDS_IOREMAP;
		man->drm_bus_maptype = _DRM_FRAME_BUFFER;
		man->io_addr = NULL;
		return via_vram_info(dev, &man->io_offset, &man->io_size);
		break;

	case DRM_BO_MEM_PRIV0:
		/* Pre-bound agpgart memory */

		if (!(drm_core_has_AGP(dev) && dev->agp)) {
			DRM_ERROR("AGP is not enabled for memory type %u\n",
				  (unsigned)type);
			return -EINVAL;
		}
		man->io_offset = dev->agp->agp_info.aper_base;
		man->io_size = dev->agp->agp_info.aper_size * 1024 * 1024;
		man->io_addr = NULL;
		man->flags =  _DRM_FLAG_MEMTYPE_MAPPABLE |
		    _DRM_FLAG_MEMTYPE_FIXED | _DRM_FLAG_NEEDS_IOREMAP;
		man->drm_bus_maptype = _DRM_AGP;
		break;

	default:
		DRM_ERROR("Unsupported memory type %u\n", (unsigned)type);
		return -EINVAL;
	}
	return 0;
}

uint32_t via_evict_mask(struct drm_buffer_object *bo)
{
	switch (bo->mem.mem_type) {
	case DRM_BO_MEM_LOCAL:
	case DRM_BO_MEM_TT:
		return DRM_BO_FLAG_MEM_LOCAL; /* Evict TT to local */
	case DRM_BO_MEM_PRIV0: /* Evict pre-bound AGP to TT */
		return DRM_BO_MEM_TT;
	case DRM_BO_MEM_VRAM:
		if (bo->mem.num_pages > 128)
			return DRM_BO_MEM_TT;
		else
			return DRM_BO_MEM_LOCAL;
	default:
		return DRM_BO_MEM_LOCAL;
	}
}
