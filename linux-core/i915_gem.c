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

	DRM_INFO ("%s:%d %p\n", __FUNCTION__, __LINE__, obj);
	DRM_INFO ("gtt_space %p\n", obj_priv->gtt_space);
	if (obj_priv->gtt_space == NULL)
		return;

	DRM_INFO ("agp_mem %p %ld pages\n", obj_priv->agp_mem, obj->size / PAGE_SIZE);
	if (obj_priv->agp_mem != NULL) {
		drm_unbind_agp(obj_priv->agp_mem);
		drm_free_agp(obj_priv->agp_mem, obj->size / PAGE_SIZE);
	}

	DRM_INFO ("free_page_list\n");
	i915_gem_object_free_page_list(obj);

	DRM_INFO ("put_block\n");
	drm_memrange_put_block(obj_priv->gtt_space);
	obj_priv->gtt_space = NULL;
	DRM_INFO ("done\n");
}

static void
i915_gem_dump_object (struct drm_gem_object *obj, int len, const char *where)
{
	struct drm_device *dev = obj->dev;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	uint32_t		*mem = kmap_atomic (obj_priv->page_list[0], KM_USER0);
	volatile uint32_t	*gtt = ioremap(dev->agp->base + obj_priv->gtt_offset,
					       PAGE_SIZE);
	int			i;

	DRM_INFO ("%s: object at offset %08x\n", where, obj_priv->gtt_offset);
	for (i = 0; i < len/4; i++)
		DRM_INFO ("%3d: mem %08x gtt %08x\n", i, mem[i], readl(gtt + i));
	iounmap (gtt);
	kunmap_atomic (mem, KM_USER0);
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
	obj_priv->gtt_space->private = obj;
	obj_priv->gtt_offset = obj_priv->gtt_space->start;

	DRM_INFO ("Binding object of size %d at 0x%08x\n", obj->size, obj_priv->gtt_offset);

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
	
	drm_ttm_cache_flush (obj_priv->page_list, page_count);
	DRM_MEMORYBARRIER();
	drm_agp_chipset_flush(dev);

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
	entry->buffer_offset = obj_priv->gtt_offset;

	relocs = (struct drm_i915_gem_relocation_entry __user *) (uintptr_t) entry->relocs_ptr;
	/* Apply the relocations, using the GTT aperture to avoid cache
	 * flushing requirements.
	 */
	for (i = 0; i < entry->relocation_count; i++) {
		struct drm_gem_object *target_obj;
		struct drm_i915_gem_object *target_obj_priv;
		void *reloc_page;
		uint32_t reloc_val, reloc_offset, *reloc_entry;
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
		reloc_offset = obj_priv->gtt_offset + reloc.offset;
		reloc_page = ioremap(dev->agp->base +
				     (reloc_offset & ~(PAGE_SIZE - 1)),
				     PAGE_SIZE);
		if (reloc_page == NULL)
		{
			drm_gem_object_unreference (target_obj);
			return -ENOMEM;
		}

		reloc_entry = (uint32_t *)((char *)reloc_page +
					   (reloc_offset & (PAGE_SIZE - 1)));
		reloc_val = target_obj_priv->gtt_offset + reloc.delta;

		DRM_INFO("Applied relocation: %p@0x%08x %08x -> %08x\n",
			  obj, (unsigned int) reloc.offset,
			  readl (reloc_entry), reloc_val);
		writel (reloc_val, reloc_entry);

		iounmap(reloc_page);
		drm_gem_object_unreference (target_obj);
	}

	i915_gem_dump_object (obj, 128, __FUNCTION__);
	return 0;
}

