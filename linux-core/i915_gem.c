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

#define WATCH_BUF	0
#define WATCH_EXEC	0
#define WATCH_LRU	0
#define WATCH_RELOC	0

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


	for (i = 0; i < page_count; i++)
		if (obj_priv->page_list[i] != NULL)
			page_cache_release(obj_priv->page_list[i]);

	drm_free(obj_priv->page_list,
		 page_count * sizeof(struct page *),
		 DRM_MEM_DRIVER);
	obj_priv->page_list = NULL;
}

static void
i915_gem_flush(struct drm_device *dev,
	       uint32_t invalidate_domains,
	       uint32_t flush_domains)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	uint32_t cmd;
	RING_LOCALS;

#if WATCH_EXEC
	DRM_INFO("%s: invalidate %08x flush %08x\n", __func__,
		  invalidate_domains, flush_domains);
#endif

	if (flush_domains & DRM_GEM_DOMAIN_CPU)
		drm_agp_chipset_flush(dev);

	if ((invalidate_domains|flush_domains) & ~DRM_GEM_DOMAIN_CPU) {
		/*
		 * read/write caches:
		 *
		 * DRM_GEM_DOMAIN_I915_RENDER is always invalidated, but is
		 * only flushed if MI_NO_WRITE_FLUSH is unset.  On 965, it is
		 * also flushed at 2d versus 3d pipeline switches.
		 *
		 * read-only caches:
		 *
		 * DRM_GEM_DOMAIN_I915_SAMPLER is flushed on pre-965 if
		 * MI_READ_FLUSH is set, and is always flushed on 965.
		 *
		 * DRM_GEM_DOMAIN_I915_COMMAND may not exist?
		 *
		 * DRM_GEM_DOMAIN_I915_INSTRUCTION, which exists on 965, is
		 * invalidated when MI_EXE_FLUSH is set.
		 *
		 * DRM_GEM_DOMAIN_I915_VERTEX, which exists on 965, is
		 * invalidated with every MI_FLUSH.
		 *
		 * TLBs:
		 *
		 * On 965, TLBs associated with DRM_GEM_DOMAIN_I915_COMMAND
		 * and DRM_GEM_DOMAIN_CPU in are invalidated at PTE write and
		 * DRM_GEM_DOMAIN_I915_RENDER and DRM_GEM_DOMAIN_I915_SAMPLER
		 * are flushed at any MI_FLUSH.
		 */

		cmd = CMD_MI_FLUSH | MI_NO_WRITE_FLUSH;
		if ((invalidate_domains|flush_domains) &
		    DRM_GEM_DOMAIN_I915_RENDER)
			cmd &= ~MI_NO_WRITE_FLUSH;
		if (!IS_I965G(dev)) {
			/*
			 * On the 965, the sampler cache always gets flushed
			 * and this bit is reserved.
			 */
			if (invalidate_domains & DRM_GEM_DOMAIN_I915_SAMPLER)
				cmd |= MI_READ_FLUSH;
		}
		if (invalidate_domains & DRM_GEM_DOMAIN_I915_INSTRUCTION)
			cmd |= MI_EXE_FLUSH;

		BEGIN_LP_RING(2);
		OUT_RING(cmd);
		OUT_RING(0); /* noop */
		ADVANCE_LP_RING();
	}
}

/**
 * Ensures that all rendering to the object has completed and the object is
 * safe to unbind from the GTT or access from the CPU.
 */
