/*
 * Copyright 2003-2008 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *     Thomas Hellstrom <thomas-at-tungstengraphics-dot-com>
 *     Dave Airlie
 *     Keith Packard
 *     ... ?
 */

#include "drmP.h"
#include "drm.h"
#include "i915_drm.h"
#include "i915_drv.h"

#if DRM_DEBUG_CODE
#define DRM_DEBUG_RELOCATION	(drm_debug != 0)
#else
#define DRM_DEBUG_RELOCATION	0
#endif

enum i915_buf_idle {
	I915_RELOC_UNCHECKED,
	I915_RELOC_IDLE,
	I915_RELOC_BUSY
};

struct i915_relocatee_info {
	struct drm_buffer_object *buf;
	unsigned long offset;
	uint32_t *data_page;
	unsigned page_offset;
	struct drm_bo_kmap_obj kmap;
	int is_iomem;
	int dst;
	int idle;
	int performed_ring_relocs;
#ifdef DRM_KMAP_ATOMIC_PROT_PFN
	unsigned long pfn;
	pgprot_t pg_prot;
#endif
};

struct drm_i915_validate_buffer {
	struct drm_buffer_object *buffer;
	int presumed_offset_correct;
	void __user *data;
	int ret;
	enum i915_buf_idle idle;
};

/*
 * I'd like to use MI_STORE_DATA_IMM here, but I can't make
 * it work. Seems like GART writes are broken with that
 * instruction. Also I'm not sure that MI_FLUSH will
 * act as a memory barrier for that instruction. It will
 * for this single dword 2D blit.
 */

static void i915_emit_ring_reloc(struct drm_device *dev, uint32_t offset,
				 uint32_t value)
{
	struct drm_i915_private *dev_priv =
	    (struct drm_i915_private *)dev->dev_private;

	RING_LOCALS;
	i915_kernel_lost_context(dev);
	BEGIN_LP_RING(6);
	OUT_RING((0x02 << 29) | (0x40 << 22) | (0x3 << 20) | (0x3));
	OUT_RING((0x3 << 24) | (0xF0 << 16) | (0x40));
	OUT_RING((0x1 << 16) | (0x4));
	OUT_RING(offset);
	OUT_RING(value);
	OUT_RING(0);
	ADVANCE_LP_RING();
}

static void i915_dereference_buffers_locked(struct drm_i915_validate_buffer
					    *buffers, unsigned num_buffers)
{
	while (num_buffers--)
		drm_bo_usage_deref_locked(&buffers[num_buffers].buffer);
}

int i915_apply_reloc(struct drm_file *file_priv, int num_buffers,
		     struct drm_i915_validate_buffer *buffers,
		     struct i915_relocatee_info *relocatee, uint32_t * reloc)
{
	unsigned index;
	unsigned long new_cmd_offset;
	u32 val;
	int ret, i;
	int buf_index = -1;

	/*
	 * FIXME: O(relocs * buffers) complexity.
	 */

	for (i = 0; i <= num_buffers; i++)
		if (buffers[i].buffer)
			if (reloc[2] == buffers[i].buffer->base.hash.key)
				buf_index = i;

	if (buf_index == -1) {
		DRM_ERROR("Illegal relocation buffer %08X\n", reloc[2]);
		return -EINVAL;
	}

	/*
	 * Short-circuit relocations that were correctly
	 * guessed by the client
	 */
	if (buffers[buf_index].presumed_offset_correct && !DRM_DEBUG_RELOCATION)
		return 0;

	new_cmd_offset = reloc[0];
	if (!relocatee->data_page ||
	    !drm_bo_same_page(relocatee->offset, new_cmd_offset)) {
		struct drm_bo_mem_reg *mem = &relocatee->buf->mem;

		drm_bo_kunmap(&relocatee->kmap);
		relocatee->data_page = NULL;
		relocatee->offset = new_cmd_offset;

		if (unlikely(relocatee->idle == I915_RELOC_UNCHECKED)) {
			ret = drm_bo_wait(relocatee->buf, 0, 0, 0);
			if (ret)
				return ret;
			relocatee->idle = I915_RELOC_IDLE;
		}

		if (unlikely((mem->mem_type != DRM_BO_MEM_LOCAL) &&
			     (mem->flags & DRM_BO_FLAG_CACHED_MAPPED)))
			drm_bo_evict_cached(relocatee->buf);

		ret = drm_bo_kmap(relocatee->buf, new_cmd_offset >> PAGE_SHIFT,
				  1, &relocatee->kmap);
		if (ret) {
			DRM_ERROR
			    ("Could not map command buffer to apply relocs\n %08lx",
			     new_cmd_offset);
			return ret;
		}
		relocatee->data_page = drm_bmo_virtual(&relocatee->kmap,
						       &relocatee->is_iomem);
		relocatee->page_offset = (relocatee->offset & PAGE_MASK);
	}

