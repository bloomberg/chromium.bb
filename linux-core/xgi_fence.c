/*
 * (C) Copyright IBM Corporation 2007
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Ian Romanick <idr@us.ibm.com>
 */

#include "xgi_drv.h"
#include "xgi_regs.h"
#include "xgi_misc.h"
#include "xgi_cmdlist.h"

static void xgi_fence_poll(struct drm_device * dev, uint32_t class, 
			   uint32_t waiting_types)
{
	struct xgi_info * info = dev->dev_private;
	uint32_t signaled_types = 0;


	if ((info == NULL) || (class != 0))
		return;

	DRM_SPINLOCK(&info->fence_lock);

	if (waiting_types) {
		if (waiting_types & DRM_FENCE_TYPE_EXE) {
			const u32 begin_id = le32_to_cpu(DRM_READ32(info->mmio_map,
							0x2820))
				& BEGIN_BEGIN_IDENTIFICATION_MASK;

			if (begin_id != info->complete_sequence) {
				info->complete_sequence = begin_id;
				signaled_types |= DRM_FENCE_TYPE_EXE;
			}
		}

		if (signaled_types) {
			drm_fence_handler(dev, 0, info->complete_sequence,
					  signaled_types, 0);
		}
	}

	DRM_SPINUNLOCK(&info->fence_lock);
}


int xgi_fence_emit_sequence(struct drm_device * dev, uint32_t class,
			    uint32_t flags, uint32_t * sequence,
			    uint32_t * native_type)
{
	struct xgi_info * info = dev->dev_private;

	if ((info == NULL) || (class != 0))
		return -EINVAL;


	DRM_SPINLOCK(&info->fence_lock);
	info->next_sequence++;
	if (info->next_sequence > BEGIN_BEGIN_IDENTIFICATION_MASK) {
		info->next_sequence = 1;
	}
	DRM_SPINUNLOCK(&info->fence_lock);


	xgi_emit_irq(info);

	*sequence = (uint32_t) info->next_sequence;
	*native_type = DRM_FENCE_TYPE_EXE;

	return 0;
}


void xgi_fence_handler(struct drm_device * dev)
{
	struct drm_fence_manager * fm = &dev->fm;
	struct drm_fence_class_manager *fc = &fm->fence_class[0];

	write_lock(&fm->lock);
	xgi_fence_poll(dev, 0, fc->waiting_types);
	write_unlock(&fm->lock);
}


int xgi_fence_has_irq(struct drm_device *dev, uint32_t class, uint32_t flags)
{
	return ((class == 0) && (flags == DRM_FENCE_TYPE_EXE)) ? 1 : 0;
}

struct drm_fence_driver xgi_fence_driver = {
	.num_classes = 1,
	.wrap_diff = BEGIN_BEGIN_IDENTIFICATION_MASK,
	.flush_diff = BEGIN_BEGIN_IDENTIFICATION_MASK - 1,
	.sequence_mask = BEGIN_BEGIN_IDENTIFICATION_MASK,
	.has_irq = xgi_fence_has_irq,
	.emit = xgi_fence_emit_sequence,
	.flush = NULL,
	.poll = xgi_fence_poll,
	.needed_flush = NULL,
	.wait = NULL
};