static int
i915_gem_object_wait_rendering(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	int ret;

	/* If there are writes queued to the buffer, flush and
	 * create a new cookie to wait for.
	 */
	if (obj->write_domain & ~(DRM_GEM_DOMAIN_CPU)) {
#if WATCH_BUF
		DRM_INFO("%s: flushing object %p from write domain %08x\n",
			  __func__, obj, obj->write_domain);
#endif
		i915_gem_flush(dev, 0, obj->write_domain);
		obj->write_domain = 0;
		if (obj_priv->last_rendering_cookie == 0)
			drm_gem_object_reference(obj);
		obj_priv->last_rendering_cookie = i915_emit_irq(dev);
	}
	/* If there is rendering queued on the buffer being evicted, wait for
	 * it.
	 */
	if (obj_priv->last_rendering_cookie != 0) {
#if WATCH_BUF
		DRM_INFO("%s: object %p wait for cookie %08x\n",
			  __func__, obj, obj_priv->last_rendering_cookie);
#endif
		ret = i915_wait_irq(dev, obj_priv->last_rendering_cookie);
		if (ret != 0)
			return ret;
		/* Clear it now that we know it's passed. */
		obj_priv->last_rendering_cookie = 0;

		/*
		 * The cookie held a reference to the object,
		 * release that now
		 */
		drm_gem_object_unreference(obj);
	}

	return 0;
}

/**
 * Unbinds an object from the GTT aperture.
 */
static void
i915_gem_object_unbind(struct drm_gem_object *obj)
{
	struct drm_i915_gem_object *obj_priv = obj->driver_private;

#if WATCH_BUF
	DRM_INFO("%s:%d %p\n", __func__, __LINE__, obj);
	DRM_INFO("gtt_space %p\n", obj_priv->gtt_space);
#endif
	if (obj_priv->gtt_space == NULL)
		return;

	i915_gem_object_wait_rendering(obj);

	if (obj_priv->agp_mem != NULL) {
		drm_unbind_agp(obj_priv->agp_mem);
		drm_free_agp(obj_priv->agp_mem, obj->size / PAGE_SIZE);
		obj_priv->agp_mem = NULL;
	}

	i915_gem_object_free_page_list(obj);

	drm_memrange_put_block(obj_priv->gtt_space);
	obj_priv->gtt_space = NULL;
	if (!list_empty(&obj_priv->gtt_lru_entry))
		list_del_init(&obj_priv->gtt_lru_entry);
}

#if WATCH_BUF | WATCH_EXEC
static void
i915_gem_dump_page(struct page *page, uint32_t start, uint32_t end,
		   uint32_t bias, uint32_t mark)
{
	uint32_t *mem = kmap_atomic(page, KM_USER0);
	int i;
	for (i = start; i < end; i += 4)
		DRM_INFO("%08x: %08x%s\n",
			  (int) (bias + i), mem[i / 4],
			  (bias + i == mark) ? " ********" : "");
	kunmap_atomic(mem, KM_USER0);
	/* give syslog time to catch up */
	msleep(1);
}

static void
i915_gem_dump_object(struct drm_gem_object *obj, int len,
		     const char *where, uint32_t mark)
{
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	int page;

	DRM_INFO("%s: object at offset %08x\n", where, obj_priv->gtt_offset);
	for (page = 0; page < (len + PAGE_SIZE-1) / PAGE_SIZE; page++) {
		int page_len, chunk, chunk_len;

		page_len = len - page * PAGE_SIZE;
		if (page_len > PAGE_SIZE)
			page_len = PAGE_SIZE;

		for (chunk = 0; chunk < page_len; chunk += 128) {
			chunk_len = page_len - chunk;
			if (chunk_len > 128)
				chunk_len = 128;
			i915_gem_dump_page(obj_priv->page_list[page],
					   chunk, chunk + chunk_len,
					   obj_priv->gtt_offset +
					   page * PAGE_SIZE,
					   mark);
		}
	}
}
#endif

#if WATCH_LRU
static void
i915_dump_lru(struct drm_device *dev, const char *where)
{
	drm_i915_private_t		*dev_priv = dev->dev_private;
	struct drm_i915_gem_object	*obj_priv;

	DRM_INFO("GTT LRU %s {\n", where);
	list_for_each_entry(obj_priv, &dev_priv->mm.gtt_lru, gtt_lru_entry) {
		DRM_INFO("    %p: %08x\n", obj_priv,
			 obj_priv->last_rendering_cookie);
	}
	DRM_INFO("}\n");
}
#endif