static int
evict_callback(struct drm_memrange_node *node, void *data)
{
	struct drm_gem_object *obj;
	struct drm_i915_gem_object *obj_priv;
	
	DRM_INFO ("evict node %p\n", node);
	
	obj = node->private;
	DRM_INFO ("evict obj %p\n", obj);
	
	obj_priv = obj->driver_private;
	DRM_INFO ("evict priv %p\n", obj_priv);

	DRM_INFO ("pin_count %d\n", obj_priv->pin_count);
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

static int
i915_dispatch_gem_execbuffer (struct drm_device * dev,
			      struct drm_i915_gem_execbuffer * exec,
			      uint64_t exec_offset)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_clip_rect __user *boxes = (struct drm_clip_rect __user *) (uintptr_t) exec->cliprects_ptr;
	int nbox = exec->num_cliprects;
	int i = 0, count;
	uint32_t	exec_start, exec_len;
	RING_LOCALS;

	exec_start = (uint32_t) exec_offset + exec->batch_start_offset;
	exec_len = (uint32_t) exec->batch_len;
	
	if ((exec_start | exec_len) & 0x7) {
		DRM_ERROR("alignment\n");
		return -EINVAL;
	}

	i915_kernel_lost_context(dev);

	DRM_INFO ("execbuffer at %x+%d len %d\n",
		  (uint32_t) exec_offset, exec->batch_start_offset, exec_len);
	
	if (!exec_start)
		return -EINVAL;

	count = nbox ? nbox : 1;

	for (i = 0; i < count; i++) {
		if (i < nbox) {
			int ret = i915_emit_box(dev, boxes, i,
						exec->DR1, exec->DR4);
			if (ret)
				return ret;
		}

		if (dev_priv->use_mi_batchbuffer_start) {
			BEGIN_LP_RING(2);
			if (IS_I965G(dev)) {
				OUT_RING(MI_BATCH_BUFFER_START | (2 << 6) | MI_BATCH_NON_SECURE_I965);
				OUT_RING(exec_start);
			} else {
				OUT_RING(MI_BATCH_BUFFER_START | (2 << 6));
				OUT_RING(exec_start | MI_BATCH_NON_SECURE);
			}
			ADVANCE_LP_RING();

		} else {
			BEGIN_LP_RING(4);
			OUT_RING(MI_BATCH_BUFFER);
			OUT_RING(exec_start | MI_BATCH_NON_SECURE);
			OUT_RING(exec_start + exec_len - 4);
			OUT_RING(0);
			ADVANCE_LP_RING();
		}
	}

	/* XXX breadcrumb */
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
	uint64_t exec_offset;

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	DRM_INFO ("buffers_ptr %d buffer_count %d\n",
		  (int) args->buffers_ptr, args->buffer_count);
	i915_kernel_lost_context(dev);

	/* Big hammer: flush and idle the hardware so we can map things in/out.
	 */
	ret = i915_gem_sync_and_evict(dev);
	if (ret != 0) {
		DRM_ERROR ("i915_gem_sync_and_evict failed %d\n", ret);
		return ret;
	}

	/* Copy in the validate list from userland */
	validate_list = drm_calloc(sizeof(*validate_list), args->buffer_count,
				   DRM_MEM_DRIVER);
	object_list = drm_calloc(sizeof(*object_list), args->buffer_count,
				 DRM_MEM_DRIVER);
	if (validate_list == NULL || object_list == NULL) {
		DRM_ERROR ("Failed to allocate validate or object list for %d buffers\n",
			   args->buffer_count);
		ret = -ENOMEM;
		goto err;
	}
	ret = copy_from_user(validate_list,
			     (struct drm_i915_relocation_entry __user*)(uintptr_t)
			     args->buffers_ptr,
			     sizeof(*validate_list) * args->buffer_count);
	if (ret != 0) {
		DRM_ERROR ("copy %d validate entries failed %d\n", args->buffer_count, ret);
		goto err;
	}

	/* Look up object handles and perform the relocations */
	for (i = 0; i < args->buffer_count; i++) {
		object_list[i] = drm_gem_object_lookup(dev, file_priv,
						       validate_list[i].buffer_handle);
		if (object_list[i] == NULL) {
			DRM_ERROR ("Invalid object handle %d at index %d\n",
				   validate_list[i].buffer_handle, i);
			ret = -EINVAL;
			goto err;
		}

		ret = i915_gem_reloc_and_validate_object(object_list[i], file_priv,
							 &validate_list[i]);
		if (ret) {
			DRM_ERROR ("reloc and validate failed %d\n", ret);
			goto err;
		}
	}

	exec_offset = validate_list[args->buffer_count - 1].buffer_offset;

	/* make sure all previous memory operations have passed */

	/* Exec the batchbuffer */
	ret = i915_dispatch_gem_execbuffer (dev, args, exec_offset);
	if (ret)
	{
		DRM_ERROR ("dispatch failed %d\n", ret);
		goto err;
	}

	/* Copy the new buffer offsets back to the user's validate list. */
	ret = copy_to_user((struct drm_i915_relocation_entry __user*)(uintptr_t)
			   args->buffers_ptr,
			   validate_list,
			   sizeof(*validate_list) * args->buffer_count);
	if (ret)
		DRM_ERROR ("failed to copy %d validate entries back to user (%d)\n",
			   args->buffer_count, ret);

	/* Clean up and return */
	ret = i915_gem_sync_and_evict(dev);
	if (ret)
		DRM_ERROR ("failed to sync/evict buffers %d\n", ret);
err:
	if (object_list != NULL) {
		for (i = 0; i < args->buffer_count; i++)
			drm_gem_object_unreference(object_list[i]);
	}
	drm_free(object_list, sizeof(*object_list) * args->buffer_count,
		 DRM_MEM_DRIVER);
	drm_free(validate_list, sizeof(*validate_list) * args->buffer_count,
		 DRM_MEM_DRIVER);

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
