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
i915_gem_object_free_page_list(struct drm_gem_object *obj)
{
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	int page_count = obj->size / PAGE_SIZE;
	int i;

	if (obj_priv->page_list == NULL)
		return;

	
	for (i = 0; i < page_count; i++) {
		if (obj_priv->page_list[i] != NULL) {
			unlock_page (obj_priv->page_list[i]);
			page_cache_release (obj_priv->page_list[i]);
		}
	}
	drm_free(obj_priv->page_list,
		 page_count * sizeof(struct page *),
		 DRM_MEM_DRIVER);
	obj_priv->page_list = NULL;
}

/**
 * Unbinds an object from the GTT aperture.
 */
static void
i915_gem_object_unbind(struct drm_gem_object *obj)
{
	struct drm_i915_gem_object *obj_priv = obj->driver_private;

	if (obj_priv->gtt_space == NULL)
		return;

	if (obj_priv->agp_mem != NULL) {
		drm_unbind_agp(obj_priv->agp_mem);
		drm_free_agp(obj_priv->agp_mem, obj->size / PAGE_SIZE);
	}

	i915_gem_object_free_page_list(obj);

	drm_memrange_put_block(obj_priv->gtt_space);
	obj_priv->gtt_space = NULL;
}

/**
 * Finds free space in the GTT aperture and binds the object there.
 */