static int
i915_gem_evict_something(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_gem_object *obj;
	struct drm_i915_gem_object *obj_priv;
	int ret;

	/* Find the LRU buffer. */
	BUG_ON(list_empty(&dev_priv->mm.gtt_lru));
	obj_priv = list_first_entry(&dev_priv->mm.gtt_lru,
				    struct drm_i915_gem_object,
				    gtt_lru_entry);
	obj = obj_priv->obj;
#if WATCH_LRU
	DRM_INFO("%s: evicting %p\n", __func__, obj);
#endif

	/* Only unpinned buffers should be on this list. */
	BUG_ON(obj_priv->pin_count != 0);

	/* Do this separately from the wait_rendering in
	 * i915_gem_object_unbind() because we want to catch interrupts and
	 * return.
	 */
	ret = i915_gem_object_wait_rendering(obj);
	if (ret != 0)
		return ret;

	/* Wait on the rendering and unbind the buffer. */
	i915_gem_object_unbind(obj);
#if WATCH_LRU
	DRM_INFO("%s: evicted %p\n", __func__, obj);
#endif

	return 0;
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
	int page_count, i, ret;

	if (alignment == 0)
		alignment = PAGE_SIZE;
	if (alignment & (PAGE_SIZE - 1)) {
		DRM_ERROR("Invalid object alignment requested %u\n", alignment);
		return -EINVAL;
	}

 search_free:
	free_space = drm_memrange_search_free(&dev_priv->mm.gtt_space,
					      obj->size,
					      alignment, 0);
	if (free_space != NULL) {
		obj_priv->gtt_space =
			drm_memrange_get_block(free_space, obj->size,
					       alignment);
		if (obj_priv->gtt_space != NULL) {
			obj_priv->gtt_space->private = obj;
			obj_priv->gtt_offset = obj_priv->gtt_space->start;
		}
	}
	if (obj_priv->gtt_space == NULL) {
		/* If the gtt is empty and we're still having trouble
		 * fitting our object in, we're out of memory.
		 */
#if WATCH_LRU
		DRM_INFO("%s: GTT full, evicting something\n", __func__);
#endif
		if (list_empty(&dev_priv->mm.gtt_lru)) {
			DRM_ERROR("GTT full, but LRU list empty\n");
			return -ENOMEM;
		}

		ret = i915_gem_evict_something(dev);
		if (ret != 0)
			return ret;
		goto search_free;
	}

#if WATCH_BUF
	DRM_INFO("Binding object of size %d at 0x%08x\n",
		 obj->size, obj_priv->gtt_offset);
#endif

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
		unlock_page(obj_priv->page_list[i]);
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

	/* When we have just bound an object, we have no valid read
	 * caches on it, regardless of where it was before.  We also need
	 * an MI_FLUSH to occur so that the render and sampler TLBs
	 * get flushed and pick up our binding change above.
	 */
	obj->read_domains = 0;

	return 0;
}

static void
i915_gem_clflush_object(struct drm_gem_object *obj)
{
	struct drm_i915_gem_object	*obj_priv = obj->driver_private;

	/* If we don't have a page list set up, then we're not pinned
	 * to GPU, and we can ignore the cache flush because it'll happen
	 * again at bind time.
	 */
	if (obj_priv->page_list == NULL)
		return;

	drm_ttm_cache_flush(obj_priv->page_list, obj->size / PAGE_SIZE);
}

/*
 * Set the next domain for the specified object. This
 * may not actually perform the necessary flushing/invaliding though,
 * as that may want to be batched with other set_domain operations
 */