	val = buffers[buf_index].buffer->offset;
	index = (reloc[0] - relocatee->page_offset) >> 2;

	/* add in validate */
	val = val + reloc[1];

	if (DRM_DEBUG_RELOCATION) {
		if (buffers[buf_index].presumed_offset_correct &&
		    relocatee->data_page[index] != val) {
			DRM_DEBUG
			    ("Relocation mismatch source %d target %d buffer %d user %08x kernel %08x\n",
			     reloc[0], reloc[1], buf_index,
			     relocatee->data_page[index], val);
		}
	}

	if (relocatee->is_iomem)
		iowrite32(val, relocatee->data_page + index);
	else
		relocatee->data_page[index] = val;
	return 0;
}

int i915_process_relocs(struct drm_file *file_priv,
			uint32_t buf_handle,
			uint32_t __user ** reloc_user_ptr,
			struct i915_relocatee_info *relocatee,
			struct drm_i915_validate_buffer *buffers,
			uint32_t num_buffers)
{
	int ret, reloc_stride;
	uint32_t cur_offset;
	uint32_t reloc_count;
	uint32_t reloc_type;
	uint32_t reloc_buf_size;
	uint32_t *reloc_buf = NULL;
	int i;

	/* do a copy from user from the user ptr */
	ret = get_user(reloc_count, *reloc_user_ptr);
	if (ret) {
		DRM_ERROR("Could not map relocation buffer.\n");
		goto out;
	}

	ret = get_user(reloc_type, (*reloc_user_ptr) + 1);
	if (ret) {
		DRM_ERROR("Could not map relocation buffer.\n");
		goto out;
	}

	if (reloc_type != 0) {
		DRM_ERROR("Unsupported relocation type requested\n");
		ret = -EINVAL;
		goto out;
	}

	reloc_buf_size =
	    (I915_RELOC_HEADER +
	     (reloc_count * I915_RELOC0_STRIDE)) * sizeof(uint32_t);
	reloc_buf = kmalloc(reloc_buf_size, GFP_KERNEL);
	if (!reloc_buf) {
		DRM_ERROR("Out of memory for reloc buffer\n");
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(reloc_buf, *reloc_user_ptr, reloc_buf_size)) {
		ret = -EFAULT;
		goto out;
	}

	/* get next relocate buffer handle */
	*reloc_user_ptr = (uint32_t *) * (unsigned long *)&reloc_buf[2];

	reloc_stride = I915_RELOC0_STRIDE * sizeof(uint32_t);	/* may be different for other types of relocs */

	DRM_DEBUG("num relocs is %d, next is %p\n", reloc_count,
		  *reloc_user_ptr);

	for (i = 0; i < reloc_count; i++) {
		cur_offset = I915_RELOC_HEADER + (i * I915_RELOC0_STRIDE);

		ret = i915_apply_reloc(file_priv, num_buffers, buffers,
				       relocatee, reloc_buf + cur_offset);
		if (ret)
			goto out;
	}

      out:
	if (reloc_buf)
		kfree(reloc_buf);

	if (relocatee->data_page) {
		drm_bo_kunmap(&relocatee->kmap);
		relocatee->data_page = NULL;
	}

	return ret;
}

