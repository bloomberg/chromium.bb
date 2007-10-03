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

static uint32_t xgi_do_flush(struct drm_device * dev, uint32_t class)
{
	struct xgi_info * info = dev->dev_private;
	struct drm_fence_class_manager * fc = &dev->fm.fence_class[class];
	uint32_t pending_flush_types = 0;
	uint32_t signaled_flush_types = 0;


	if ((info == NULL) || (class != 0))
		return 0;

	DRM_SPINLOCK(&info->fence_lock);

	pending_flush_types = fc->pending_flush |
		((fc->pending_exe_flush) ? DRM_FENCE_TYPE_EXE : 0);

	if (pending_flush_types) {
		if (pending_flush_types & DRM_FENCE_TYPE_EXE) {
			const u32 begin_id = le32_to_cpu(DRM_READ32(info->mmio_map,
							0x2820))
				& BEGIN_BEGIN_IDENTIFICATION_MASK;

			if (begin_id != info->complete_sequence) {
				info->complete_sequence = begin_id;
				signaled_flush_types |= DRM_FENCE_TYPE_EXE;
			}
		}

		if (signaled_flush_types) {
			drm_fence_handler(dev, 0, info->complete_sequence,
					  signaled_flush_types);
		}
	}

	DRM_SPINUNLOCK(&info->fence_lock);

	return fc->pending_flush |
		((fc->pending_exe_flush) ? DRM_FENCE_TYPE_EXE : 0);
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


void xgi_poke_flush(struct drm_device * dev, uint32_t class)
{
	struct drm_fence_manager * fm = &dev->fm;
	unsigned long flags;


	write_lock_irqsave(&fm->lock, flags);
	xgi_do_flush(dev, class);
	write_unlock_irqrestore(&fm->lock, flags);
}


void xgi_fence_handler(struct drm_device * dev)
{
	struct drm_fence_manager * fm = &dev->fm;


	write_lock(&fm->lock);
	xgi_do_flush(dev, 0);
	write_unlock(&fm->lock);
}


int xgi_fence_has_irq(struct drm_device *dev, uint32_t class, uint32_t flags)
{
	return ((class == 0) && (flags == DRM_FENCE_TYPE_EXE)) ? 1 : 0;
}
