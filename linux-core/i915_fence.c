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
 * Initiate a sync flush if it's not already pending.
 */

static inline void i915_initiate_rwflush(struct drm_i915_private *dev_priv,
					 struct drm_fence_class_manager *fc)
{
	if ((fc->pending_flush & DRM_I915_FENCE_TYPE_RW) &&
	    !dev_priv->flush_pending) {
		dev_priv->flush_sequence = (uint32_t) READ_BREADCRUMB(dev_priv);
		dev_priv->flush_flags = fc->pending_flush;
		dev_priv->saved_flush_status = READ_HWSP(dev_priv, 0);
		I915_WRITE(I915REG_INSTPM, (1 << 5) | (1 << 21));
		dev_priv->flush_pending = 1;
		fc->pending_flush &= ~DRM_I915_FENCE_TYPE_RW;
	}
}

static inline void i915_report_rwflush(struct drm_device *dev,
				       struct drm_i915_private *dev_priv)
{
	if (unlikely(dev_priv->flush_pending)) {

		uint32_t flush_flags;
		uint32_t i_status;
		uint32_t flush_sequence;

		i_status = READ_HWSP(dev_priv, 0);
		if ((i_status & (1 << 12)) !=
		    (dev_priv->saved_flush_status & (1 << 12))) {
			flush_flags = dev_priv->flush_flags;
			flush_sequence = dev_priv->flush_sequence;
			dev_priv->flush_pending = 0;
			drm_fence_handler(dev, 0, flush_sequence,
					  flush_flags, 0);
		}
	}
}

static void i915_fence_flush(struct drm_device *dev,
			     uint32_t fence_class)
{
	struct drm_i915_private *dev_priv = 
		(struct drm_i915_private *) dev->dev_private;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_class_manager *fc = &fm->fence_class[0];
	unsigned long irq_flags;

	if (unlikely(!dev_priv))
		return;

	write_lock_irqsave(&fm->lock, irq_flags);
	i915_initiate_rwflush(dev_priv, fc);
	write_unlock_irqrestore(&fm->lock, irq_flags);
}


static void i915_fence_poll(struct drm_device *dev, uint32_t fence_class,
			    uint32_t waiting_types)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_class_manager *fc = &fm->fence_class[0];
	uint32_t sequence;

	if (unlikely(!dev_priv))
		return;

	/*
	 * First, report any executed sync flush:
	 */

	i915_report_rwflush(dev, dev_priv);

	/*
	 * Report A new breadcrumb, and adjust IRQs.
	 */

	if (waiting_types & DRM_FENCE_TYPE_EXE) {

		sequence = READ_BREADCRUMB(dev_priv);
		drm_fence_handler(dev, 0, sequence,
				  DRM_FENCE_TYPE_EXE, 0);

		if (dev_priv->fence_irq_on &&
		    !(fc->waiting_types & DRM_FENCE_TYPE_EXE)) {
			i915_user_irq_off(dev_priv);
			dev_priv->fence_irq_on = 0;
		} else if (!dev_priv->fence_irq_on &&
			   (fc->waiting_types & DRM_FENCE_TYPE_EXE)) {
			i915_user_irq_on(dev_priv);
			dev_priv->fence_irq_on = 1;
		}
	}

	/*
	 * There may be new RW flushes pending. Start them.
	 */
	
	i915_initiate_rwflush(dev_priv, fc); 

	/*
	 * And possibly, but unlikely, they finish immediately.
	 */

	i915_report_rwflush(dev, dev_priv);

}

static int i915_fence_emit_sequence(struct drm_device *dev, uint32_t class,
			     uint32_t flags, uint32_t *sequence,
			     uint32_t *native_type)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	if (unlikely(!dev_priv))
		return -EINVAL;

	i915_emit_irq(dev);
	*sequence = (uint32_t) dev_priv->counter;
	*native_type = DRM_FENCE_TYPE_EXE;
	if (flags & DRM_I915_FENCE_FLAG_FLUSHED)
		*native_type |= DRM_I915_FENCE_TYPE_RW;

	return 0;
}