static int i915_exec_reloc(struct drm_file *file_priv, drm_handle_t buf_handle,
			   uint32_t __user * reloc_user_ptr,
			   struct drm_i915_validate_buffer *buffers,
			   uint32_t buf_count)
{
	struct drm_device *dev = file_priv->minor->dev;
	struct i915_relocatee_info relocatee;
	int ret = 0;
	int b;

	/*
	 * Short circuit relocations when all previous
	 * buffers offsets were correctly guessed by
	 * the client
	 */
	if (!DRM_DEBUG_RELOCATION) {
		for (b = 0; b < buf_count; b++)
			if (!buffers[b].presumed_offset_correct)
				break;

		if (b == buf_count)
			return 0;
	}

	memset(&relocatee, 0, sizeof(relocatee));
	relocatee.idle = I915_RELOC_UNCHECKED;

	mutex_lock(&dev->struct_mutex);
	relocatee.buf = drm_lookup_buffer_object(file_priv, buf_handle, 1);
	mutex_unlock(&dev->struct_mutex);
	if (!relocatee.buf) {
		DRM_DEBUG("relocatee buffer invalid %08x\n", buf_handle);
		ret = -EINVAL;
		goto out_err;
	}

	mutex_lock(&relocatee.buf->mutex);
	while (reloc_user_ptr) {
		ret =
		    i915_process_relocs(file_priv, buf_handle, &reloc_user_ptr,
					&relocatee, buffers, buf_count);
		if (ret) {
			DRM_ERROR("process relocs failed\n");
			goto out_err1;
		}
	}

      out_err1:
	mutex_unlock(&relocatee.buf->mutex);
	drm_bo_usage_deref_unlocked(&relocatee.buf);
      out_err:
	return ret;
}

static void i915_clear_relocatee(struct i915_relocatee_info *relocatee)
{
	if (relocatee->data_page) {
#ifndef DRM_KMAP_ATOMIC_PROT_PFN
		drm_bo_kunmap(&relocatee->kmap);
#else
		kunmap_atomic(relocatee->data_page, KM_USER0);
#endif
		relocatee->data_page = NULL;
	}
	relocatee->buf = NULL;
	relocatee->dst = ~0;
}

static int i915_update_relocatee(struct i915_relocatee_info *relocatee,
				 struct drm_i915_validate_buffer *buffers,
				 unsigned int dst, unsigned long dst_offset)
{
	int ret;

	if (unlikely(dst != relocatee->dst || NULL == relocatee->buf)) {
		i915_clear_relocatee(relocatee);
		relocatee->dst = dst;
		relocatee->buf = buffers[dst].buffer;
		relocatee->idle = buffers[dst].idle;

		/*
		 * Check for buffer idle. If the buffer is busy, revert to
		 * ring relocations.
		 */

		if (relocatee->idle == I915_RELOC_UNCHECKED) {
			preempt_enable();
			ret = mutex_lock_interruptible(&relocatee->buf->mutex);
			if (unlikely(ret))
				return -EAGAIN;

			ret = drm_bo_wait(relocatee->buf, 0, 0, 1);
			if (ret == 0)
				relocatee->idle = I915_RELOC_IDLE;
			else {
				relocatee->idle = I915_RELOC_BUSY;
				relocatee->performed_ring_relocs = 1;
			}
			mutex_unlock(&relocatee->buf->mutex);
			preempt_disable();
			buffers[dst].idle = relocatee->idle;
		}
	}

	if (relocatee->idle == I915_RELOC_BUSY)
		return 0;

	if (unlikely(dst_offset > relocatee->buf->num_pages * PAGE_SIZE)) {
		DRM_ERROR("Relocation destination out of bounds.\n");
		return -EINVAL;
	}
	if (unlikely(!drm_bo_same_page(relocatee->page_offset, dst_offset) ||
		     NULL == relocatee->data_page)) {
#ifdef DRM_KMAP_ATOMIC_PROT_PFN
		if (NULL != relocatee->data_page) {
			kunmap_atomic(relocatee->data_page, KM_USER0);
			relocatee->data_page = NULL;
		}
		ret = drm_bo_pfn_prot(relocatee->buf, dst_offset,
				      &relocatee->pfn, &relocatee->pg_prot);
		if (ret) {
			DRM_ERROR("Can't map relocation destination.\n");
			return -EINVAL;
		}
		relocatee->data_page =
		    kmap_atomic_prot_pfn(relocatee->pfn, KM_USER0,
					 relocatee->pg_prot);
#else
		if (NULL != relocatee->data_page) {
			drm_bo_kunmap(&relocatee->kmap);
			relocatee->data_page = NULL;
		}

		ret = drm_bo_kmap(relocatee->buf, dst_offset >> PAGE_SHIFT,
				  1, &relocatee->kmap);
		if (ret) {
			DRM_ERROR("Can't map relocation destination.\n");
			return ret;
		}

		relocatee->data_page = drm_bmo_virtual(&relocatee->kmap,
						       &relocatee->is_iomem);
#endif
		relocatee->page_offset = dst_offset & PAGE_MASK;
	}
	return 0;
}

