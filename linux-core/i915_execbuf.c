/*
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
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
 */

#include "drmP.h"
#include "drm.h"
#include "i915_drm.h"
#include "i915_drv.h"

struct i915_relocatee_info {
	struct drm_buffer_object *buf;
	unsigned long offset;
	uint32_t *data_page;
	unsigned page_offset;
	struct drm_bo_kmap_obj kmap;
	int is_iomem;
	int idle;
        int dst;
#ifdef DRM_KMAP_ATOMIC_PROT_PFN
	unsigned long pfn;
	pgprot_t pg_prot;
#endif
};

struct drm_i915_validate_buffer {
	struct drm_buffer_object *buffer;
	struct drm_bo_info_rep rep;
	int presumed_offset_correct;
	void __user *data;
	int ret;
};


static int i915_update_relocatee(struct i915_relocatee_info *relocatee,
				 struct drm_i915_validate_buffer *buffers,
				 unsigned int dst,
				 unsigned long dst_offset)
{
	int ret;

	if (unlikely(dst != relocatee->dst || NULL == relocatee->buf)) {
		i915_clear_relocatee(relocatee);
		relocatee->dst = dst;
		relocatee->buf = buffers[dst].buffer;
		preempt_enable();
		ret = mutex_lock_interruptible(&relocatee->buf->mutex);
		preempt_disable();
		if (unlikely(ret))
			return -EAGAIN;

		ret = drm_bo_wait(relocatee->buf, 0, 0, 0);
		if (unlikely(ret))
			return ret;

		mutex_unlock(&relocatee->buf->mutex);
	}

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
				      &relocatee->pfn,
				      &relocatee->pg_prot);
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


static void i915_dereference_buffers_locked(struct drm_i915_validate_buffer *buffers,
					    unsigned num_buffers)
{
	while (num_buffers--)
		drm_bo_usage_deref_locked(&buffers[num_buffers].buffer);
}

int i915_apply_reloc(struct drm_file *file_priv, int num_buffers,
		     struct drm_i915_validate_buffer *buffers,
		     struct i915_relocatee_info *relocatee,
		     uint32_t *reloc)
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
		drm_bo_kunmap(&relocatee->kmap);
		relocatee->data_page = NULL;
		relocatee->offset = new_cmd_offset;

		/*
		 * Note on buffer idle:
		 * Since we're applying relocations, this part of the
		 * buffer is obviously not used by the GPU and we don't
		 * need to wait for buffer idle. This is an important
		 * consideration for user-space buffer pools.
		 */

		ret = drm_bo_kmap(relocatee->buf, new_cmd_offset >> PAGE_SHIFT,
				  1, &relocatee->kmap);
		if (ret) {
			DRM_ERROR("Could not map command buffer to apply relocs\n %08lx", new_cmd_offset);
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
			DRM_DEBUG ("Relocation mismatch source %d target %d buffer %d user %08x kernel %08x\n",
				   reloc[0], reloc[1], buf_index, relocatee->data_page[index], val);
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
			uint32_t __user **reloc_user_ptr,
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

	ret = get_user(reloc_type, (*reloc_user_ptr)+1);
	if (ret) {
		DRM_ERROR("Could not map relocation buffer.\n");
		goto out;
	}

	if (reloc_type != 0) {
		DRM_ERROR("Unsupported relocation type requested\n");
		ret = -EINVAL;
		goto out;
	}

	reloc_buf_size = (I915_RELOC_HEADER + (reloc_count * I915_RELOC0_STRIDE)) * sizeof(uint32_t);
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
	*reloc_user_ptr = (uint32_t *)*(unsigned long *)&reloc_buf[2];

	reloc_stride = I915_RELOC0_STRIDE * sizeof(uint32_t); /* may be different for other types of relocs */

	DRM_DEBUG("num relocs is %d, next is %p\n", reloc_count, *reloc_user_ptr);

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
			   uint32_t __user *reloc_user_ptr,
			   struct drm_i915_validate_buffer *buffers,
			   uint32_t buf_count)
{
	struct drm_device *dev = file_priv->head->dev;
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

	mutex_lock(&dev->struct_mutex);
	relocatee.buf = drm_lookup_buffer_object(file_priv, buf_handle, 1);
	mutex_unlock(&dev->struct_mutex);
	if (!relocatee.buf) {
		DRM_DEBUG("relocatee buffer invalid %08x\n", buf_handle);
		ret = -EINVAL;
		goto out_err;
	}

	mutex_lock (&relocatee.buf->mutex);
	while (reloc_user_ptr) {
		ret = i915_process_relocs(file_priv, buf_handle, &reloc_user_ptr, &relocatee, buffers, buf_count);
		if (ret) {
			DRM_ERROR("process relocs failed\n");
			goto out_err1;
		}
	}

out_err1:
	mutex_unlock (&relocatee.buf->mutex);
	drm_bo_usage_deref_unlocked(&relocatee.buf);
out_err:
	return ret;
}

static int i915_check_presumed(struct drm_i915_op_arg *arg,
			       struct drm_buffer_object *bo,
			       uint32_t __user *data,
			       int *presumed_ok)
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

       	hint_offset = (uint32_t *)&req->bo_req.hint - (uint32_t *)arg;
	hint &= ~DRM_BO_HINT_PRESUMED_OFFSET;
	return __put_user(hint, data + hint_offset);
}


