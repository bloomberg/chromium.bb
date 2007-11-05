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
#include "i915_drm.h"
#include "i915_drv.h"

/*
 * Implements an intel sync flush operation.
 */

static void i915_perform_flush(struct drm_device * dev)
{
	struct drm_i915_private *dev_priv = (struct drm_i915_private *) dev->dev_private;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_class_manager *fc = &fm->fence_class[0];
	struct drm_fence_driver *driver = dev->driver->fence_driver;
	uint32_t flush_flags = 0;
	uint32_t flush_sequence = 0;
	uint32_t i_status;
	uint32_t diff;
	uint32_t sequence;
	int rwflush;

	if (!dev_priv)
		return;

	if (fc->pending_exe_flush) {
		sequence = READ_BREADCRUMB(dev_priv);

		/*
		 * First update fences with the current breadcrumb.
		 */

		diff = (sequence - fc->last_exe_flush) & BREADCRUMB_MASK;
		if (diff < driver->wrap_diff && diff != 0) {
		        drm_fence_handler(dev, 0, sequence,
					  DRM_FENCE_TYPE_EXE, 0);
		}

		if (dev_priv->fence_irq_on && !fc->pending_exe_flush) {
			i915_user_irq_off(dev_priv);
			dev_priv->fence_irq_on = 0;
		} else if (!dev_priv->fence_irq_on && fc->pending_exe_flush) {
			i915_user_irq_on(dev_priv);
			dev_priv->fence_irq_on = 1;
		}
	}

	if (dev_priv->flush_pending) {
		i_status = READ_HWSP(dev_priv, 0);
		if ((i_status & (1 << 12)) !=
		    (dev_priv->saved_flush_status & (1 << 12))) {
			flush_flags = dev_priv->flush_flags;
			flush_sequence = dev_priv->flush_sequence;
			dev_priv->flush_pending = 0;
			drm_fence_handler(dev, 0, flush_sequence, flush_flags, 0);
		}
	}

	rwflush = fc->pending_flush & DRM_I915_FENCE_TYPE_RW;
	if (rwflush && !dev_priv->flush_pending) {
		dev_priv->flush_sequence = (uint32_t) READ_BREADCRUMB(dev_priv);
		dev_priv->flush_flags = fc->pending_flush;
		dev_priv->saved_flush_status = READ_HWSP(dev_priv, 0);
		I915_WRITE(I915REG_INSTPM, (1 << 5) | (1 << 21));
		dev_priv->flush_pending = 1;
		fc->pending_flush &= ~DRM_I915_FENCE_TYPE_RW;
	}

	if (dev_priv->flush_pending) {
		i_status = READ_HWSP(dev_priv, 0);
		if ((i_status & (1 << 12)) !=
		    (dev_priv->saved_flush_status & (1 << 12))) {
			flush_flags = dev_priv->flush_flags;
			flush_sequence = dev_priv->flush_sequence;
			dev_priv->flush_pending = 0;
			drm_fence_handler(dev, 0, flush_sequence, flush_flags, 0);
		}
	}

}

void i915_poke_flush(struct drm_device * dev, uint32_t class)
{
	struct drm_fence_manager *fm = &dev->fm;
	unsigned long flags;

	write_lock_irqsave(&fm->lock, flags);
	i915_perform_flush(dev);
	write_unlock_irqrestore(&fm->lock, flags);
}

int i915_fence_emit_sequence(struct drm_device * dev, uint32_t class, uint32_t flags,
			     uint32_t * sequence, uint32_t * native_type)
{
	struct drm_i915_private *dev_priv = (struct drm_i915_private *) dev->dev_private;
	if (!dev_priv)
		return -EINVAL;

	i915_emit_irq(dev);
	*sequence = (uint32_t) dev_priv->counter;
	*native_type = DRM_FENCE_TYPE_EXE;
	if (flags & DRM_I915_FENCE_FLAG_FLUSHED)
		*native_type |= DRM_I915_FENCE_TYPE_RW;

	return 0;
}

void i915_fence_handler(struct drm_device * dev)
{
	struct drm_fence_manager *fm = &dev->fm;

	write_lock(&fm->lock);
	i915_perform_flush(dev);
	write_unlock(&fm->lock);
}

int i915_fence_has_irq(struct drm_device *dev, uint32_t class, uint32_t flags)
{
	/*
	 * We have an irq that tells us when we have a new breadcrumb.
	 */

	if (class == 0 && flags == DRM_FENCE_TYPE_EXE)
		return 1;

	return 0;
}