static int i915_apply_post_reloc(uint32_t reloc[],
				 struct drm_i915_validate_buffer *buffers,
				 uint32_t num_buffers,
				 struct i915_relocatee_info *relocatee)
{
	uint32_t reloc_buffer = reloc[2];
	uint32_t dst_buffer = reloc[3];
	uint32_t val;
	uint32_t index;
	int ret;

	if (likely(buffers[reloc_buffer].presumed_offset_correct))
		return 0;
	if (unlikely(reloc_buffer >= num_buffers)) {
		DRM_ERROR("Invalid reloc buffer index.\n");
		return -EINVAL;
	}
	if (unlikely(dst_buffer >= num_buffers)) {
		DRM_ERROR("Invalid dest buffer index.\n");
		return -EINVAL;
	}

	ret = i915_update_relocatee(relocatee, buffers, dst_buffer, reloc[0]);
	if (unlikely(ret))
		return ret;

	val = buffers[reloc_buffer].buffer->offset;
	index = (reloc[0] - relocatee->page_offset) >> 2;
	val = val + reloc[1];

	if (relocatee->idle == I915_RELOC_BUSY) {
		i915_emit_ring_reloc(relocatee->buf->dev,
				     relocatee->buf->offset + reloc[0], val);
		return 0;
	}
#ifdef DRM_KMAP_ATOMIC_PROT_PFN
	relocatee->data_page[index] = val;
#else
	if (likely(relocatee->is_iomem))
		iowrite32(val, relocatee->data_page + index);
	else
		relocatee->data_page[index] = val;
#endif

	return 0;
}

static int i915_post_relocs(struct drm_file *file_priv,
			    uint32_t __user * new_reloc_ptr,
			    struct drm_i915_validate_buffer *buffers,
			    unsigned int num_buffers)
{
	uint32_t *reloc;
	uint32_t reloc_stride = I915_RELOC0_STRIDE * sizeof(uint32_t);
	uint32_t header_size = I915_RELOC_HEADER * sizeof(uint32_t);
	struct i915_relocatee_info relocatee;
	uint32_t reloc_type;
	uint32_t num_relocs;
	uint32_t count;
	int ret = 0;
	int i;
	int short_circuit = 1;
	uint32_t __user *reloc_ptr;
	uint64_t new_reloc_data;
	uint32_t reloc_buf_size;
	uint32_t *reloc_buf;

	for (i = 0; i < num_buffers; ++i) {
		if (unlikely(!buffers[i].presumed_offset_correct)) {
			short_circuit = 0;
			break;
		}
	}

	if (likely(short_circuit))
		return 0;

	memset(&relocatee, 0, sizeof(relocatee));

	while (new_reloc_ptr) {
		reloc_ptr = new_reloc_ptr;

		ret = get_user(num_relocs, reloc_ptr);
		if (unlikely(ret))
			goto out;
		if (unlikely(!access_ok(VERIFY_READ, reloc_ptr,
					header_size +
					num_relocs * reloc_stride)))
			return -EFAULT;

		ret = __get_user(reloc_type, reloc_ptr + 1);
		if (unlikely(ret))
			goto out;

		if (unlikely(reloc_type != 1)) {
			DRM_ERROR("Unsupported relocation type requested.\n");
			ret = -EINVAL;
			goto out;
		}

		ret = __get_user(new_reloc_data, reloc_ptr + 2);
		new_reloc_ptr = (uint32_t __user *) (unsigned long)
		    new_reloc_data;

		reloc_ptr += I915_RELOC_HEADER;

		if (num_relocs == 0)
			goto out;

		reloc_buf_size =
		    (num_relocs * I915_RELOC0_STRIDE) * sizeof(uint32_t);
		reloc_buf = kmalloc(reloc_buf_size, GFP_KERNEL);
		if (!reloc_buf) {
			DRM_ERROR("Out of memory for reloc buffer\n");
			ret = -ENOMEM;
			goto out;
		}

		if (__copy_from_user(reloc_buf, reloc_ptr, reloc_buf_size)) {
			ret = -EFAULT;
			goto out;
		}
		reloc = reloc_buf;
		preempt_disable();
		for (count = 0; count < num_relocs; ++count) {
			ret = i915_apply_post_reloc(reloc, buffers,
						    num_buffers, &relocatee);
			if (unlikely(ret)) {
				preempt_enable();
				goto out;
			}
			reloc += I915_RELOC0_STRIDE;
		}
		preempt_enable();

		if (reloc_buf) {
			kfree(reloc_buf);
			reloc_buf = NULL;
		}
		i915_clear_relocatee(&relocatee);
	}

      out:
	/*
	 * Flush ring relocs so the command parser will pick them up.
	 */

	if (relocatee.performed_ring_relocs)
		(void)i915_emit_mi_flush(file_priv->minor->dev, 0);

	i915_clear_relocatee(&relocatee);
	if (reloc_buf) {
		kfree(reloc_buf);
		reloc_buf = NULL;
	}

	return ret;
}

