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
#include "via_drm.h"
#include "via_drv.h"

/*
 * DRM_FENCE_TYPE_EXE guarantees that all command buffers can be evicted.
 * DRM_VIA_FENCE_TYPE_ACCEL guarantees that all 2D & 3D rendering is complete.
 */


static uint32_t via_perform_flush(drm_device_t *dev, uint32_t class)
{
	drm_via_private_t *dev_priv = (drm_via_private_t *) dev->dev_private;
	drm_fence_class_manager_t *fc = &dev->fm.class[class];
	uint32_t pending_flush_types = 0;
	uint32_t signaled_flush_types = 0;
	uint32_t status;

	if (class != 0)
		return 0;

	if (!dev_priv)
		return 0;

	spin_lock(&dev_priv->fence_lock);

	pending_flush_types = fc->pending_flush |
		((fc->pending_exe_flush) ? DRM_FENCE_TYPE_EXE : 0);

	if (pending_flush_types) {

		/*
		 * Take the idlelock. This guarantees that the next time a client tries
		 * to grab the lock, it will stall until the idlelock is released. This
		 * guarantees that eventually, the GPU engines will be idle, but nothing
		 * else. It cannot be used to protect the hardware.
		 */


		if (!dev_priv->have_idlelock) {
			drm_idlelock_take(&dev->lock);
			dev_priv->have_idlelock = 1;
		}

		/*
		 * Check if AGP command reader is idle.
		 */

		if (pending_flush_types & DRM_FENCE_TYPE_EXE)
			if (VIA_READ(0x41C) & 0x80000000)
				signaled_flush_types |= DRM_FENCE_TYPE_EXE;

		/*
		 * Check VRAM command queue empty and 2D + 3D engines idle.
		 */

		if (pending_flush_types & DRM_VIA_FENCE_TYPE_ACCEL) {
			status = VIA_READ(VIA_REG_STATUS);
			if ((status & VIA_VR_QUEUE_BUSY) &&
			    !(status & (VIA_CMD_RGTR_BUSY | VIA_2D_ENG_BUSY | VIA_3D_ENG_BUSY)))
				signaled_flush_types |= DRM_VIA_FENCE_TYPE_ACCEL;
		}

		if (signaled_flush_types) {
			pending_flush_types &= ~signaled_flush_types;
			if (!pending_flush_types && dev_priv->have_idlelock) {
				drm_idlelock_release(&dev->lock);
				dev_priv->have_idlelock = 0;
			}
			drm_fence_handler(dev, 0, dev_priv->emit_0_sequence, signaled_flush_types);
		}
	}

	spin_unlock(&dev_priv->fence_lock);

	return fc->pending_flush |
		((fc->pending_exe_flush) ? DRM_FENCE_TYPE_EXE : 0);
}


/**
 * Emit a fence sequence.
 */

int via_fence_emit_sequence(drm_device_t * dev, uint32_t class, uint32_t flags,
			     uint32_t * sequence, uint32_t * native_type)
{
	drm_via_private_t *dev_priv = (drm_via_private_t *) dev->dev_private;
	int ret = 0;

	if (!dev_priv)
		return -EINVAL;

	switch(class) {
	case 0: /* AGP command stream */

		/*
		 * The sequence number isn't really used by the hardware yet.
		 */

		spin_lock(&dev_priv->fence_lock);
		*sequence = ++dev_priv->emit_0_sequence;
		spin_unlock(&dev_priv->fence_lock);

		/*
		 * When drm_fence_handler() is called with flush type 0x01, and a
		 * sequence number, That means that the EXE flag is expired.
		 * Nothing else. No implicit flushing or other engines idle.
		 */

		*native_type = DRM_FENCE_TYPE_EXE;
		break;
	default:
		ret = DRM_ERR(EINVAL);
		break;
	}
	return ret;
}

/**
 * Manual poll (from the fence manager).
 */

void via_poke_flush(drm_device_t * dev, uint32_t class)
{
	drm_via_private_t *dev_priv = (drm_via_private_t *) dev->dev_private;
	drm_fence_manager_t *fm = &dev->fm;
	unsigned long flags;
	uint32_t pending_flush;

	if (!dev_priv)
		return ;

	write_lock_irqsave(&fm->lock, flags);
	pending_flush = via_perform_flush(dev, class);
	if (pending_flush)
		pending_flush = via_perform_flush(dev, class);
	write_unlock_irqrestore(&fm->lock, flags);

	/*
	 * Kick the timer if there are more flushes pending.
	 */

	if (pending_flush && !timer_pending(&dev_priv->fence_timer)) {
		dev_priv->fence_timer.expires = jiffies + 1;
		add_timer(&dev_priv->fence_timer);
	}
}

/**
 * No irq fence expirations implemented yet.
 * Although both the HQV engines and PCI dmablit engines signal
 * idle with an IRQ, we haven't implemented this yet.
 * This means that the drm fence manager will always poll for engine idle,
 * unless the caller wanting to wait for a fence object has indicated a lazy wait.
 */

int via_fence_has_irq(struct drm_device * dev, uint32_t class,
		      uint32_t flags)
{
	return 0;
}

/**
 * Regularly call the flush function. This enables lazy waits, so we can
 * set lazy_capable. Lazy waits don't really care when the fence expires,
 * so a timer tick delay should be fine.
 */

void via_fence_timer(unsigned long data)
{
	drm_device_t *dev = (drm_device_t *) data;
	drm_via_private_t *dev_priv = (drm_via_private_t *) dev->dev_private;
	drm_fence_manager_t *fm = &dev->fm;
	uint32_t pending_flush;
	drm_fence_class_manager_t *fc = &dev->fm.class[0];

	if (!dev_priv)
		return;
	if (!fm->initialized)
		goto out_unlock;

	via_poke_flush(dev, 0);
	pending_flush = fc->pending_flush |
		((fc->pending_exe_flush) ? DRM_FENCE_TYPE_EXE : 0);

	/*
	 * disable timer if there are no more flushes pending.
	 */

	if (!pending_flush && timer_pending(&dev_priv->fence_timer)) {
		BUG_ON(dev_priv->have_idlelock);
		del_timer(&dev_priv->fence_timer);
	}
	return;
out_unlock:
	return;

}
