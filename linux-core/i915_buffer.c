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

#define INTEL_AGP_MEM_USER (1 << 16)
#define INTEL_AGP_MEM_UCACHED (2 << 16)

drm_ttm_backend_t *i915_create_ttm_backend_entry(drm_device_t *dev)
{
	return drm_agp_init_ttm(dev, NULL, INTEL_AGP_MEM_USER, INTEL_AGP_MEM_UCACHED,
				INTEL_AGP_MEM_USER);
}

int i915_fence_types(uint32_t buffer_flags, uint32_t *class, uint32_t *type)
{
	*class = 0;
	if (buffer_flags & (DRM_BO_FLAG_READ | DRM_BO_FLAG_WRITE)) 
		*type = 3;
	else
		*type = 1;
	return 0;
}

int i915_invalidate_caches(drm_device_t *dev, uint32_t flags)
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
