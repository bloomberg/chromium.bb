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

static void
i915_gem_object_set_domain(struct drm_gem_object *obj,
			    uint32_t read_domains,
			    uint32_t write_domain);

int
i915_gem_init_ioctl(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_init *args = data;

	mutex_lock(&dev->struct_mutex);

	if (args->gtt_start >= args->gtt_end ||
	    (args->gtt_start & (PAGE_SIZE - 1)) != 0 ||
	    (args->gtt_end & (PAGE_SIZE - 1)) != 0) {
		mutex_unlock(&dev->struct_mutex);
		return -EINVAL;
	}

	drm_memrange_init(&dev_priv->mm.gtt_space, args->gtt_start,
	    args->gtt_end - args->gtt_start);

	mutex_unlock(&dev->struct_mutex);

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
i915_gem_object_move_to_active(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;

	/* Add a reference if we're newly entering the active list. */
	if (!obj_priv->active) {
		drm_gem_object_reference(obj);
		obj_priv->active = 1;
	}
	/* Move from whatever list we were on to the tail of execution. */
	list_move_tail(&obj_priv->list,
		       &dev_priv->mm.active_list);
}

static void
i915_gem_object_move_to_inactive(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;

	if (obj_priv->pin_count != 0)
		list_del_init(&obj_priv->list);
	else
		list_move_tail(&obj_priv->list, &dev_priv->mm.inactive_list);

	if (obj_priv->active) {
		obj_priv->active = 0;
		drm_gem_object_unreference(obj);
	}
}

/**
 * Creates a new sequence number, emitting a write of it to the status page
 * plus an interrupt, which will trigger i915_user_interrupt_handler.
 *
 * Must be called with struct_lock held.
 *
 * Returned sequence numbers are nonzero on success.
 */
static uint32_t
i915_add_request(struct drm_device *dev, uint32_t flush_domains)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_request *request;
	uint32_t seqno;
	RING_LOCALS;

	request = drm_calloc(1, sizeof(*request), DRM_MEM_DRIVER);
	if (request == NULL)
		return 0;

	/* Grab the seqno we're going to make this request be, and bump the
	 * next (skipping 0 so it can be the reserved no-seqno value).
	 */
	seqno = dev_priv->mm.next_gem_seqno;
	dev_priv->mm.next_gem_seqno++;
	if (dev_priv->mm.next_gem_seqno == 0)
		dev_priv->mm.next_gem_seqno++;

	BEGIN_LP_RING(4);
	OUT_RING(CMD_STORE_DWORD_IDX);
	OUT_RING(I915_GEM_HWS_INDEX << STORE_DWORD_INDEX_SHIFT);
	OUT_RING(seqno);

	OUT_RING(GFX_OP_USER_INTERRUPT);
	ADVANCE_LP_RING();

	DRM_DEBUG("%d\n", seqno);

	request->seqno = seqno;
	request->emitted_jiffies = jiffies;
	request->flush_domains = flush_domains;
	list_add_tail(&request->list, &dev_priv->mm.request_list);

	return seqno;
}

/**
 * Command execution barrier
 *
 * Ensures that all commands in the ring are finished
 * before signalling the CPU
 */

uint32_t
i915_retire_commands(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	uint32_t cmd = CMD_MI_FLUSH | MI_NO_WRITE_FLUSH;
	uint32_t flush_domains = 0;
	RING_LOCALS;

	/* The sampler always gets flushed on i965 (sigh) */
	if (IS_I965G(dev))
		flush_domains |= DRM_GEM_DOMAIN_I915_SAMPLER;
	BEGIN_LP_RING(2);
	OUT_RING(cmd);
	OUT_RING(0); /* noop */
	ADVANCE_LP_RING();
	return flush_domains;
}

/**
 * Moves buffers associated only with the given active seqno from the active
 * to inactive list, potentially freeing them.
 */