static int i915_check_presumed(struct drm_i915_op_arg *arg,
			       struct drm_buffer_object *bo,
			       uint32_t __user * data, int *presumed_ok)
{
	struct drm_bo_op_req *req = &arg->d.req;
	uint32_t hint_offset;
	uint32_t hint = req->bo_req.hint;

	*presumed_ok = 0;

	if (!(hint & DRM_BO_HINT_PRESUMED_OFFSET))
		return 0;
	if (bo->offset == req->bo_req.presumed_offset) {
		*presumed_ok = 1;
		return 0;
	}

	/*
	 * We need to turn off the HINT_PRESUMED_OFFSET for this buffer in
	 * the user-space IOCTL argument list, since the buffer has moved,
	 * we're about to apply relocations and we might subsequently
	 * hit an -EAGAIN. In that case the argument list will be reused by
	 * user-space, but the presumed offset is no longer valid.
	 *
	 * Needless to say, this is a bit ugly.
	 */

	hint_offset = (uint32_t *) & req->bo_req.hint - (uint32_t *) arg;
	hint &= ~DRM_BO_HINT_PRESUMED_OFFSET;
	return __put_user(hint, data + hint_offset);
}

/*
 * Validate, add fence and relocate a block of bos from a userspace list
 */
int i915_validate_buffer_list(struct drm_file *file_priv,
			      unsigned int fence_class, uint64_t data,
			      struct drm_i915_validate_buffer *buffers,
			      uint32_t * num_buffers,
			      uint32_t __user ** post_relocs)
{
	struct drm_i915_op_arg arg;
	struct drm_bo_op_req *req = &arg.d.req;
	int ret = 0;
	unsigned buf_count = 0;
	uint32_t buf_handle;
	uint32_t __user *reloc_user_ptr;
	struct drm_i915_validate_buffer *item = buffers;
	*post_relocs = NULL;

	do {
		if (buf_count >= *num_buffers) {
			DRM_ERROR("Buffer count exceeded %d\n.", *num_buffers);
			ret = -EINVAL;
			goto out_err;
		}
		item = buffers + buf_count;
		item->buffer = NULL;
		item->presumed_offset_correct = 0;
		item->idle = I915_RELOC_UNCHECKED;

		if (copy_from_user
		    (&arg, (void __user *)(unsigned long)data, sizeof(arg))) {
			ret = -EFAULT;
			goto out_err;
		}

		ret = 0;
		if (req->op != drm_bo_validate) {
			DRM_ERROR
			    ("Buffer object operation wasn't \"validate\".\n");
			ret = -EINVAL;
			goto out_err;
		}
		item->ret = 0;
		item->data = (void __user *)(unsigned long)data;

		buf_handle = req->bo_req.handle;
		reloc_user_ptr = (uint32_t *) (unsigned long)arg.reloc_ptr;

		/*
		 * Switch mode to post-validation relocations?
		 */

		if (unlikely((buf_count == 0) && (*post_relocs == NULL) &&
			     (reloc_user_ptr != NULL))) {
			uint32_t reloc_type;

			ret = get_user(reloc_type, reloc_user_ptr + 1);
			if (ret)
				goto out_err;

			if (reloc_type == 1)
				*post_relocs = reloc_user_ptr;

		}

		if ((*post_relocs == NULL) && (reloc_user_ptr != NULL)) {
			ret =
			    i915_exec_reloc(file_priv, buf_handle,
					    reloc_user_ptr, buffers, buf_count);
			if (ret)
				goto out_err;
			DRM_MEMORYBARRIER();
		}

		ret = drm_bo_handle_validate(file_priv, req->bo_req.handle,
					     req->bo_req.flags,
					     req->bo_req.mask, req->bo_req.hint,
					     req->bo_req.fence_class, 0,
					     NULL, &item->buffer);
		if (ret) {
			DRM_ERROR("error on handle validate %d\n", ret);
			goto out_err;
		}

		buf_count++;

		ret = i915_check_presumed(&arg, item->buffer,
					  (uint32_t __user *)
					  (unsigned long)data,
					  &item->presumed_offset_correct);
		if (ret)
			goto out_err;

		data = arg.next;
	} while (data != 0);
      out_err:
	*num_buffers = buf_count;
	item->ret = (ret != -EAGAIN) ? ret : 0;
	return ret;
}

