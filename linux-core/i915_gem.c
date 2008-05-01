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
i915_gem_object_free_page_list(struct drm_device *dev,
			       struct drm_gem_object *obj)
{
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	int page_count = obj->size / PAGE_SIZE;
	int i;

	if (obj_priv->page_list == NULL)
		return;

	/* Count how many we had successfully allocated, since release_pages()
	 * doesn't like NULLs.
	 */
	for (i = 0; i < obj->size / PAGE_SIZE; i++) {
		if (obj_priv->page_list[i] == NULL)
			break;
	}
	release_pages(obj_priv->page_list, i, 0);

	drm_free(obj_priv->page_list,
		 page_count * sizeof(struct page *),
		 DRM_MEM_DRIVER);
	obj_priv->page_list = NULL;
}

/**
 * Unbinds an object from the GTT aperture.
 */
static void
i915_gem_object_unbind(struct drm_device *dev, struct drm_gem_object *obj)
{
	struct drm_i915_gem_object *obj_priv = obj->driver_private;

	if (obj_priv->agp_mem != NULL) {
		drm_unbind_agp(obj_priv->agp_mem);
		drm_free_agp(obj_priv->agp_mem, obj->size / PAGE_SIZE);
	}

	i915_gem_object_free_page_list(dev, obj);

	drm_memrange_put_block(obj_priv->gtt_space);
}

/**
 * Finds free space in the GTT aperture and binds the object there.
 */
static int
i915_gem_object_bind_to_gtt(struct drm_device *dev, struct drm_gem_object *obj)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	struct drm_memrange_node *free_space;
	int page_count, i;

	free_space = drm_memrange_search_free(&dev_priv->mm.gtt_space,
					      obj->size,
					      PAGE_SIZE, 0);

	obj_priv->gtt_space = drm_memrange_get_block(free_space,
						     obj->size,
						     PAGE_SIZE);

	/* Get the list of pages out of our struct file.  They'll be pinned
	 * at this point until we release them.
	 */
	page_count = obj->size / PAGE_SIZE;
	BUG_ON(obj_priv->page_list != NULL);
	obj_priv->page_list = drm_calloc(page_count, sizeof(struct page *),
					 DRM_MEM_DRIVER);
	for (i = 0; i < page_count; i++) {
		obj_priv->page_list[i] =
		    find_or_create_page(obj->filp->f_mapping, i, GFP_HIGHUSER);

		if (obj_priv->page_list[i] == NULL) {
			i915_gem_object_free_page_list(dev, obj);
			return -ENOMEM;
		}
	}

	/* Create an AGP memory structure pointing at our pages, and bind it
	 * into the GTT.
	 */
	obj_priv->agp_mem = drm_agp_bind_pages(dev,
					       obj_priv->page_list,
					       page_count,
					       obj_priv->gtt_offset);
	if (obj_priv->agp_mem == NULL) {
		i915_gem_object_free_page_list(dev, obj);
		return -ENOMEM;
	}

	return 0;
}

static int
i915_gem_reloc_and_validate_object(struct drm_device *dev,
				   struct drm_file *file_priv,
				   struct drm_i915_gem_validate_entry *entry,
				   struct drm_gem_object *obj)
{
	struct drm_i915_gem_reloc *relocs;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;

	/* Walk the list of relocations and perform them if necessary. */
	/* XXX */

	/* Choose the GTT offset for our buffer and put it there. */
	if (obj_priv->gtt_space == NULL) {
		i915_gem_object_bind_to_gtt(dev, obj);
		if (obj_priv->gtt_space == NULL)
			return -ENOMEM;
	}

	return 0;
}

static int
evict_callback(struct drm_memrange_node *node, void *data)
{
	struct drm_device *dev = data;
	struct drm_gem_object *obj = node->private;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;

	if (obj_priv->pin_count == 0)
		i915_gem_object_unbind(dev, obj);

	return 0;
}

int
i915_gem_execbuffer(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_execbuffer *args = data;
	struct drm_i915_gem_validate_entry *validate_list;
	struct drm_gem_object **object_list;
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
	drm_memrange_for_each(&dev_priv->mm.gtt_space, evict_callback, dev);

	/* Copy in the validate list from userland */
	validate_list = drm_calloc(sizeof(*validate_list), args->buffer_count,
				   DRM_MEM_DRIVER);
	object_list = drm_calloc(sizeof(*object_list), args->buffer_count,
				 DRM_MEM_DRIVER);
	if (validate_list == NULL || object_list == NULL) {
		ret = -ENOMEM;
		goto err;
	}
	ret = copy_from_user(validate_list,
			     (struct drm_i915_relocation_entry __user*)
			     args->buffers,
			     sizeof(*validate_list) * args->buffer_count);
	if (ret != 0)
		goto err;

	/* Look up object handles and perform the relocations */
	for (i = 0; i < args->buffer_count; i++) {
		object_list[i] = drm_gem_object_lookup(dev, file_priv,
						       validate_list[i].buffer_handle);
		if (object_list[i] == NULL) {
			ret = -EINVAL;
			goto err;
		}

		i915_gem_reloc_and_validate_object(dev, file_priv,
						   &validate_list[i],
						   object_list[i]);
	}

	/* Exec the batchbuffer */

	/* Copy the new buffer offsets back to the user's validate list. */
	for (i = 0; i < args->buffer_count; i++) {
		struct drm_i915_gem_object *obj_priv =
			object_list[i]->driver_private;

		validate_list[i].buffer_offset = obj_priv->gtt_offset;
	}
	ret = copy_to_user(validate_list,
			     (struct drm_i915_relocation_entry __user*)
			     args->buffers,
			     sizeof(*validate_list) * args->buffer_count);

	/* Clean up and return */
err:
	if (object_list != NULL) {
		for (i = 0; i < args->buffer_count; i++)
			drm_gem_object_unreference(dev, object_list[i]);
	}
	drm_free(object_list, sizeof(*object_list) * args->buffer_count,
		 DRM_MEM_DRIVER);
	drm_free(validate_list, sizeof(*validate_list) * args->buffer_count,
		 DRM_MEM_DRIVER);

	return ret;
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