static void
i915_gem_object_set_domain(struct drm_gem_object *obj,
			    uint32_t read_domains,
			    uint32_t write_domain)
{
	struct drm_device		*dev = obj->dev;
	uint32_t			invalidate_domains = 0;
	uint32_t			flush_domains = 0;

#if WATCH_BUF
	DRM_INFO("%s: object %p read %08x write %08x\n",
		 __func__, obj, read_domains, write_domain);
#endif
	/*
	 * Flush the current write domain if
	 * the new read domains don't match. Invalidate
	 * any read domains which differ from the old
	 * write domain
	 */
	if (obj->write_domain && obj->write_domain != read_domains) {
		flush_domains |= obj->write_domain;
		invalidate_domains |= read_domains & ~obj->write_domain;
	}
	/*
	 * Invalidate any read caches which may have
	 * stale data. That is, any new read domains.
	 */
	invalidate_domains |= read_domains & ~obj->read_domains;
	if ((flush_domains | invalidate_domains) & DRM_GEM_DOMAIN_CPU) {
#if WATCH_BUF
		DRM_INFO("%s: CPU domain flush %08x invalidate %08x\n",
			 __func__, flush_domains, invalidate_domains);
#endif
		/*
		 * If we're invaliding the CPU cache and flushing a GPU cache,
		 * then pause for rendering so that the GPU caches will be
		 * flushed before the cpu cache is invalidated
		 */
		if ((invalidate_domains & DRM_GEM_DOMAIN_CPU) &&
		    (flush_domains & ~DRM_GEM_DOMAIN_CPU))
			i915_gem_object_wait_rendering(obj);
		i915_gem_clflush_object(obj);
	}

	obj->write_domain = write_domain;
	obj->read_domains = read_domains;
	dev->invalidate_domains |= invalidate_domains;
	dev->flush_domains |= flush_domains;
}

/**
 * Once all of the objects have been set in the proper domain,
 * perform the necessary flush and invalidate operations
 */

static void
i915_gem_dev_set_domain(struct drm_device *dev)
{
	/*
	 * Now that all the buffers are synced to the proper domains,
	 * flush and invalidate the collected domains
	 */
	if (dev->invalidate_domains | dev->flush_domains) {
#if WATCH_EXEC
		DRM_INFO("%s: invalidate_domains %08x flush_domains %08x\n",
			  __func__,
			 dev->invalidate_domains,
			 dev->flush_domains);
#endif
		i915_gem_flush(dev,
			       dev->invalidate_domains,
			       dev->flush_domains);
		dev->invalidate_domains = 0;
		dev->flush_domains = 0;
	}
}

static int
i915_gem_reloc_and_validate_object(struct drm_gem_object *obj,
				   struct drm_file *file_priv,
				   struct drm_i915_gem_validate_entry *entry)
{
	struct drm_device *dev = obj->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_relocation_entry reloc;
	struct drm_i915_gem_relocation_entry __user *relocs;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	int i;
	uint32_t last_reloc_offset = -1;
	void *reloc_page = NULL;

	/* Choose the GTT offset for our buffer and put it there. */
	if (obj_priv->gtt_space == NULL) {
		i915_gem_object_bind_to_gtt(obj, (unsigned) entry->alignment);
		if (obj_priv->gtt_space == NULL)
			return -ENOMEM;
	}

	entry->buffer_offset = obj_priv->gtt_offset;

	if (obj_priv->pin_count == 0) {
		/* Move our buffer to the head of the LRU. */
		if (list_empty(&obj_priv->gtt_lru_entry))
			list_add_tail(&obj_priv->gtt_lru_entry,
				      &dev_priv->mm.gtt_lru);
		else
			list_move_tail(&obj_priv->gtt_lru_entry,
				       &dev_priv->mm.gtt_lru);
#if WATCH_LRU && 0
		i915_dump_lru(dev, __func__);
#endif
	}

