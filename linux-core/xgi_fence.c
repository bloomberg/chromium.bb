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

static int xgi_low_level_fence_emit(struct drm_device *dev, u32 *sequence)
{
	struct xgi_info *const info = dev->dev_private;

	if (info == NULL) {
		DRM_ERROR("called with no initialization\n");
		return -EINVAL;
	}

	DRM_SPINLOCK(&info->fence_lock);
	info->next_sequence++;
	if (info->next_sequence > BEGIN_BEGIN_IDENTIFICATION_MASK) {
		info->next_sequence = 1;
	}

	*sequence = (u32) info->next_sequence;
	DRM_SPINUNLOCK(&info->fence_lock);


	xgi_emit_irq(info);
	return 0;
}

#define GET_BEGIN_ID(i) (le32_to_cpu(DRM_READ32((i)->mmio_map, 0x2820)) \
				 & BEGIN_BEGIN_IDENTIFICATION_MASK)

static int xgi_low_level_fence_wait(struct drm_device *dev, unsigned *sequence)
{
	struct xgi_info *const info = dev->dev_private;
	unsigned int cur_fence;
	int ret = 0;

	if (info == NULL) {
		DRM_ERROR("called with no initialization\n");
		return -EINVAL;
	}

	/* Assume that the user has missed the current sequence number
	 * by about a day rather than she wants to wait for years
	 * using fences.
	 */
	DRM_WAIT_ON(ret, info->fence_queue, 3 * DRM_HZ,
		    ((((cur_fence = GET_BEGIN_ID(info))
		      - *sequence) & BEGIN_BEGIN_IDENTIFICATION_MASK)
		     <= (1 << 18)));

	info->complete_sequence = cur_fence;
	*sequence = cur_fence;

	return ret;
}


int xgi_set_fence_ioctl(struct drm_device * dev, void * data,
			struct drm_file * filp)
{
	(void) filp;
	return xgi_low_level_fence_emit(dev, (u32 *) data);
}


int xgi_wait_fence_ioctl(struct drm_device * dev, void * data,
			 struct drm_file * filp)
{
	(void) filp;
	return xgi_low_level_fence_wait(dev, (u32 *) data);
}
