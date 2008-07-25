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

int radeon_fence_emit_sequence(struct drm_device *dev, uint32_t class,
			       uint32_t flags, uint32_t *sequence,
			       uint32_t *native_type)
{
	struct drm_radeon_private *dev_priv = (struct drm_radeon_private *) dev->dev_private;
	RING_LOCALS;

	if (!dev_priv)
		return -EINVAL;

	radeon_emit_irq(dev);
	*sequence = (uint32_t) dev_priv->counter;
	*native_type = DRM_FENCE_TYPE_EXE;

	return 0;
}

static void radeon_fence_poll(struct drm_device *dev, uint32_t fence_class,
			      uint32_t waiting_types)
{
	struct drm_radeon_private *dev_priv = (struct drm_radeon_private *) dev->dev_private;
	uint32_t sequence;
        if (waiting_types & DRM_FENCE_TYPE_EXE) {

                sequence = READ_BREADCRUMB(dev_priv);

                drm_fence_handler(dev, 0, sequence,
                                  DRM_FENCE_TYPE_EXE, 0);
        }
}

void radeon_fence_handler(struct drm_device * dev)
{
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_class_manager *fc = &fm->fence_class[0];

	write_lock(&fm->lock);
	radeon_fence_poll(dev, 0, fc->waiting_types);
	write_unlock(&fm->lock);
}

int radeon_fence_has_irq(struct drm_device *dev, uint32_t class, uint32_t flags)
{
	/*
	 * We have an irq that tells us when we have a new breadcrumb.
	 */
	return 1;
}


struct drm_fence_driver radeon_fence_driver = {
	.num_classes = 1,
	.wrap_diff = (1U << (BREADCRUMB_BITS -1)),
	.flush_diff = (1U << (BREADCRUMB_BITS - 2)),
	.sequence_mask = BREADCRUMB_MASK,
	.emit = radeon_fence_emit_sequence,
	.has_irq = radeon_fence_has_irq,
	.poll = radeon_fence_poll,
};