	relocs = (struct drm_i915_gem_relocation_entry __user *)
		 (uintptr_t) entry->relocs_ptr;
	/* Apply the relocations, using the GTT aperture to avoid cache
	 * flushing requirements.
	 */
	for (i = 0; i < entry->relocation_count; i++) {
		struct drm_gem_object *target_obj;
		struct drm_i915_gem_object *target_obj_priv;
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
			drm_gem_object_unreference(target_obj);
			return -EINVAL;
		}

		if (reloc.offset > obj->size - 4) {
			DRM_ERROR("Relocation beyond object bounds: "
				  "obj %p target %d offset %d size %d.\n",
				  obj, reloc.target_handle,
				  (int) reloc.offset, (int) obj->size);
			drm_gem_object_unreference(target_obj);
			return -EINVAL;
		}
		if (reloc.offset & 3) {
			DRM_ERROR("Relocation not 4-byte aligned: "
				  "obj %p target %d offset %d.\n",
				  obj, reloc.target_handle,
				  (int) reloc.offset);
			drm_gem_object_unreference(target_obj);
			return -EINVAL;
		}

		if (reloc.write_domain && target_obj->pending_write_domain &&
		    reloc.write_domain != target_obj->pending_write_domain) {
			DRM_ERROR("Write domain conflict: "
				  "obj %p target %d offset %d "
				  "new %08x old %08x\n",
				  obj, reloc.target_handle,
				  (int) reloc.offset,
				  reloc.write_domain,
				  target_obj->pending_write_domain);
			drm_gem_object_unreference(target_obj);
			return -EINVAL;
		}

#if WATCH_RELOC
		DRM_INFO("%s: obj %p offset %08x target %d "
			 "read %08x write %08x gtt %08x "
			 "presumed %08x delta %08x\n",
			 __func__,
			 obj,
			 (int) reloc.offset,
			 (int) reloc.target_handle,
			 (int) reloc.read_domains,
			 (int) reloc.write_domain,
			 (int) target_obj_priv->gtt_offset,
			 (int) reloc.presumed_offset,
			 reloc.delta);
#endif

		target_obj->pending_read_domains |= reloc.read_domains;
		target_obj->pending_write_domain |= reloc.write_domain;

		/* If the relocation already has the right value in it, no
		 * more work needs to be done.
		 */
		if (target_obj_priv->gtt_offset == reloc.presumed_offset) {
			drm_gem_object_unreference(target_obj);
			continue;
		}

		/* Now that we're going to actually write some data in,
		 * make sure that any rendering using this buffer's contents
		 * is completed.
		 */
		i915_gem_object_wait_rendering(obj);

		/* As we're writing through the gtt, flush
		 * any CPU writes before we write the relocations
		 */
		if (obj->write_domain & DRM_GEM_DOMAIN_CPU) {
			i915_gem_clflush_object(obj);
			drm_agp_chipset_flush(dev);
			obj->write_domain = 0;
		}

		/* Map the page containing the relocation we're going to
		 * perform.
		 */
		reloc_offset = obj_priv->gtt_offset + reloc.offset;
		if (reloc_page == NULL ||
		    (last_reloc_offset & ~(PAGE_SIZE - 1)) !=
		    (reloc_offset & ~(PAGE_SIZE - 1))) {
			if (reloc_page != NULL)
				iounmap(reloc_page);

			reloc_page = ioremap(dev->agp->base +
					     (reloc_offset & ~(PAGE_SIZE - 1)),
					     PAGE_SIZE);
			last_reloc_offset = reloc_offset;
			if (reloc_page == NULL) {
				drm_gem_object_unreference(target_obj);
				return -ENOMEM;
			}
		}

		reloc_entry = (uint32_t *)((char *)reloc_page +
					   (reloc_offset & (PAGE_SIZE - 1)));
		reloc_val = target_obj_priv->gtt_offset + reloc.delta;

#if WATCH_BUF
		DRM_INFO("Applied relocation: %p@0x%08x %08x -> %08x\n",
			  obj, (unsigned int) reloc.offset,
			  readl(reloc_entry), reloc_val);
#endif
		writel(reloc_val, reloc_entry);