static void
i915_gem_retire_request(struct drm_device *dev,
			struct drm_i915_gem_request *request)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	if (request->flush_domains != 0) {
		struct drm_i915_gem_object *obj_priv, *next;

		/* First clear any buffers that were only waiting for a flush
		 * matching the one just retired.
		 */

		list_for_each_entry_safe(obj_priv, next,
					 &dev_priv->mm.flushing_list, list) {
			struct drm_gem_object *obj = obj_priv->obj;

			if (obj->write_domain & request->flush_domains) {
				obj->write_domain = 0;
				i915_gem_object_move_to_inactive(obj);
			}
		}

	}

	/* Move any buffers on the active list that are no longer referenced
	 * by the ringbuffer to the flushing/inactive lists as appropriate.
	 */
	while (!list_empty(&dev_priv->mm.active_list)) {
		struct drm_gem_object *obj;
		struct drm_i915_gem_object *obj_priv;

		obj_priv = list_first_entry(&dev_priv->mm.active_list,
					    struct drm_i915_gem_object,
					    list);
		obj = obj_priv->obj;

		/* If the seqno being retired doesn't match the oldest in the
		 * list, then the oldest in the list must still be newer than
		 * this seqno.
		 */
		if (obj_priv->last_rendering_seqno != request->seqno)
			return;
#if WATCH_LRU
		DRM_INFO("%s: retire %d moves to inactive list %p\n",
			 __func__, request->seqno, obj);
#endif

		if (obj->write_domain != 0) {
			list_move_tail(&obj_priv->list,
				       &dev_priv->mm.flushing_list);
		} else {
			i915_gem_object_move_to_inactive(obj);
		}
	}
}

/**
 * Returns true if seq1 is later than seq2.
 */
static int
i915_seqno_passed(uint32_t seq1, uint32_t seq2)
{
	return (int32_t)(seq1 - seq2) >= 0;
}

static uint32_t
i915_get_gem_seqno(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	return READ_HWSP(dev_priv, I915_GEM_HWS_INDEX);
}

/**
 * This function clears the request list as sequence numbers are passed.
 */
void
i915_gem_retire_requests(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	uint32_t seqno;

	seqno = i915_get_gem_seqno(dev);

	while (!list_empty(&dev_priv->mm.request_list)) {
		struct drm_i915_gem_request *request;
		uint32_t retiring_seqno;

		request = list_first_entry(&dev_priv->mm.request_list,
					   struct drm_i915_gem_request,
					   list);
		retiring_seqno = request->seqno;

		if (i915_seqno_passed(seqno, retiring_seqno)) {
			i915_gem_retire_request(dev, request);

			list_del(&request->list);
			drm_free(request, sizeof(*request), DRM_MEM_DRIVER);
		} else
		    break;
	}
}

/**
 * Waits for a sequence number to be signaled, and cleans up the
 * request and object lists appropriately for that event.
 */
int
i915_wait_request(struct drm_device *dev, uint32_t seqno)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret = 0;

	BUG_ON(seqno == 0);

	if (!i915_seqno_passed(i915_get_gem_seqno(dev), seqno)) {
		i915_user_irq_on(dev_priv);
		ret = wait_event_interruptible(dev_priv->irq_queue,
					       i915_seqno_passed(i915_get_gem_seqno(dev),
								 seqno));
		i915_user_irq_off(dev_priv);
	}

	/* Directly dispatch request retiring.  While we have the work queue
	 * to handle this, the waiter on a request often wants an associated
	 * buffer to have made it to the inactive list, and we would need
	 * a separate wait queue to handle that.
	 */
	if (ret == 0)
		i915_gem_retire_requests(dev);

	return ret;
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

#if WATCH_EXEC
		DRM_INFO("%s: queue flush %08x to ring\n", __func__, cmd);