void i915_fence_handler(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = (struct drm_i915_private *) dev->dev_private;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_class_manager *fc = &fm->fence_class[0];

	write_lock(&fm->lock);
	if (likely(dev_priv->fence_irq_on))
		i915_fence_poll(dev, 0, fc->waiting_types);
	write_unlock(&fm->lock);
}

/*
 * We need a separate wait function since we need to poll for
 * sync flushes.
 */

static int i915_fence_wait(struct drm_fence_object *fence,
			   int lazy, int interruptible, uint32_t mask)
{
	struct drm_device *dev = fence->dev;
	drm_i915_private_t *dev_priv = (struct drm_i915_private *) dev->dev_private;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_class_manager *fc = &fm->fence_class[0];
	int ret;
	unsigned long  _end = jiffies + 3 * DRM_HZ;

	drm_fence_object_flush(fence, mask);
	if (likely(interruptible))
		ret = wait_event_interruptible_timeout
			(fc->fence_queue, drm_fence_object_signaled(fence, DRM_FENCE_TYPE_EXE), 
			 3 * DRM_HZ);
	else 
		ret = wait_event_timeout
			(fc->fence_queue, drm_fence_object_signaled(fence, DRM_FENCE_TYPE_EXE), 
			 3 * DRM_HZ);

	if (unlikely(ret == -ERESTARTSYS))
		return -EAGAIN;

	if (unlikely(ret == 0))
		return -EBUSY;

	if (likely(mask == DRM_FENCE_TYPE_EXE || 
		   drm_fence_object_signaled(fence, mask))) 
		return 0;

	/*
	 * Remove this code snippet when fixed. HWSTAM doesn't let
	 * flush info through...
	 */

	if (unlikely(dev_priv && !dev_priv->irq_enabled)) {
		unsigned long irq_flags;

		DRM_ERROR("X server disabled IRQs before releasing frame buffer.\n");
		msleep(100);
		dev_priv->flush_pending = 0;
		write_lock_irqsave(&fm->lock, irq_flags);
		drm_fence_handler(dev, fence->fence_class, 
				  fence->sequence, fence->type, 0);
		write_unlock_irqrestore(&fm->lock, irq_flags);
	}

	/*
	 * Poll for sync flush completion.
	 */

	return drm_fence_wait_polling(fence, lazy, interruptible, mask, _end);
}

static uint32_t i915_fence_needed_flush(struct drm_fence_object *fence)
{
	uint32_t flush_flags = fence->waiting_types & 
		~(DRM_FENCE_TYPE_EXE | fence->signaled_types);

	if (likely(flush_flags == 0 || 
		   ((flush_flags & ~fence->native_types) == 0) || 
		   (fence->signaled_types != DRM_FENCE_TYPE_EXE)))
		return 0;
	else {
		struct drm_device *dev = fence->dev;
		struct drm_i915_private *dev_priv = (struct drm_i915_private *) dev->dev_private;
		struct drm_fence_driver *driver = dev->driver->fence_driver;
		
		if (unlikely(!dev_priv))
			return 0;

		if (dev_priv->flush_pending) {
			uint32_t diff = (dev_priv->flush_sequence - fence->sequence) & 
				driver->sequence_mask;

			if (diff < driver->wrap_diff)
				return 0;
		}
	}
	return flush_flags;
}

struct drm_fence_driver i915_fence_driver = {
	.num_classes = 1,
	.wrap_diff = (1U << (BREADCRUMB_BITS - 1)),
	.flush_diff = (1U << (BREADCRUMB_BITS - 2)),
	.sequence_mask = BREADCRUMB_MASK,
	.has_irq = NULL,
	.emit = i915_fence_emit_sequence,
	.flush = i915_fence_flush,
	.poll = i915_fence_poll,
	.needed_flush = i915_fence_needed_flush,
	.wait = i915_fence_wait,
};
