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
#include "drm.h"
#include "radeon_drm.h"
#include "radeon_drv.h"

/*
 * Implements an intel sync flush operation.
 */

static void radeon_perform_flush(struct drm_device * dev)
{
	drm_radeon_private_t *dev_priv = (drm_radeon_private_t *) dev->dev_private;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_class_manager *fc = &dev->fm.fence_class[0];
	struct drm_fence_driver *driver = dev->driver->fence_driver;
	uint32_t pending_flush_types = 0;
	uint32_t sequence;

	if (!dev_priv)
		return;

	pending_flush_types = fc->pending_flush |
		((fc->pending_exe_flush) ? DRM_FENCE_TYPE_EXE : 0);

	if (pending_flush_types) {
		sequence = READ_BREADCRUMB(dev_priv);
					     
		drm_fence_handler(dev, 0, sequence, pending_flush_types, 0);
	}

	return;
}

void radeon_poke_flush(struct drm_device * dev, uint32_t class)
{
	struct drm_fence_manager *fm = &dev->fm;
	unsigned long flags;

	if (class != 0)
		return;

	write_lock_irqsave(&fm->lock, flags);
	radeon_perform_flush(dev);
	write_unlock_irqrestore(&fm->lock, flags);
}

int radeon_fence_emit_sequence(struct drm_device *dev, uint32_t class,
			       uint32_t flags, uint32_t *sequence,
			       uint32_t *native_type)
{
	drm_radeon_private_t *dev_priv = (drm_radeon_private_t *) dev->dev_private;
	RING_LOCALS;

	if (!dev_priv)
		return -EINVAL;

	*native_type = DRM_FENCE_TYPE_EXE;
	if (flags & DRM_RADEON_FENCE_FLAG_FLUSHED) {
		*native_type |= DRM_RADEON_FENCE_TYPE_RW;
		
		BEGIN_RING(4);
		
		RADEON_FLUSH_CACHE();
		RADEON_FLUSH_ZCACHE();
		ADVANCE_RING();
	}

	radeon_emit_irq(dev);
	*sequence = (uint32_t) dev_priv->counter;


	return 0;
}

void radeon_fence_handler(struct drm_device * dev)
{
	struct drm_fence_manager *fm = &dev->fm;

	write_lock(&fm->lock);
	radeon_perform_flush(dev);
	write_unlock(&fm->lock);
}

int radeon_fence_has_irq(struct drm_device *dev, uint32_t class, uint32_t flags)
{
	/*
	 * We have an irq that tells us when we have a new breadcrumb.
	 */

	if (class == 0 && flags == DRM_FENCE_TYPE_EXE)
		return 1;

	return 0;
}
