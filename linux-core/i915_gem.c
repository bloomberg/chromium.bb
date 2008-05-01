/*
 * Copyright © 2008 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

#include "drmP.h"
#include "drm.h"
#include "i915_drm.h"
#include "i915_drv.h"

int
i915_gem_init_ioctl(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_init *args = data;

	if (args->gtt_start >= args->gtt_end ||
	    (args->gtt_start & (PAGE_SIZE - 1)) != 0 ||
	    (args->gtt_end & (PAGE_SIZE - 1)) != 0)
		return -EINVAL;

	drm_memrange_init(&dev_priv->mm.gtt_space, args->gtt_start,
	    args->gtt_end);

	return 0;
}

static void
i915_gem_evict_object(struct drm_device *dev, struct drm_gem_object *obj)
{
	
}

static int
evict_callback(struct drm_memrange_node *node, void *data)
{
	struct drm_device *dev = data;
	struct drm_gem_object *obj = node->private;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;

	if (obj_priv->pin_count == 0)
		i915_gem_evict_object(dev, obj);

	return 0;
}

int
i915_gem_execbuffer(struct drm_device *dev, void *data,
		   struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_execbuffer *args = data;
	struct drm_i915_gem_validate_entry *validate_list;
	int ret, i;
	RING_LOCALS;

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	/* Big hammer: flush and idle the hardware so we can map things in/out.
	 */
	BEGIN_LP_RING(2);
	OUT_RING(CMD_MI_FLUSH | MI_READ_FLUSH | MI_EXE_FLUSH);
	OUT_RING(0); /* noop */
	ADVANCE_LP_RING();
	ret = i915_quiescent(dev);
	if (ret != 0)
		return ret;

	/* Evict everything so we have space for sure. */
	drm_memrange_for_each(&dev_priv->mm.gtt_space, evict_callback);

	/* Copy in the validate list from userland */
	validate_list = drm_calloc(sizeof(*validate_list), args->buffer_count,
				   DRM_MEM_DRIVER);
	ret = copy_from_user(validate_list,
			     (struct drm_i915_relocation_entry __user*)
			     args->buffers,
			     sizeof(*validate_list) * args->buffer_count);
	if (ret != 0) {
		drm_free(validate_list,
			 sizeof(*validate_list) * args->buffer_count,
			 DRM_MEM_DRIVER);
		return ret;
	}

	/* Perform the relocations */
	for (i = 0; i < args->buffer_count; i++) {
		intel_gem_reloc_and_validate_buffer(dev, &validate_list[i]);
	}

	/* Exec the batchbuffer */


	drm_free(validate_list, sizeof(*validate_list) * args->buffer_count,
		 DRM_MEM_DRIVER);

	return 0;
}

int i915_gem_init_object(struct drm_device *dev, struct drm_gem_object *obj)
{
	struct drm_i915_gem_object *obj_priv;

	obj_priv = drm_calloc(1, sizeof(*obj_priv), DRM_MEM_DRIVER);
	if (obj_priv == NULL)
		return -ENOMEM;

	obj->driver_private = obj_priv;

	return 0;
}

void i915_gem_free_object(struct drm_device *dev, struct drm_gem_object *obj)
{
	drm_free(obj->driver_private, 1, DRM_MEM_DRIVER);
}