#endif
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
	 * create a new seqno to wait for.
	 */
	if (obj->write_domain & ~(DRM_GEM_DOMAIN_CPU)) {
		uint32_t write_domain = obj->write_domain;
#if WATCH_BUF
		DRM_INFO("%s: flushing object %p from write domain %08x\n",
			  __func__, obj, write_domain);
#endif
		i915_gem_flush(dev, 0, write_domain);
		obj->write_domain = 0;

		i915_gem_object_move_to_active(obj);
		obj_priv->last_rendering_seqno = i915_add_request(dev,
								  write_domain);
		BUG_ON(obj_priv->last_rendering_seqno == 0);
#if WATCH_LRU
		DRM_INFO("%s: flush moves to exec list %p\n", __func__, obj);
#endif
	}
	/* If there is rendering queued on the buffer being evicted, wait for
	 * it.
	 */
	if (obj_priv->active) {
#if WATCH_BUF
		DRM_INFO("%s: object %p wait for seqno %08x\n",
			  __func__, obj, obj_priv->last_rendering_seqno);
#endif
		ret = i915_wait_request(dev, obj_priv->last_rendering_seqno);
		if (ret != 0)
			return ret;
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

	/* Move the object to the CPU domain to ensure that
	 * any possible CPU writes while it's not in the GTT
	 * are flushed when we go to remap it. This will
	 * also ensure that all pending GPU writes are finished
	 * before we unbind.
	 */
	i915_gem_object_set_domain (obj, DRM_GEM_DOMAIN_CPU,
				    DRM_GEM_DOMAIN_CPU);

	if (obj_priv->agp_mem != NULL) {
		drm_unbind_agp(obj_priv->agp_mem);
		drm_free_agp(obj_priv->agp_mem, obj->size / PAGE_SIZE);
		obj_priv->agp_mem = NULL;
	}

	i915_gem_object_free_page_list(obj);

	drm_memrange_put_block(obj_priv->gtt_space);
	obj_priv->gtt_space = NULL;

	/* Remove ourselves from the LRU list if present. */
	if (!list_empty(&obj_priv->list)) {
		list_del_init(&obj_priv->list);
		if (obj_priv->active) {
			DRM_ERROR("Failed to wait on buffer when unbinding, "
				  "continued anyway.\n");
			obj_priv->active = 0;
			drm_gem_object_unreference(obj);
		}
	}
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

	DRM_INFO("active list %s {\n", where);
	list_for_each_entry(obj_priv, &dev_priv->mm.active_list,
			    list)
	{
		DRM_INFO("    %p: %08x\n", obj_priv,
			 obj_priv->last_rendering_seqno);
	}
	DRM_INFO("}\n");
	DRM_INFO("flushing list %s {\n", where);
	list_for_each_entry(obj_priv, &dev_priv->mm.flushing_list,
			    list)
	{
		DRM_INFO("    %p: %08x\n", obj_priv,
			 obj_priv->last_rendering_seqno);
	}
	DRM_INFO("}\n");
	DRM_INFO("inactive %s {\n", where);
	list_for_each_entry(obj_priv, &dev_priv->mm.inactive_list, list) {
		DRM_INFO("    %p: %08x\n", obj_priv,
			 obj_priv->last_rendering_seqno);
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

	for (;;) {
		/* If there's an inactive buffer available now, grab it
		 * and be done.
		 */
		if (!list_empty(&dev_priv->mm.inactive_list)) {
			obj_priv = list_first_entry(&dev_priv->mm.inactive_list,
						    struct drm_i915_gem_object,
						    list);
			obj = obj_priv->obj;
			BUG_ON(obj_priv->pin_count != 0);
			break;
		}

		/* If we didn't get anything, but the ring is still processing
		 * things, wait for one of those things to finish and hopefully
		 * leave us a buffer to evict.
		 */
		if (!list_empty(&dev_priv->mm.request_list)) {
			struct drm_i915_gem_request *request;
			int ret;

			request = list_first_entry(&dev_priv->mm.request_list,
						   struct drm_i915_gem_request,
						   list);

			ret = i915_wait_request(dev, request->seqno);
			if (ret != 0)
				return ret;

			continue;
		}

		/* If we didn't have anything on the request list but there
		 * are buffers awaiting a flush, emit one and try again.
		 * When we wait on it, those buffers waiting for that flush
		 * will get moved to inactive.
		 */
		if (!list_empty(&dev_priv->mm.flushing_list)) {
			obj_priv = list_first_entry(&dev_priv->mm.flushing_list,
						    struct drm_i915_gem_object,
						    list);
			obj = obj_priv->obj;

			i915_gem_flush(dev,
				       obj->write_domain,
				       obj->write_domain);
			i915_add_request(dev, obj->write_domain);

			obj = NULL;
			continue;
		}

		/* If we didn't do any of the above, there's nothing to be done
		 * and we just can't fit it in.
		 */
		return -ENOMEM;
	}

#if WATCH_LRU
	DRM_INFO("%s: evicting %p\n", __func__, obj);
#endif

	BUG_ON(obj_priv->active);

	/* Wait on the rendering and unbind the buffer. */
	i915_gem_object_unbind(obj);

	return 0;
}

static int
i915_gem_object_get_page_list(struct drm_gem_object *obj)
{
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	int page_count, i;
	if (obj_priv->page_list)
		return 0;

	/* Get the list of pages out of our struct file.  They'll be pinned
	 * at this point until we release them.
	 */
	page_count = obj->size / PAGE_SIZE;
	BUG_ON(obj_priv->page_list != NULL);
	obj_priv->page_list = drm_calloc(page_count, sizeof(struct page *),
					 DRM_MEM_DRIVER);
	if (obj_priv->page_list == NULL)
		return -ENOMEM;

	for (i = 0; i < page_count; i++) {
		obj_priv->page_list[i] =
		    find_or_create_page(obj->filp->f_mapping, i, GFP_HIGHUSER);

		if (obj_priv->page_list[i] == NULL) {
			i915_gem_object_free_page_list(obj);
			return -ENOMEM;
		}
		unlock_page(obj_priv->page_list[i]);
	}
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
	int page_count, ret;

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
		if (list_empty(&dev_priv->mm.inactive_list) &&
		    list_empty(&dev_priv->mm.active_list)) {
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
	ret = i915_gem_object_get_page_list(obj);
	if (ret) {
		drm_memrange_put_block(obj_priv->gtt_space);
		obj_priv->gtt_space = NULL;
		return ret;
	}

	page_count = obj->size / PAGE_SIZE;
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

	/* Assert that the object is not currently in any GPU domain. As it
	 * wasn't in the GTT, there shouldn't be any way it could have been in
	 * a GPU cache
	 */
	BUG_ON(obj->read_domains & ~DRM_GEM_DOMAIN_CPU);
	BUG_ON(obj->write_domain & ~DRM_GEM_DOMAIN_CPU);

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
 *
 * This is (we hope) the only really tricky part of gem. The goal
 * is fairly simple -- track which caches hold bits of the object
 * and make sure they remain coherent. A few concrete examples may
 * help to explain how it works. For shorthand, we use the notation
 * (read_domains, write_domain), e.g. (CPU, CPU) to indicate the
 * a pair of read and write domain masks.
 *
 * Case 1: the batch buffer
 *
 *	1. Allocated
 *	2. Written by CPU
 *	3. Mapped to GTT
 *	4. Read by GPU
 *	5. Unmapped from GTT
 *	6. Freed
 *
 *	Let's take these a step at a time
 *
 *	1. Allocated
 *		Pages allocated from the kernel may still have
 *		cache contents, so we set them to (CPU, CPU) always.
 *	2. Written by CPU (using pwrite)
 *		The pwrite function calls set_domain (CPU, CPU) and
 *		this function does nothing (as nothing changes)
 *	3. Mapped by GTT
 *		This function asserts that the object is not
 *		currently in any GPU-based read or write domains
 *	4. Read by GPU
 *		i915_gem_execbuffer calls set_domain (COMMAND, 0).
 *		As write_domain is zero, this function adds in the
 *		current read domains (CPU+COMMAND, 0).
 *		flush_domains is set to CPU.
 *		invalidate_domains is set to COMMAND
 *		clflush is run to get data out of the CPU caches
 *		then i915_dev_set_domain calls i915_gem_flush to
 *		emit an MI_FLUSH and drm_agp_chipset_flush
 *	5. Unmapped from GTT
 *		i915_gem_object_unbind calls set_domain (CPU, CPU)
 *		flush_domains and invalidate_domains end up both zero
 *		so no flushing/invalidating happens
 *	6. Freed
 *		yay, done
 *
 * Case 2: The shared render buffer
 *
 *	1. Allocated
 *	2. Mapped to GTT
 *	3. Read/written by GPU
 *	4. set_domain to (CPU,CPU)
 *	5. Read/written by CPU
 *	6. Read/written by GPU
 *
 *	1. Allocated
 *		Same as last example, (CPU, CPU)
 *	2. Mapped to GTT
 *		Nothing changes (assertions find that it is not in the GPU)
 *	3. Read/written by GPU
 *		execbuffer calls set_domain (RENDER, RENDER)
 *		flush_domains gets CPU
 *		invalidate_domains gets GPU
 *		clflush (obj)
 *		MI_FLUSH and drm_agp_chipset_flush
 *	4. set_domain (CPU, CPU)
 *		flush_domains gets GPU
 *		invalidate_domains gets CPU
 *		wait_rendering (obj) to make sure all drawing is complete.
 *		This will include an MI_FLUSH to get the data from GPU
 *		to memory
 *		clflush (obj) to invalidate the CPU cache
 *		Another MI_FLUSH in i915_gem_flush (eliminate this somehow?)
 *	5. Read/written by CPU
 *		cache lines are loaded and dirtied
 *	6. Read written by GPU
 *		Same as last GPU access
 *
 * Case 3: The constant buffer
 *
 *	1. Allocated
 *	2. Written by CPU
 *	3. Read by GPU
 *	4. Updated (written) by CPU again
 *	5. Read by GPU
 *
 *	1. Allocated
 *		(CPU, CPU)
 *	2. Written by CPU
 *		(CPU, CPU)
 *	3. Read by GPU
 *		(CPU+RENDER, 0)
 *		flush_domains = CPU
 *		invalidate_domains = RENDER
 *		clflush (obj)
 *		MI_FLUSH
 *		drm_agp_chipset_flush
 *	4. Updated (written) by CPU again
 *		(CPU, CPU)
 *		flush_domains = 0 (no previous write domain)
 *		invalidate_domains = 0 (no new read domains)
 *	5. Read by GPU
 *		(CPU+RENDER, 0)
 *		flush_domains = CPU
 *		invalidate_domains = RENDER
 *		clflush (obj)
 *		MI_FLUSH
 *		drm_agp_chipset_flush
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
	 * If the object isn't moving to a new write domain,
	 * let the object stay in multiple read domains
	 */
	if (write_domain == 0)
		read_domains |= obj->read_domains;

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
 * perform the necessary flush and invalidate operations.
 *
 * Returns the write domains flushed, for use in flush tracking.
 */
static uint32_t
i915_gem_dev_set_domain(struct drm_device *dev)
{
	uint32_t flush_domains = dev->flush_domains;

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

	return flush_domains;
}

static int
i915_gem_reloc_and_validate_object(struct drm_gem_object *obj,
				   struct drm_file *file_priv,
				   struct drm_i915_gem_exec_object *entry)
{
	struct drm_device *dev = obj->dev;
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

	entry->offset = obj_priv->gtt_offset;

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

/* Throttle our rendering by waiting until the ring has completed our requests
 * emitted over 20 msec ago.
 *
 * This should get us reasonable parallelism between CPU and GPU but also
 * relatively low latency when blocking on a particular request to finish.
 */
static int
i915_gem_ring_throttle(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret = 0;

	mutex_lock(&dev->struct_mutex);
	while (!list_empty(&dev_priv->mm.request_list)) {
		struct drm_i915_gem_request *request;

		request = list_first_entry(&dev_priv->mm.request_list,
					   struct drm_i915_gem_request,
					   list);

		/* Break out if we're close enough. */
		if ((long) (jiffies - request->emitted_jiffies) <= (20 * HZ) / 1000) {
			mutex_unlock(&dev->struct_mutex);
			return 0;
		}

		/* Wait on the last request if not. */
		ret = i915_wait_request(dev, request->seqno);
		if (ret != 0) {
			mutex_unlock(&dev->struct_mutex);
			return ret;
		}
	}
	mutex_unlock(&dev->struct_mutex);
	return ret;
}

int
i915_gem_execbuffer(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct drm_i915_gem_execbuffer *args = data;
	struct drm_i915_gem_exec_object *validate_list = NULL;
	struct drm_gem_object **object_list = NULL;
	struct drm_gem_object *batch_obj;
	int ret, i;
	uint64_t exec_offset;
	uint32_t seqno, flush_domains;

	LOCK_TEST_WITH_RETURN(dev, file_priv);

#if WATCH_EXEC
	DRM_INFO("buffers_ptr %d buffer_count %d len %08x\n",
		  (int) args->buffers_ptr, args->buffer_count, args->batch_len);
#endif
	i915_kernel_lost_context(dev);

	ret = i915_gem_ring_throttle(dev);
	if (ret)
		return ret;

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

	mutex_lock(&dev->struct_mutex);
	/* Look up object handles and perform the relocations */
	for (i = 0; i < args->buffer_count; i++) {
		object_list[i] = drm_gem_object_lookup(dev, file_priv,
						       validate_list[i].handle);
		if (object_list[i] == NULL) {
			DRM_ERROR("Invalid object handle %d at index %d\n",
				   validate_list[i].handle, i);
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
	flush_domains = i915_gem_dev_set_domain(dev);

	exec_offset = validate_list[args->buffer_count - 1].offset;

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
	 * Ensure that the commands in the batch buffer are
	 * finished before the interrupt fires
	 */
	flush_domains |= i915_retire_commands(dev);

	/*
	 * Get a seqno representing the execution of the current buffer,
	 * which we can wait on.  We would like to mitigate these interrupts,
	 * likely by only creating seqnos occasionally (so that we have
	 * *some* interrupts representing completion of buffers that we can
	 * wait on when trying to clear up gtt space).
	 */
	seqno = i915_add_request(dev, flush_domains);
	BUG_ON(seqno == 0);
	for (i = 0; i < args->buffer_count; i++) {
		struct drm_gem_object *obj = object_list[i];
		struct drm_i915_gem_object *obj_priv = obj->driver_private;

		i915_gem_object_move_to_active(obj);
		obj_priv->last_rendering_seqno = seqno;
#if WATCH_LRU
		DRM_INFO("%s: move to exec list %p\n", __func__, obj);
#endif
	}
#if WATCH_LRU
	i915_dump_lru(dev, __func__);
#endif

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
	mutex_unlock(&dev->struct_mutex);

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

	mutex_lock(&dev->struct_mutex);

	i915_kernel_lost_context(dev);
	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL) {
		DRM_ERROR("Bad handle in i915_gem_pin_ioctl(): %d\n",
			  args->handle);
		mutex_unlock(&dev->struct_mutex);
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
			mutex_unlock(&dev->struct_mutex);
			return ret;
		}
	}

	obj_priv->pin_count++;
	args->offset = obj_priv->gtt_offset;
	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);

	return 0;
}

int
i915_gem_unpin_ioctl(struct drm_device *dev, void *data,
		     struct drm_file *file_priv)
{
	struct drm_i915_gem_pin *args = data;
	struct drm_gem_object *obj;
	struct drm_i915_gem_object *obj_priv;

	mutex_lock(&dev->struct_mutex);

	i915_kernel_lost_context(dev);
	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL) {
		DRM_ERROR("Bad handle in i915_gem_unpin_ioctl(): %d\n",
			  args->handle);
		mutex_unlock(&dev->struct_mutex);
		return -EINVAL;
	}

	obj_priv = obj->driver_private;
	obj_priv->pin_count--;
	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);
	return 0;
}

int
i915_gem_busy_ioctl(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct drm_i915_gem_busy *args = data;
	struct drm_gem_object *obj;
	struct drm_i915_gem_object *obj_priv;

	mutex_lock(&dev->struct_mutex);
	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL) {
		DRM_ERROR("Bad handle in i915_gem_busy_ioctl(): %d\n",
			  args->handle);
		mutex_unlock(&dev->struct_mutex);
		return -EINVAL;
	}

	obj_priv = obj->driver_private;
	args->busy = obj_priv->active;
	
	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);
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
	INIT_LIST_HEAD(&obj_priv->list);
	return 0;
}