/*
 * Validate, add fence and relocate a block of bos from a userspace list
 */
int i915_validate_buffer_list(struct drm_file *file_priv,
			      unsigned int fence_class, uint64_t data,
			      struct drm_i915_validate_buffer *buffers,
			      uint32_t *num_buffers)
{
	struct drm_i915_op_arg arg;
	struct drm_bo_op_req *req = &arg.d.req;
	int ret = 0;
	unsigned buf_count = 0;
	uint32_t buf_handle;
	uint32_t __user *reloc_user_ptr;
	struct drm_i915_validate_buffer *item = buffers;

	do {
		if (buf_count >= *num_buffers) {
			DRM_ERROR("Buffer count exceeded %d\n.", *num_buffers);
			ret = -EINVAL;
			goto out_err;
		}
		item = buffers + buf_count;
		item->buffer = NULL;
		item->presumed_offset_correct = 0;

		buffers[buf_count].buffer = NULL;

		if (copy_from_user(&arg, (void __user *)(unsigned long)data, sizeof(arg))) {
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
		item->data = (void __user *) (unsigned long) data;

		buf_handle = req->bo_req.handle;
		reloc_user_ptr = (uint32_t *)(unsigned long)arg.reloc_ptr;

		if (reloc_user_ptr) {
			ret = i915_exec_reloc(file_priv, buf_handle, reloc_user_ptr, buffers, buf_count);
			if (ret)
				goto out_err;
			DRM_MEMORYBARRIER();
		}

		ret = drm_bo_handle_validate(file_priv, req->bo_req.handle,
					     req->bo_req.flags, req->bo_req.mask,
					     req->bo_req.hint,
					     req->bo_req.fence_class, 0,
					     &item->rep,
					     &item->buffer);

		if (ret) {
			DRM_ERROR("error on handle validate %d\n", ret);
			goto out_err;
		}

		buf_count++;

		ret = i915_check_presumed(&arg, item->buffer,
					  (uint32_t __user *)
					  (unsigned long) data,
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

	if (ret)
		drm_putback_buffer_objects(dev);

	if (ret != -EAGAIN) {
		for (i = 0; i < num_buffers; ++i) {
			arg.handled = 1;
			arg.d.rep.ret = buffers->ret;
			arg.d.rep.bo_info = buffers->rep;
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
	struct drm_device *dev = file_priv->head->dev;
	int ret;
	struct drm_fence_object *fence;

	ret = drm_fence_buffer_objects(dev, NULL, fence_flags,
			 NULL, &fence);

	if (ret) {

		/*
		 * Fence creation failed.
		 * Fall back to synchronous operation and idle the engine.
		 */

		(void) i915_emit_mi_flush(dev, MI_READ_FLUSH);
		(void) i915_quiescent(dev);

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

			(void) drm_fence_object_wait(fence, 0, 1,
						     fence->type);
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


static int i915_execbuffer(struct drm_device *dev, void *data,
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
	struct drm_i915_validate_buffer *buffers;

	if (!dev_priv->allow_batchbuffer) {
		DRM_ERROR("Batchbuffer ioctl disabled\n");
		return -EINVAL;
	}


	if (batch->num_cliprects && DRM_VERIFYAREA_READ(batch->cliprects,
							batch->num_cliprects *
							sizeof(struct drm_clip_rect)))
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

	buffers = drm_calloc(num_buffers, sizeof(struct drm_i915_validate_buffer), DRM_MEM_DRIVER);
	if (!buffers) {
		drm_bo_read_unlock(&dev->bm.bm_lock);
		mutex_unlock(&dev_priv->cmdbuf_mutex);
		return -ENOMEM;
	}

	/* validate buffer list + fixup relocations */
	ret = i915_validate_buffer_list(file_priv, 0, exec_buf->ops_list,
					buffers, &num_buffers);
	if (ret)
		goto out_err0;

	/* make sure all previous memory operations have passed */
	DRM_MEMORYBARRIER();
	drm_agp_chipset_flush(dev);

	/* submit buffer */
	batch->start = buffers[num_buffers-1].buffer->offset;

	DRM_DEBUG("i915 exec batchbuffer, start %x used %d cliprects %d\n",
		  batch->start, batch->used, batch->num_cliprects);

	ret = i915_dispatch_batchbuffer(dev, batch);
	if (ret)
		goto out_err0;

	if (sarea_priv)
		sarea_priv->last_dispatch = READ_BREADCRUMB(dev_priv);

	i915_fence_or_sync(file_priv, fence_arg->flags, fence_arg, NULL);

out_err0:

	/* handle errors */
	ret = i915_handle_copyback(dev, buffers, num_buffers, ret);
	mutex_lock(&dev->struct_mutex);
	i915_dereference_buffers_locked(buffers, num_buffers);
	mutex_unlock(&dev->struct_mutex);

	drm_free(buffers, (exec_buf->num_buffers * sizeof(struct drm_buffer_object *)), DRM_MEM_DRIVER);

	mutex_unlock(&dev_priv->cmdbuf_mutex);
	drm_bo_read_unlock(&dev->bm.bm_lock);
	return ret;
}
#endif
