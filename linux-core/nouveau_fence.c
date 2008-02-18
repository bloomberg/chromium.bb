/*
 * Copyright (C) 2007 Ben Skeggs.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "drmP.h"
#include "drm.h"
#include "nouveau_drv.h"
#include "nouveau_dma.h"

static int
nouveau_fence_has_irq(struct drm_device *dev, uint32_t class, uint32_t flags)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	DRM_DEBUG("class=%d, flags=0x%08x\n", class, flags);

	/* DRM's channel always uses IRQs to signal fences */
	if (class == dev_priv->channel.chan->id)
		return 1;

	/* Other channels don't use IRQs at all yet */
	return 0;
}

static int
nouveau_fence_emit(struct drm_device *dev, uint32_t class, uint32_t flags,
		   uint32_t *breadcrumb, uint32_t *native_type)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nouveau_channel *chan = dev_priv->fifos[class];
	struct nouveau_drm_channel *dchan = &dev_priv->channel;

	DRM_DEBUG("class=%d, flags=0x%08x\n", class, flags);

	/* We can't emit fences on client channels, update sequence number
	 * and userspace will emit the fence
	 */
	*breadcrumb  = ++chan->next_sequence;
	*native_type = DRM_FENCE_TYPE_EXE;
	if (chan != dchan->chan) {
		DRM_DEBUG("user fence 0x%08x\n", *breadcrumb);
		return 0;
	}

	DRM_DEBUG("emit 0x%08x\n", *breadcrumb);
	BEGIN_RING(NvSubM2MF, NV_MEMORY_TO_MEMORY_FORMAT_SET_REF, 1);
	OUT_RING  (*breadcrumb);
	BEGIN_RING(NvSubM2MF, 0x0150, 1);
	OUT_RING  (0);
	FIRE_RING ();

	return 0;
}

static void
nouveau_fence_poll(struct drm_device *dev, uint32_t class, uint32_t waiting_types)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct drm_fence_class_manager *fc = &dev->fm.fence_class[class];
	struct nouveau_channel *chan = dev_priv->fifos[class];
	uint32_t pending_types = 0;

	DRM_DEBUG("class=%d\n", class);
	DRM_DEBUG("pending: 0x%08x 0x%08x\n", waiting_types, fc->waiting_types);

	if (pending_types) {
		uint32_t sequence = NV_READ(chan->ref_cnt);

		DRM_DEBUG("got 0x%08x\n", sequence);
		drm_fence_handler(dev, class, sequence, waiting_types, 0);
	}
}

void
nouveau_fence_handler(struct drm_device *dev, int channel)
{
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_class_manager *fc = &fm->fence_class[channel];

	DRM_DEBUG("class=%d\n", channel);

	write_lock(&fm->lock);
	nouveau_fence_poll(dev, channel, fc->waiting_types);
	write_unlock(&fm->lock);
}

struct drm_fence_driver nouveau_fence_driver = {
	.num_classes	= 8,
	.wrap_diff	= (1 << 30),
	.flush_diff	= (1 << 29),
	.sequence_mask	= 0xffffffffU,
	.has_irq	= nouveau_fence_has_irq,
	.emit		= nouveau_fence_emit,
	.flush          = NULL,
	.poll           = nouveau_fence_poll,
	.needed_flush   = NULL,
	.wait           = NULL
};