/*
 * Remove all buffers from the unfenced list.
 * If the execbuffer operation was aborted, for example due to a signal,
 * this also make sure that buffers retain their original state and
 * fence pointers.
 * Copy back buffer information to user-space unless we were interrupted
 * by a signal. In which case the IOCTL must be rerun.
 */

static int i915_handle_copyback(struct drm_device *dev,
				struct drm_i915_validate_buffer *buffers,
				unsigned int num_buffers, int ret)
{
	int err = ret;
	int i;
	struct drm_i915_op_arg arg;
	struct drm_buffer_object *bo;

	if (ret)
		drm_putback_buffer_objects(dev);

	if (ret != -EAGAIN) {
		for (i = 0; i < num_buffers; ++i) {
			arg.handled = 1;
			arg.d.rep.ret = buffers->ret;
			bo = buffers->buffer;
			mutex_lock(&bo->mutex);
			drm_bo_fill_rep_arg(bo, &arg.d.rep.bo_info);
			mutex_unlock(&bo->mutex);
			if (__copy_to_user(buffers->data, &arg, sizeof(arg)))
				err = -EFAULT;
			buffers++;
		}
	}

	return err;
}

/*
 * Create a fence object, and if that fails, pretend that everything is
 * OK and just idle the GPU.
 */

void i915_fence_or_sync(struct drm_file *file_priv,
			uint32_t fence_flags,
			struct drm_fence_arg *fence_arg,
			struct drm_fence_object **fence_p)
{
	struct drm_device *dev = file_priv->minor->dev;
	int ret;
	struct drm_fence_object *fence;

	ret = drm_fence_buffer_objects(dev, NULL, fence_flags, NULL, &fence);

	if (ret) {

		/*
		 * Fence creation failed.
		 * Fall back to synchronous operation and idle the engine.
		 */

		(void)i915_emit_mi_flush(dev, MI_READ_FLUSH);
		(void)i915_quiescent(dev);

		if (!(fence_flags & DRM_FENCE_FLAG_NO_USER)) {

			/*
			 * Communicate to user-space that
			 * fence creation has failed and that
			 * the engine is idle.
			 */

			fence_arg->handle = ~0;
			fence_arg->error = ret;
		}
		drm_putback_buffer_objects(dev);
		if (fence_p)
			*fence_p = NULL;
		return;
	}