		drm_gem_object_unreference(target_obj);
	}

	if (reloc_page != NULL)
		iounmap(reloc_page);

#if WATCH_BUF
	i915_gem_dump_object(obj, 128, __func__, ~0);
#endif
	return 0;
}

static int
i915_dispatch_gem_execbuffer(struct drm_device *dev,
			      struct drm_i915_gem_execbuffer *exec,
			      uint64_t exec_offset)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_clip_rect __user *boxes = (struct drm_clip_rect __user *)
					     (uintptr_t) exec->cliprects_ptr;
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
				OUT_RING(MI_BATCH_BUFFER_START |
					 (2 << 6) |
					 MI_BATCH_NON_SECURE_I965);
				OUT_RING(exec_start);
			} else {
				OUT_RING(MI_BATCH_BUFFER_START |
					 (2 << 6));
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
	struct drm_gem_object *batch_obj;
	int ret, i;
	uint64_t exec_offset;
	uint32_t cookie;

	LOCK_TEST_WITH_RETURN(dev, file_priv);

#if WATCH_EXEC
	DRM_INFO("buffers_ptr %d buffer_count %d len %08x\n",
		  (int) args->buffers_ptr, args->buffer_count, args->batch_len);
#endif
	i915_kernel_lost_context(dev);

	/* Copy in the validate list from userland */
	validate_list = drm_calloc(sizeof(*validate_list), args->buffer_count,
				   DRM_MEM_DRIVER);
	object_list = drm_calloc(sizeof(*object_list), args->buffer_count,
				 DRM_MEM_DRIVER);
	if (validate_list == NULL || object_list == NULL) {
		DRM_ERROR("Failed to allocate validate or object list "
			  "for %d buffers\n",
			  args->buffer_count);
		ret = -ENOMEM;
		goto err;
	}
	ret = copy_from_user(validate_list,
			     (struct drm_i915_relocation_entry __user *)
			     (uintptr_t) args->buffers_ptr,
			     sizeof(*validate_list) * args->buffer_count);
	if (ret != 0) {
		DRM_ERROR("copy %d validate entries failed %d\n",
			  args->buffer_count, ret);
		goto err;
	}

	/* Look up object handles and perform the relocations */
	for (i = 0; i < args->buffer_count; i++) {
		object_list[i] = drm_gem_object_lookup(dev, file_priv,
						       validate_list[i].
						       buffer_handle);
		if (object_list[i] == NULL) {
			DRM_ERROR("Invalid object handle %d at index %d\n",
				   validate_list[i].buffer_handle, i);
			ret = -EINVAL;
			goto err;
		}

		ret = i915_gem_reloc_and_validate_object(object_list[i],
							 file_priv,
							 &validate_list[i]);
		if (ret) {
			DRM_ERROR("reloc and validate failed %d\n", ret);
			goto err;
		}
	}

	/* Set the pending read domains for the batch buffer to COMMAND */
	batch_obj = object_list[args->buffer_count-1];
	batch_obj->pending_read_domains = DRM_GEM_DOMAIN_I915_COMMAND;
	batch_obj->pending_write_domain = 0;

	for (i = 0; i < args->buffer_count; i++) {
		struct drm_gem_object *obj = object_list[i];
		struct drm_i915_gem_object *obj_priv = obj->driver_private;

		if (obj_priv->gtt_space == NULL) {
			/* We evicted the buffer in the process of validating
			 * our set of buffers in.  We could try to recover by
			 * kicking them everything out and trying again from
			 * the start.
			 */
			ret = -ENOMEM;
			goto err;
		}

		/* make sure all previous memory operations have passed */
		i915_gem_object_set_domain(obj,
					    obj->pending_read_domains,
					    obj->pending_write_domain);
		obj->pending_read_domains = 0;
		obj->pending_write_domain = 0;
	}

	/* Flush/invalidate caches and chipset buffer */
	i915_gem_dev_set_domain(dev);

	exec_offset = validate_list[args->buffer_count - 1].buffer_offset;

#if WATCH_EXEC
	i915_gem_dump_object(object_list[args->buffer_count - 1],
			      args->batch_len,
			      __func__,
			      ~0);
#endif

	/* Exec the batchbuffer */
	ret = i915_dispatch_gem_execbuffer(dev, args, exec_offset);
	if (ret) {
		DRM_ERROR("dispatch failed %d\n", ret);
		goto err;
	}

	/*
	 * Get a cookie representing the execution of the current buffer,
	 * which we can wait on.  We would like to mitigate these interrupts,
	 * likely by only creating cookies occasionally (so that we have
	 * *some* interrupts representing completion of buffers that we can
	 * wait on when trying to clear up gtt space).
	 */
	cookie = i915_emit_irq(dev);
	for (i = 0; i < args->buffer_count; i++) {
		struct drm_gem_object *obj = object_list[i];
		struct drm_i915_gem_object *obj_priv = obj->driver_private;

		/*
		 * Have the cookie hold a reference to this object
		 * which is freed when the object is waited for
		 */
		if (obj_priv->last_rendering_cookie == 0)
			drm_gem_object_reference(obj);
		obj_priv->last_rendering_cookie = cookie;
	}

	/* Copy the new buffer offsets back to the user's validate list. */
	ret = copy_to_user((struct drm_i915_relocation_entry __user *)
			   (uintptr_t) args->buffers_ptr,
			   validate_list,
			   sizeof(*validate_list) * args->buffer_count);
	if (ret)
		DRM_ERROR("failed to copy %d validate entries "
			  "back to user (%d)\n",
			   args->buffer_count, ret);
err:
	if (object_list != NULL) {
		for (i = 0; i < args->buffer_count; i++)
			drm_gem_object_unreference(object_list[i]);
	}

#if 1
	/* XXX kludge for now as we don't clean the exec ring yet */
	if (object_list != NULL) {
		for (i = 0; i < args->buffer_count; i++)
			i915_gem_object_wait_rendering(object_list[i]);
	}
#endif

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
	if (obj_priv->gtt_space == NULL) {
		ret = i915_gem_object_bind_to_gtt(obj,
						  (unsigned) args->alignment);
		if (ret != 0) {
			DRM_ERROR("Failure to bind in "
				  "i915_gem_pin_ioctl(): %d\n",
				  ret);
			drm_gem_object_unreference(obj);
			return ret;
		}
	}

	obj_priv->pin_count++;
	args->offset = obj_priv->gtt_offset;
	drm_gem_object_unreference(obj);

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
	drm_gem_object_unreference(obj);
	return 0;
}

int i915_gem_init_object(struct drm_gem_object *obj)
{
	struct drm_i915_gem_object *obj_priv;

	obj_priv = drm_calloc(1, sizeof(*obj_priv), DRM_MEM_DRIVER);
	if (obj_priv == NULL)
		return -ENOMEM;

	obj->driver_private = obj_priv;
	obj_priv->obj = obj;
	INIT_LIST_HEAD(&obj_priv->gtt_lru_entry);
	return 0;
}

void i915_gem_free_object(struct drm_gem_object *obj)
{
	i915_gem_object_unbind(obj);

	drm_free(obj->driver_private, 1, DRM_MEM_DRIVER);
}

int
i915_gem_set_domain_ioctl(struct drm_gem_object *obj,
			   uint32_t read_domains,
			   uint32_t write_domain)
{
	i915_gem_object_set_domain(obj, read_domains, write_domain);
	i915_gem_dev_set_domain(obj->dev);
	return 0;
}