void i915_gem_free_object(struct drm_gem_object *obj)
{
	i915_kernel_lost_context(obj->dev);
	i915_gem_object_unbind(obj);

	drm_free(obj->driver_private, 1, DRM_MEM_DRIVER);
}

int
i915_gem_set_domain(struct drm_gem_object *obj,
		    struct drm_file *file_priv,
		    uint32_t read_domains,
		    uint32_t write_domain)
{
	struct drm_device *dev = obj->dev;

	BUG_ON(!mutex_is_locked(&dev->struct_mutex));

	drm_client_lock_take(dev, file_priv);
	i915_kernel_lost_context(dev);
	i915_gem_object_set_domain(obj, read_domains, write_domain);
	i915_gem_dev_set_domain(obj->dev);
	drm_client_lock_release(dev);

	return 0;
}

int
i915_gem_flush_pwrite(struct drm_gem_object *obj,
		      uint64_t offset, uint64_t size)
{
#if 0
	struct drm_device *dev = obj->dev;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;

	/*
	 * For writes much less than the size of the object and
	 * which are already pinned in memory, do the flush right now
	 */

	if ((size < obj->size >> 1) && obj_priv->page_list != NULL) {
		unsigned long first_page = offset / PAGE_SIZE;
		unsigned long beyond_page = roundup(offset + size, PAGE_SIZE) / PAGE_SIZE;

		drm_ttm_cache_flush(obj_priv->page_list + first_page,
				    beyond_page - first_page);
		drm_agp_chipset_flush(dev);
		obj->write_domain = 0;
	}
#endif
	return 0;
}

void
i915_gem_lastclose(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	mutex_lock(&dev->struct_mutex);

	/* Assume that the chip has been idled at this point. Just pull them
	 * off the execution list and unref them.  Since this is the last
	 * close, this is also the last ref and they'll go away.
	 */

	while (!list_empty(&dev_priv->mm.active_list)) {
		struct drm_i915_gem_object *obj_priv;

		obj_priv = list_first_entry(&dev_priv->mm.active_list,
					    struct drm_i915_gem_object,
					    list);

		list_del_init(&obj_priv->list);
		obj_priv->active = 0;
		obj_priv->obj->write_domain = 0;
		drm_gem_object_unreference(obj_priv->obj);
	}

	mutex_unlock(&dev->struct_mutex);
}