	if (!(fence_flags & DRM_FENCE_FLAG_NO_USER)) {

		ret = drm_fence_add_user_object(file_priv, fence,
						fence_flags &
						DRM_FENCE_FLAG_SHAREABLE);
		if (!ret)
			drm_fence_fill_arg(fence, fence_arg);
		else {
			/*
			 * Fence user object creation failed.
			 * We must idle the engine here as well, as user-
			 * space expects a fence object to wait on. Since we
			 * have a fence object we wait for it to signal
			 * to indicate engine "sufficiently" idle.
			 */

			(void)drm_fence_object_wait(fence, 0, 1, fence->type);
			drm_fence_usage_deref_unlocked(&fence);
			fence_arg->handle = ~0;
			fence_arg->error = ret;
		}
	}

	if (fence_p)
		*fence_p = fence;
	else if (fence)
		drm_fence_usage_deref_unlocked(&fence);
}

int i915_execbuffer(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	drm_i915_sarea_t *sarea_priv = (drm_i915_sarea_t *)
	    dev_priv->sarea_priv;
	struct drm_i915_execbuffer *exec_buf = data;
	struct drm_i915_batchbuffer *batch = &exec_buf->batch;
	struct drm_fence_arg *fence_arg = &exec_buf->fence_arg;
	int num_buffers;
	int ret;
	uint32_t __user *post_relocs;

	if (!dev_priv->allow_batchbuffer) {
		DRM_ERROR("Batchbuffer ioctl disabled\n");
		return -EINVAL;
	}

	if (batch->num_cliprects && DRM_VERIFYAREA_READ(batch->cliprects,
							batch->num_cliprects *
							sizeof(struct
							       drm_clip_rect)))
		return -EFAULT;

	if (exec_buf->num_buffers > dev_priv->max_validate_buffers)
		return -EINVAL;

	ret = drm_bo_read_lock(&dev->bm.bm_lock);
	if (ret)
		return ret;

	/*
	 * The cmdbuf_mutex makes sure the validate-submit-fence
	 * operation is atomic.
	 */

	ret = mutex_lock_interruptible(&dev_priv->cmdbuf_mutex);
	if (ret) {
		drm_bo_read_unlock(&dev->bm.bm_lock);
		return -EAGAIN;
	}

	num_buffers = exec_buf->num_buffers;

	if (!dev_priv->val_bufs) {
		dev_priv->val_bufs =
		    vmalloc(sizeof(struct drm_i915_validate_buffer) *
			    dev_priv->max_validate_buffers);
	}
	if (!dev_priv->val_bufs) {
		drm_bo_read_unlock(&dev->bm.bm_lock);
		mutex_unlock(&dev_priv->cmdbuf_mutex);
		return -ENOMEM;
	}

	/* validate buffer list + fixup relocations */
	ret = i915_validate_buffer_list(file_priv, 0, exec_buf->ops_list,
					dev_priv->val_bufs, &num_buffers,
					&post_relocs);
	if (ret)
		goto out_err0;

	if (post_relocs) {
		ret = i915_post_relocs(file_priv, post_relocs,
				       dev_priv->val_bufs, num_buffers);
		if (ret)
			goto out_err0;
	}

	/* make sure all previous memory operations have passed */
	DRM_MEMORYBARRIER();

	if (!post_relocs) {
		drm_agp_chipset_flush(dev);
		batch->start =
		    dev_priv->val_bufs[num_buffers - 1].buffer->offset;
	} else {
		batch->start += dev_priv->val_bufs[0].buffer->offset;
	}

	DRM_DEBUG("i915 exec batchbuffer, start %x used %d cliprects %d\n",
		  batch->start, batch->used, batch->num_cliprects);

	ret = i915_dispatch_batchbuffer(dev, batch);
	if (ret)
		goto out_err0;
	if (sarea_priv)
		sarea_priv->last_dispatch = READ_BREADCRUMB(dev_priv);
	i915_fence_or_sync(file_priv, fence_arg->flags, fence_arg, NULL);

      out_err0:
	ret = i915_handle_copyback(dev, dev_priv->val_bufs, num_buffers, ret);
	mutex_lock(&dev->struct_mutex);
	i915_dereference_buffers_locked(dev_priv->val_bufs, num_buffers);
	mutex_unlock(&dev->struct_mutex);
	mutex_unlock(&dev_priv->cmdbuf_mutex);
	drm_bo_read_unlock(&dev->bm.bm_lock);
	return ret;
}