static int
i915_gem_object_bind_to_gtt(struct drm_gem_object *obj, unsigned alignment)
{
	struct drm_device *dev = obj->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	struct drm_memrange_node *free_space;
	int page_count, i;

	if (alignment == 0)
		alignment = PAGE_SIZE;
	if (alignment & (PAGE_SIZE - 1)) {
		DRM_ERROR("Invalid object alignment requested %u\n", alignment);
		return -EINVAL;
	}

	free_space = drm_memrange_search_free(&dev_priv->mm.gtt_space,
					      obj->size,
					      alignment, 0);
	if (free_space == NULL)
		return -ENOMEM;
	obj_priv->gtt_space = drm_memrange_get_block(free_space,
						     obj->size,
						     alignment);
	if (obj_priv->gtt_space == NULL)
		return -ENOMEM;

	obj_priv->gtt_offset = obj_priv->gtt_space->start;

	DRM_DEBUG("Binding object of size %d at 0x%08x\n", obj->size, obj_priv->gtt_offset);

	/* Get the list of pages out of our struct file.  They'll be pinned
	 * at this point until we release them.
	 */
	page_count = obj->size / PAGE_SIZE;
	BUG_ON(obj_priv->page_list != NULL);
	obj_priv->page_list = drm_calloc(page_count, sizeof(struct page *),
					 DRM_MEM_DRIVER);
	if (obj_priv->page_list == NULL) {
		drm_memrange_put_block(obj_priv->gtt_space);
		obj_priv->gtt_space = NULL;
		return -ENOMEM;
	}

	for (i = 0; i < page_count; i++) {
		obj_priv->page_list[i] =
		    find_or_create_page(obj->filp->f_mapping, i, GFP_HIGHUSER);

		if (obj_priv->page_list[i] == NULL) {
			i915_gem_object_free_page_list(obj);
			drm_memrange_put_block(obj_priv->gtt_space);
			obj_priv->gtt_space = NULL;
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
		i915_gem_object_free_page_list(obj);
		drm_memrange_put_block(obj_priv->gtt_space);
		obj_priv->gtt_space = NULL;
		return -ENOMEM;
	}

	return 0;
}

static int
i915_gem_reloc_and_validate_object(struct drm_gem_object *obj,
				   struct drm_file *file_priv,
				   struct drm_i915_gem_validate_entry *entry)
{
	struct drm_device *dev = obj->dev;
	struct drm_i915_gem_relocation_entry reloc;
	struct drm_i915_gem_relocation_entry __user *relocs;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	int i;

	/* Choose the GTT offset for our buffer and put it there. */
	if (obj_priv->gtt_space == NULL) {
		i915_gem_object_bind_to_gtt(obj, (unsigned) entry->alignment);
		if (obj_priv->gtt_space == NULL)
			return -ENOMEM;
	}

	relocs = (struct drm_i915_gem_relocation_entry __user *) (uintptr_t) entry->relocs_ptr;
	/* Apply the relocations, using the GTT aperture to avoid cache
	 * flushing requirements.
	 */
	for (i = 0; i < entry->relocation_count; i++) {
		struct drm_gem_object *target_obj;
		struct drm_i915_gem_object *target_obj_priv;
		void *reloc_page;
		uint32_t reloc_val, *reloc_entry;
		int ret;

		ret = copy_from_user(&reloc, relocs + i, sizeof(reloc));
		if (ret != 0)
			return ret;

		target_obj = drm_gem_object_lookup(obj->dev, file_priv,
						   reloc.target_handle);
		if (target_obj == NULL)
			return -EINVAL;
		target_obj_priv = target_obj->driver_private;

		/* The target buffer should have appeared before us in the
		 * validate list, so it should have a GTT space bound by now.
		 */
		if (target_obj_priv->gtt_space == NULL) {
			DRM_ERROR("No GTT space found for object %d\n",
				  reloc.target_handle);
			drm_gem_object_unreference (target_obj);
			return -EINVAL;
		}

		if (reloc.offset > obj->size - 4) {
			DRM_ERROR("Relocation beyond object bounds.\n");
			drm_gem_object_unreference (target_obj);
			return -EINVAL;
		}
		if (reloc.offset & 3) {
			DRM_ERROR("Relocation not 4-byte aligned.\n");
			drm_gem_object_unreference (target_obj);
			return -EINVAL;
		}

		/* Map the page containing the relocation we're going to
		 * perform.
		 */
		reloc_page = ioremap(dev->agp->base +
				     (reloc.offset & ~(PAGE_SIZE - 1)),
				     PAGE_SIZE);
		if (reloc_page == NULL)
		{
			drm_gem_object_unreference (target_obj);
			return -ENOMEM;
		}

		reloc_entry = (uint32_t *)((char *)reloc_page +
					   (reloc.offset & (PAGE_SIZE - 1)));
		reloc_val = target_obj_priv->gtt_offset + reloc.delta;

		DRM_DEBUG("Applied relocation: %p@0x%08x = 0x%08x\n",
			  obj, (unsigned int) reloc.offset, reloc_val);
		*reloc_entry = reloc_val;

		iounmap(reloc_page);
		drm_gem_object_unreference (target_obj);
	}

	return 0;
}

static int
evict_callback(struct drm_memrange_node *node, void *data)
{
	struct drm_gem_object *obj = node->private;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;

	if (obj_priv->pin_count == 0)
		i915_gem_object_unbind(obj);

	return 0;
}

static int
i915_gem_sync_and_evict(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret;
	RING_LOCALS;

	BEGIN_LP_RING(2);
	OUT_RING(CMD_MI_FLUSH | MI_READ_FLUSH | MI_EXE_FLUSH);
	OUT_RING(0); /* noop */
	ADVANCE_LP_RING();
	ret = i915_quiescent(dev);
	if (ret != 0)
		return ret;

	/* Evict everything so we have space for sure. */
	drm_memrange_for_each(&dev_priv->mm.gtt_space, evict_callback, NULL);

	return 0;
}

int
i915_gem_execbuffer(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct drm_i915_gem_execbuffer *args = data;
	struct drm_i915_gem_validate_entry *validate_list;
	struct drm_gem_object **object_list;
	int ret, i;

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	DRM_INFO ("%s:%d\n", __FUNCTION__, __LINE__);
	/* Big hammer: flush and idle the hardware so we can map things in/out.
	 */
	ret = i915_gem_sync_and_evict(dev);
	if (ret != 0)
		return ret;

	DRM_INFO ("%s:%d\n", __FUNCTION__, __LINE__);
	/* Copy in the validate list from userland */
	validate_list = drm_calloc(sizeof(*validate_list), args->buffer_count,
				   DRM_MEM_DRIVER);
	object_list = drm_calloc(sizeof(*object_list), args->buffer_count,
				 DRM_MEM_DRIVER);
	if (validate_list == NULL || object_list == NULL) {
		ret = -ENOMEM;
		goto err;
	}
	DRM_INFO ("%s:%d\n", __FUNCTION__, __LINE__);
	ret = copy_from_user(validate_list,
			     (struct drm_i915_relocation_entry __user*)(uintptr_t)
			     args->buffers_ptr,
			     sizeof(*validate_list) * args->buffer_count);
	if (ret != 0)
		goto err;

	DRM_INFO ("%s:%d\n", __FUNCTION__, __LINE__);
	/* Look up object handles and perform the relocations */
	for (i = 0; i < args->buffer_count; i++) {
		object_list[i] = drm_gem_object_lookup(dev, file_priv,
						       validate_list[i].buffer_handle);
		if (object_list[i] == NULL) {
			ret = -EINVAL;
			goto err;
		}

		i915_gem_reloc_and_validate_object(object_list[i], file_priv,
						   &validate_list[i]);
	}

	/* Exec the batchbuffer */

	DRM_INFO ("%s:%d\n", __FUNCTION__, __LINE__);
	/* Copy the new buffer offsets back to the user's validate list. */
	for (i = 0; i < args->buffer_count; i++) {
		struct drm_i915_gem_object *obj_priv =
			object_list[i]->driver_private;

		validate_list[i].buffer_offset = obj_priv->gtt_offset;
	}
	DRM_INFO ("%s:%d\n", __FUNCTION__, __LINE__);
	ret = copy_to_user(validate_list,
			     (struct drm_i915_relocation_entry __user*)(uintptr_t)
			     args->buffers_ptr,
			     sizeof(*validate_list) * args->buffer_count);

	DRM_INFO ("%s:%d\n", __FUNCTION__, __LINE__);
	/* Clean up and return */
	ret = i915_gem_sync_and_evict(dev);

	DRM_INFO ("%s:%d\n", __FUNCTION__, __LINE__);
err:
	if (object_list != NULL) {
		for (i = 0; i < args->buffer_count; i++)
			drm_gem_object_unreference(object_list[i]);
	}
	DRM_INFO ("%s:%d\n", __FUNCTION__, __LINE__);
	drm_free(object_list, sizeof(*object_list) * args->buffer_count,
		 DRM_MEM_DRIVER);
	drm_free(validate_list, sizeof(*validate_list) * args->buffer_count,
		 DRM_MEM_DRIVER);

	DRM_INFO ("%s:%d\n", __FUNCTION__, __LINE__);
	return ret;
}

int
i915_gem_pin_ioctl(struct drm_device *dev, void *data,
		   struct drm_file *file_priv)
{
	struct drm_i915_gem_pin *args = data;
	struct drm_gem_object *obj;
	struct drm_i915_gem_object *obj_priv;
	int ret;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL) {
		DRM_ERROR("Bad handle in i915_gem_pin_ioctl(): %d\n",
			  args->handle);
		return -EINVAL;
	}

	obj_priv = obj->driver_private;
	if (obj_priv->gtt_space == NULL)
	{
		ret = i915_gem_object_bind_to_gtt(obj, (unsigned) args->alignment);
		if (ret != 0) {
			DRM_ERROR("Failure to bind in i915_gem_pin_ioctl(): %d\n",
				  ret);
			drm_gem_object_unreference (obj);
			return ret;
		}
	}

	obj_priv->pin_count++;
	args->offset = obj_priv->gtt_offset;
	drm_gem_object_unreference (obj);

	return 0;
}

int
i915_gem_unpin_ioctl(struct drm_device *dev, void *data,
		     struct drm_file *file_priv)
{
	struct drm_i915_gem_pin *args = data;
	struct drm_gem_object *obj;
	struct drm_i915_gem_object *obj_priv;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL) {
		DRM_ERROR("Bad handle in i915_gem_unpin_ioctl(): %d\n",
			  args->handle);
		return -EINVAL;
	}

	obj_priv = obj->driver_private;
	obj_priv->pin_count--;
	drm_gem_object_unreference (obj);
	return 0;
}

int i915_gem_init_object(struct drm_gem_object *obj)
{
	struct drm_i915_gem_object *obj_priv;

	obj_priv = drm_calloc(1, sizeof(*obj_priv), DRM_MEM_DRIVER);
	if (obj_priv == NULL)
		return -ENOMEM;

	obj->driver_private = obj_priv;

	return 0;
}

void i915_gem_free_object(struct drm_gem_object *obj)
{
	i915_gem_object_unbind(obj);

	drm_free(obj->driver_private, 1, DRM_MEM_DRIVER);
}
