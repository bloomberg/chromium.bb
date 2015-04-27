/*
 * Copyright 2014 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <sys/ioctl.h>

#include "xf86drm.h"
#include "amdgpu_drm.h"
#include "amdgpu_internal.h"

/**
 * Create an IB buffer.
 *
 * \param   dev - \c [in] Device handle
 * \param   context - \c [in] GPU Context
 * \param   ib_size - \c [in] Size of allocation
 * \param   ib - \c [out] return the pointer to the created IB buffer
 *
 * \return  0 on success otherwise POSIX Error code
*/
static int amdgpu_cs_create_ib(amdgpu_device_handle dev,
			       amdgpu_context_handle context,
			       enum amdgpu_cs_ib_size ib_size,
			       amdgpu_ib_handle *ib)
{
	struct amdgpu_bo_alloc_request alloc_buffer;
	struct amdgpu_bo_alloc_result info;
	int r;
	void *cpu;
	struct amdgpu_ib *new_ib;

	memset(&alloc_buffer, 0, sizeof(alloc_buffer));

	switch (ib_size) {
	case amdgpu_cs_ib_size_4K:
		alloc_buffer.alloc_size = 4 * 1024;
		break;
	case amdgpu_cs_ib_size_16K:
		alloc_buffer.alloc_size = 16 * 1024;
		break;
	case amdgpu_cs_ib_size_32K:
		alloc_buffer.alloc_size = 32 * 1024;
		break;
	case amdgpu_cs_ib_size_64K:
		alloc_buffer.alloc_size = 64 * 1024;
		break;
	case amdgpu_cs_ib_size_128K:
		alloc_buffer.alloc_size = 128 * 1024;
		break;
	default:
		return -EINVAL;
	}

	alloc_buffer.phys_alignment = 4 * 1024;

	alloc_buffer.preferred_heap = AMDGPU_GEM_DOMAIN_GTT;

	r = amdgpu_bo_alloc(dev,
			    &alloc_buffer,
			    &info);
	if (r)
		return r;

	r = amdgpu_bo_cpu_map(info.buf_handle, &cpu);
	if (r) {
		amdgpu_bo_free(info.buf_handle);
		return r;
	}

	new_ib = malloc(sizeof(struct amdgpu_ib));
	if (NULL == new_ib) {
		amdgpu_bo_cpu_unmap(info.buf_handle);
		amdgpu_bo_free(info.buf_handle);
		return -ENOMEM;
	}

	new_ib->buf_handle = info.buf_handle;
	new_ib->cpu = cpu;
	new_ib->virtual_mc_base_address = info.virtual_mc_base_address;
	new_ib->ib_size = ib_size;
	*ib = new_ib;
	return 0;
}

/**
 * Destroy an IB buffer.
 *
 * \param   dev - \c [in]  Device handle
 * \param   ib - \c [in] the IB buffer
 *
 * \return  0 on success otherwise POSIX Error code
*/
static int amdgpu_cs_destroy_ib(amdgpu_device_handle dev,
				amdgpu_ib_handle ib)
{
	int r;
	r = amdgpu_bo_cpu_unmap(ib->buf_handle);
	if (r)
		return r;

	r = amdgpu_bo_free(ib->buf_handle);
	if (r)
		return r;

	free(ib);
	return 0;
}

/**
 * Initialize IB pools to empty.
 *
 * \param   context - \c [in]  GPU Context
 *
 * \return  0 on success otherwise POSIX Error code
*/
static int amdgpu_cs_init_ib_pool(amdgpu_context_handle context)
{
	int i;
	int r;

	r = pthread_mutex_init(&context->pool_mutex, NULL);
	if (r)
		return r;

	for (i = 0; i < AMDGPU_CS_IB_SIZE_NUM; i++)
		LIST_INITHEAD(&context->ib_pools[i]);

	return 0;
}

/**
 * Allocate an IB buffer from IB pools.
 *
 * \param   dev - \c [in]  Device handle
 * \param   context - \c [in] GPU Context
 * \param   ib_size - \c [in]  Size of allocation
 * \param   ib - \c [out] return the pointer to the allocated IB buffer
 *
 * \return  0 on success otherwise POSIX Error code
*/
static int amdgpu_cs_alloc_from_ib_pool(amdgpu_device_handle dev,
					amdgpu_context_handle context,
					enum amdgpu_cs_ib_size ib_size,
					amdgpu_ib_handle *ib)
{
	int r;
	struct list_head *head;
	head = &context->ib_pools[ib_size];

	r = -ENOMEM;
	pthread_mutex_lock(&context->pool_mutex);
	if (!LIST_IS_EMPTY(head)) {
		*ib = LIST_ENTRY(struct amdgpu_ib, head->next, list_node);
		LIST_DEL(&(*ib)->list_node);
		r = 0;
	}
	pthread_mutex_unlock(&context->pool_mutex);

	return r;
}

/**
 * Free an IB buffer to IB pools.
 *
 * \param   context - \c [in]  GPU Context
 * \param   ib - \c [in] the IB buffer
 *
 * \return  N/A
*/
static void amdgpu_cs_free_to_ib_pool(amdgpu_context_handle context,
				      amdgpu_ib_handle ib)
{
	struct list_head *head;
	head = &context->ib_pools[ib->ib_size];
	pthread_mutex_lock(&context->pool_mutex);
	LIST_ADD(&ib->list_node, head);
	pthread_mutex_unlock(&context->pool_mutex);
	return;
}

/**
 * Destroy all IB buffers in pools
 *
 * \param   dev - \c [in]  Device handle
 * \param   context - \c [in]  GPU Context
 *
 * \return  0 on success otherwise POSIX Error code
*/
static int amdgpu_cs_destroy_ib_pool(amdgpu_device_handle dev,
				     amdgpu_context_handle context)
{
	int i;
	int r;
	struct list_head *head;
	struct amdgpu_ib *next;
	struct amdgpu_ib *storage;

	r = 0;
	pthread_mutex_lock(&context->pool_mutex);
	for (i = 0; i < AMDGPU_CS_IB_SIZE_NUM; i++) {
		head = &context->ib_pools[i];
		LIST_FOR_EACH_ENTRY_SAFE(next, storage, head, list_node) {
			r = amdgpu_cs_destroy_ib(dev, next);
			if (r)
				break;
		}
	}
	pthread_mutex_unlock(&context->pool_mutex);
	pthread_mutex_destroy(&context->pool_mutex);
	return r;
}

/**
 * Initialize pending IB lists
 *
 * \param   context - \c [in]  GPU Context
 *
 * \return  0 on success otherwise POSIX Error code
*/
static int amdgpu_cs_init_pendings(amdgpu_context_handle context)
{
	unsigned ip, inst;
	uint32_t ring;
	int r;

	r = pthread_mutex_init(&context->pendings_mutex, NULL);
	if (r)
		return r;

	for (ip = 0; ip < AMDGPU_HW_IP_NUM; ip++)
		for (inst = 0; inst < AMDGPU_HW_IP_INSTANCE_MAX_COUNT; inst++)
			for (ring = 0; ring < AMDGPU_CS_MAX_RINGS; ring++)
				LIST_INITHEAD(&context->pendings[ip][inst][ring]);

	LIST_INITHEAD(&context->freed);
	return 0;
}

/**
 * Free pending IBs
 *
 * \param   dev - \c [in]  Device handle
 * \param   context - \c [in]  GPU Context
 *
 * \return  0 on success otherwise POSIX Error code
*/
static int amdgpu_cs_destroy_pendings(amdgpu_device_handle dev,
				      amdgpu_context_handle context)
{
	int ip, inst;
	uint32_t ring;
	int r;
	struct amdgpu_ib *next;
	struct amdgpu_ib *s;
	struct list_head *head;

	r = 0;
	pthread_mutex_lock(&context->pendings_mutex);
	for (ip = 0; ip < AMDGPU_HW_IP_NUM; ip++)
		for (inst = 0; inst < AMDGPU_HW_IP_INSTANCE_MAX_COUNT; inst++)
			for (ring = 0; ring < AMDGPU_CS_MAX_RINGS; ring++) {
				head = &context->pendings[ip][inst][ring];
				LIST_FOR_EACH_ENTRY_SAFE(next, s, head, list_node) {
					r = amdgpu_cs_destroy_ib(dev, next);
					if (r)
						break;
				}
			}

	head = &context->freed;
	LIST_FOR_EACH_ENTRY_SAFE(next, s, head, list_node) {
		r = amdgpu_cs_destroy_ib(dev, next);
		if (r)
			break;
	}

	pthread_mutex_unlock(&context->pendings_mutex);
	pthread_mutex_destroy(&context->pendings_mutex);
	return r;
}

/**
 * Add IB to pending IB lists without holding sequence_mutex.
 *
 * \param   context - \c [in]  GPU Context
 * \param   ib - \c [in]  ib to added to pending lists
 * \param   ip - \c [in]  hw ip block
 * \param   ip_instance - \c [in]  instance of the hw ip block
 * \param   ring - \c [in]  Ring of hw ip
 *
 * \return  N/A
*/
static void amdgpu_cs_add_pending(amdgpu_context_handle context,
				  amdgpu_ib_handle ib,
				  unsigned ip, unsigned ip_instance,
				  uint32_t ring)
{
	struct list_head *head;
	pthread_mutex_lock(&context->pendings_mutex);
	head = &context->pendings[ip][ip_instance][ring];
	LIST_ADDTAIL(&ib->list_node, head);
	pthread_mutex_unlock(&context->pendings_mutex);
	return;
}

/**
 * Garbage collector on a pending IB list without holding pendings_mutex.
 * This function by itself is not multithread safe.
 *
 * \param   context - \c [in]  GPU Context
 * \param   ip - \c [in]  hw ip block
 * \param   ip_instance - \c [in]  instance of the hw ip block
 * \param   ring - \c [in]  Ring of hw ip
 * \param   expired_fence - \c [in]  fence expired
 *
 * \return  N/A
 * \note Hold pendings_mutex before calling this function.
*/
static void amdgpu_cs_pending_gc_not_safe(amdgpu_context_handle context,
					  unsigned ip, unsigned ip_instance,
					  uint32_t ring,
					  uint64_t expired_fence)
{
	struct list_head *head;
	struct amdgpu_ib *next;
	struct amdgpu_ib *s;
	int r;

	head = &context->pendings[ip][ip_instance][ring];
	LIST_FOR_EACH_ENTRY_SAFE(next, s, head, list_node)
		if (next->cs_handle <= expired_fence) {
			LIST_DEL(&next->list_node);
			amdgpu_cs_free_to_ib_pool(context, next);
		} else {
			/* The pending list is a sorted list.
			   There is no need to continue. */
			break;
		}

	/* walk the freed list as well */
	head = &context->freed;
	LIST_FOR_EACH_ENTRY_SAFE(next, s, head, list_node) {
		bool busy;

		r = amdgpu_bo_wait_for_idle(next->buf_handle, 0, &busy);
		if (r || busy)
			break;

		LIST_DEL(&next->list_node);
		amdgpu_cs_free_to_ib_pool(context, next);
	}

	return;
}

/**
 * Garbage collector on a pending IB list
 *
 * \param   context - \c [in]  GPU Context
 * \param   ip - \c [in]  hw ip block
 * \param   ip_instance - \c [in]  instance of the hw ip block
 * \param   ring - \c [in]  Ring of hw ip
 * \param   expired_fence - \c [in]  fence expired
 *
 * \return  N/A
*/
static void amdgpu_cs_pending_gc(amdgpu_context_handle context,
				 unsigned ip, unsigned ip_instance,
				 uint32_t ring,
				 uint64_t expired_fence)
{
	pthread_mutex_lock(&context->pendings_mutex);
	amdgpu_cs_pending_gc_not_safe(context, ip, ip_instance, ring,
				      expired_fence);
	pthread_mutex_unlock(&context->pendings_mutex);
	return;
}

/**
 * Garbage collector on all pending IB lists
 *
 * \param   context - \c [in]  GPU Context
 *
 * \return  N/A
*/
static void amdgpu_cs_all_pending_gc(amdgpu_context_handle context)
{
	unsigned ip, inst;
	uint32_t ring;
	uint64_t expired_fences[AMDGPU_HW_IP_NUM][AMDGPU_HW_IP_INSTANCE_MAX_COUNT][AMDGPU_CS_MAX_RINGS];

	pthread_mutex_lock(&context->sequence_mutex);
	for (ip = 0; ip < AMDGPU_HW_IP_NUM; ip++)
		for (inst = 0; inst < AMDGPU_HW_IP_INSTANCE_MAX_COUNT; inst++)
			for (ring = 0; ring < AMDGPU_CS_MAX_RINGS; ring++)
				expired_fences[ip][inst][ring] =
					context->expired_fences[ip][inst][ring];
	pthread_mutex_unlock(&context->sequence_mutex);

	pthread_mutex_lock(&context->pendings_mutex);
	for (ip = 0; ip < AMDGPU_HW_IP_NUM; ip++)
		for (inst = 0; inst < AMDGPU_HW_IP_INSTANCE_MAX_COUNT; inst++)
			for (ring = 0; ring < AMDGPU_CS_MAX_RINGS; ring++)
				amdgpu_cs_pending_gc_not_safe(context, ip, inst, ring,
					expired_fences[ip][inst][ring]);
	pthread_mutex_unlock(&context->pendings_mutex);
}

/**
 * Allocate an IB buffer
 * If there is no free IB buffer in pools, create one.
 *
 * \param   dev - \c [in] Device handle
 * \param   context - \c [in] GPU Context
 * \param   ib_size - \c [in] Size of allocation
 * \param   ib - \c [out] return the pointer to the allocated IB buffer
 *
 * \return  0 on success otherwise POSIX Error code
*/
static int amdgpu_cs_alloc_ib_local(amdgpu_device_handle dev,
				    amdgpu_context_handle context,
				    enum amdgpu_cs_ib_size ib_size,
				    amdgpu_ib_handle *ib)
{
	int r;

	r = amdgpu_cs_alloc_from_ib_pool(dev, context, ib_size, ib);
	if (!r)
		return r;

	amdgpu_cs_all_pending_gc(context);

	/* Retry to allocate from free IB pools after garbage collector. */
	r = amdgpu_cs_alloc_from_ib_pool(dev, context, ib_size, ib);
	if (!r)
		return r;

	/* There is no suitable IB in free pools. Create one. */
	r = amdgpu_cs_create_ib(dev, context, ib_size, ib);
	return r;
}

int amdgpu_cs_alloc_ib(amdgpu_device_handle dev,
		       amdgpu_context_handle context,
		       enum amdgpu_cs_ib_size ib_size,
		       struct amdgpu_cs_ib_alloc_result *output)
{
	int r;
	amdgpu_ib_handle ib;

	if (NULL == dev)
		return -EINVAL;
	if (NULL == context)
		return -EINVAL;
	if (NULL == output)
		return -EINVAL;
	if (ib_size >= AMDGPU_CS_IB_SIZE_NUM)
		return -EINVAL;

	r = amdgpu_cs_alloc_ib_local(dev, context, ib_size, &ib);
	if (!r) {
		output->handle = ib;
		output->cpu = ib->cpu;
		output->mc_address = ib->virtual_mc_base_address;
	}

	return r;
}

int amdgpu_cs_free_ib(amdgpu_device_handle dev,
		      amdgpu_context_handle context,
		      amdgpu_ib_handle handle)
{
	if (NULL == dev)
		return -EINVAL;
	if (NULL == context)
		return -EINVAL;
	if (NULL == handle)
		return -EINVAL;

	pthread_mutex_lock(&context->pendings_mutex);
	LIST_ADD(&handle->list_node, &context->freed);
	pthread_mutex_unlock(&context->pendings_mutex);
	return 0;
}

/**
 * Create command submission context
 *
 * \param   dev - \c [in] amdgpu device handle
 * \param   context - \c [out] amdgpu context handle
 *
 * \return  0 on success otherwise POSIX Error code
*/
int amdgpu_cs_ctx_create(amdgpu_device_handle dev,
			 amdgpu_context_handle *context)
{
	struct amdgpu_context *gpu_context;
	union drm_amdgpu_ctx args;
	int r;

	if (NULL == dev)
		return -EINVAL;
	if (NULL == context)
		return -EINVAL;

	gpu_context = calloc(1, sizeof(struct amdgpu_context));
	if (NULL == gpu_context)
		return -ENOMEM;

	r = pthread_mutex_init(&gpu_context->sequence_mutex, NULL);
	if (r)
		goto error_mutex;

	r = amdgpu_cs_init_ib_pool(gpu_context);
	if (r)
		goto error_pool;

	r = amdgpu_cs_init_pendings(gpu_context);
	if (r)
		goto error_pendings;

	r = amdgpu_cs_alloc_ib_local(dev, gpu_context, amdgpu_cs_ib_size_4K,
				     &gpu_context->fence_ib);
	if (r)
		goto error_fence_ib;


	memset(&args, 0, sizeof(args));
	args.in.op = AMDGPU_CTX_OP_ALLOC_CTX;
	r = drmCommandWriteRead(dev->fd, DRM_AMDGPU_CTX, &args, sizeof(args));
	if (r)
		goto error_kernel;

	gpu_context->id = args.out.alloc.ctx_id;
	*context = (amdgpu_context_handle)gpu_context;

	return 0;

error_kernel:
	amdgpu_cs_free_ib(dev, gpu_context, gpu_context->fence_ib);

error_fence_ib:
	amdgpu_cs_destroy_pendings(dev, gpu_context);

error_pendings:
	amdgpu_cs_destroy_ib_pool(dev, gpu_context);

error_pool:
	pthread_mutex_destroy(&gpu_context->sequence_mutex);

error_mutex:
	free(gpu_context);
	return r;
}

/**
 * Release command submission context
 *
 * \param   dev - \c [in] amdgpu device handle
 * \param   context - \c [in] amdgpu context handle
 *
 * \return  0 on success otherwise POSIX Error code
*/
int amdgpu_cs_ctx_free(amdgpu_device_handle dev,
		       amdgpu_context_handle context)
{
	int r;
	union drm_amdgpu_ctx args;

	if (NULL == dev)
		return -EINVAL;
	if (NULL == context)
		return -EINVAL;

	r = amdgpu_cs_free_ib(dev, context, context->fence_ib);
	if (r)
		return r;

	r = amdgpu_cs_destroy_pendings(dev, context);
	if (r)
		return r;

	r = amdgpu_cs_destroy_ib_pool(dev, context);
	if (r)
		return r;

	pthread_mutex_destroy(&context->sequence_mutex);

	/* now deal with kernel side */
	memset(&args, 0, sizeof(args));
	args.in.op = AMDGPU_CTX_OP_FREE_CTX;
	args.in.ctx_id = context->id;
	r = drmCommandWriteRead(dev->fd, DRM_AMDGPU_CTX, &args, sizeof(args));

	free(context);

	return r;
}

static int amdgpu_cs_create_bo_list(amdgpu_device_handle dev,
				    amdgpu_context_handle context,
				    struct amdgpu_cs_request *request,
				    amdgpu_ib_handle fence_ib,
				    uint32_t *handle)
{
	struct drm_amdgpu_bo_list_entry *list;
	union drm_amdgpu_bo_list args;
	unsigned num_resources;
	unsigned i;
	int r;

	num_resources = request->number_of_resources;
	if (fence_ib)
		++num_resources;

	list = alloca(sizeof(struct drm_amdgpu_bo_list_entry) * num_resources);

	memset(&args, 0, sizeof(args));
	args.in.operation = AMDGPU_BO_LIST_OP_CREATE;
	args.in.bo_number = num_resources;
	args.in.bo_info_size = sizeof(struct drm_amdgpu_bo_list_entry);
	args.in.bo_info_ptr = (uint64_t)(uintptr_t)list;

	for (i = 0; i < request->number_of_resources; i++) {
		list[i].bo_handle = request->resources[i]->handle;
		if (request->resource_flags)
			list[i].bo_priority = request->resource_flags[i];
		else
			list[i].bo_priority = 0;
	}

	if (fence_ib)
		list[i].bo_handle = fence_ib->buf_handle->handle;

	r = drmCommandWriteRead(dev->fd, DRM_AMDGPU_BO_LIST,
				&args, sizeof(args));
	if (r)
		return r;

	*handle = args.out.list_handle;
	return 0;
}

static int amdgpu_cs_free_bo_list(amdgpu_device_handle dev, uint32_t handle)
{
	union drm_amdgpu_bo_list args;
	int r;

	memset(&args, 0, sizeof(args));
	args.in.operation = AMDGPU_BO_LIST_OP_DESTROY;
	args.in.list_handle = handle;

	r = drmCommandWriteRead(dev->fd, DRM_AMDGPU_BO_LIST,
				&args, sizeof(args));

	return r;
}

static uint32_t amdgpu_cs_fence_index(unsigned ip, unsigned ring)
{
	return ip * AMDGPU_CS_MAX_RINGS + ring;
}

/**
 * Submit command to kernel DRM
 * \param   dev - \c [in]  Device handle
 * \param   context - \c [in]  GPU Context
 * \param   ibs_request - \c [in]  Pointer to submission requests
 * \param   fence - \c [out] return fence for this submission
 *
 * \return  0 on success otherwise POSIX Error code
 * \sa amdgpu_cs_submit()
*/
static int amdgpu_cs_submit_one(amdgpu_device_handle dev,
				amdgpu_context_handle context,
				struct amdgpu_cs_request *ibs_request,
				uint64_t *fence)
{
	int r;
	uint32_t i, size;
	union drm_amdgpu_cs cs;
	uint64_t *chunk_array;
	struct drm_amdgpu_cs_chunk *chunks;
	struct drm_amdgpu_cs_chunk_data *chunk_data;
	uint32_t bo_list_handle;

	if (ibs_request->ip_type >= AMDGPU_HW_IP_NUM)
		return -EINVAL;
	if (ibs_request->ring >= AMDGPU_CS_MAX_RINGS)
		return -EINVAL;
	if (ibs_request->number_of_ibs > AMDGPU_CS_MAX_IBS_PER_SUBMIT)
		return -EINVAL;

	size = (ibs_request->number_of_ibs + 1) * ((sizeof(uint64_t) +
		sizeof(struct drm_amdgpu_cs_chunk) +
		sizeof(struct drm_amdgpu_cs_chunk_data)) +
	       ibs_request->number_of_resources + 1) *
	       sizeof(struct drm_amdgpu_bo_list_entry);
	chunk_array = malloc(size);
	if (NULL == chunk_array)
		return -ENOMEM;
	memset(chunk_array, 0, size);

	chunks = (struct drm_amdgpu_cs_chunk *)(chunk_array + ibs_request->number_of_ibs + 1);
	chunk_data = (struct drm_amdgpu_cs_chunk_data *)(chunks + ibs_request->number_of_ibs + 1);

	memset(&cs, 0, sizeof(cs));
	cs.in.chunks = (uint64_t)(uintptr_t)chunk_array;
	cs.in.ctx_id = context->id;
	cs.in.num_chunks = ibs_request->number_of_ibs;
	/* IB chunks */
	for (i = 0; i < ibs_request->number_of_ibs; i++) {
		struct amdgpu_cs_ib_info *ib;
		chunk_array[i] = (uint64_t)(uintptr_t)&chunks[i];
		chunks[i].chunk_id = AMDGPU_CHUNK_ID_IB;
		chunks[i].length_dw = sizeof(struct drm_amdgpu_cs_chunk_ib) / 4;
		chunks[i].chunk_data = (uint64_t)(uintptr_t)&chunk_data[i];

		ib = &ibs_request->ibs[i];

		chunk_data[i].ib_data.handle = ib->ib_handle->buf_handle->handle;
		chunk_data[i].ib_data.va_start = ib->ib_handle->virtual_mc_base_address;
		chunk_data[i].ib_data.ib_bytes = ib->size * 4;
		chunk_data[i].ib_data.ip_type = ibs_request->ip_type;
		chunk_data[i].ib_data.ip_instance = ibs_request->ip_instance;
		chunk_data[i].ib_data.ring = ibs_request->ring;

		if (ib->flags & AMDGPU_CS_GFX_IB_CE)
			chunk_data[i].ib_data.flags = AMDGPU_IB_FLAG_CE;
	}

	r = amdgpu_cs_create_bo_list(dev, context, ibs_request, NULL,
				     &bo_list_handle);
	if (r)
		goto error_unlock;

	cs.in.bo_list_handle = bo_list_handle;
	pthread_mutex_lock(&context->sequence_mutex);

	if (ibs_request->ip_type != AMDGPU_HW_IP_UVD &&
	    ibs_request->ip_type != AMDGPU_HW_IP_VCE) {
		i = cs.in.num_chunks++;

		/* fence chunk */
		chunk_array[i] = (uint64_t)(uintptr_t)&chunks[i];
		chunks[i].chunk_id = AMDGPU_CHUNK_ID_FENCE;
		chunks[i].length_dw = sizeof(struct drm_amdgpu_cs_chunk_fence) / 4;
		chunks[i].chunk_data = (uint64_t)(uintptr_t)&chunk_data[i];

		/* fence bo handle */
		chunk_data[i].fence_data.handle = context->fence_ib->buf_handle->handle;
		/* offset */
		chunk_data[i].fence_data.offset = amdgpu_cs_fence_index(
			ibs_request->ip_type, ibs_request->ring);
		chunk_data[i].fence_data.offset *= sizeof(uint64_t);
	}

	r = drmCommandWriteRead(dev->fd, DRM_AMDGPU_CS,
				&cs, sizeof(cs));
	if (r)
		goto error_unlock;


	/* Hold sequence_mutex while adding record to the pending list.
	   So the pending list is a sorted list according to fence value. */

	for (i = 0; i < ibs_request->number_of_ibs; i++) {
		struct amdgpu_cs_ib_info *ib;

		ib = &ibs_request->ibs[i];
		if (ib->flags & AMDGPU_CS_REUSE_IB)
			continue;

		ib->ib_handle->cs_handle = cs.out.handle;

		amdgpu_cs_add_pending(context, ib->ib_handle, ibs_request->ip_type,
				      ibs_request->ip_instance,
				      ibs_request->ring);
	}

	*fence = cs.out.handle;

	pthread_mutex_unlock(&context->sequence_mutex);

	r = amdgpu_cs_free_bo_list(dev, bo_list_handle);
	if (r)
		goto error_free;

	free(chunk_array);
	return 0;

error_unlock:
	pthread_mutex_unlock(&context->sequence_mutex);

error_free:
	free(chunk_array);
	return r;
}

int amdgpu_cs_submit(amdgpu_device_handle  dev,
		     amdgpu_context_handle context,
		     uint64_t flags,
		     struct amdgpu_cs_request *ibs_request,
		     uint32_t number_of_requests,
		     uint64_t *fences)
{
	int r;
	uint32_t i;

	if (NULL == dev)
		return -EINVAL;
	if (NULL == context)
		return -EINVAL;
	if (NULL == ibs_request)
		return -EINVAL;
	if (NULL == fences)
		return -EINVAL;

	r = 0;
	for (i = 0; i < number_of_requests; i++) {
		r = amdgpu_cs_submit_one(dev, context, ibs_request, fences);
		if (r)
			break;
		fences++;
		ibs_request++;
	}

	return r;
}

/**
 * Calculate absolute timeout.
 *
 * \param   timeout - \c [in] timeout in nanoseconds.
 *
 * \return  absolute timeout in nanoseconds
*/
uint64_t amdgpu_cs_calculate_timeout(uint64_t timeout)
{
	int r;

	if (timeout != AMDGPU_TIMEOUT_INFINITE) {
		struct timespec current;
		r = clock_gettime(CLOCK_MONOTONIC, &current);
		if (r)
			return r;

		timeout += ((uint64_t)current.tv_sec) * 1000000000ull;
		timeout += current.tv_nsec;
	}
	return timeout;
}

static int amdgpu_ioctl_wait_cs(amdgpu_device_handle dev,
				unsigned ip,
				unsigned ip_instance,
				uint32_t ring,
				uint64_t handle,
				uint64_t timeout_ns,
				bool *busy)
{
	union drm_amdgpu_wait_cs args;
	int r;

	memset(&args, 0, sizeof(args));
	args.in.handle = handle;
	args.in.ip_type = ip;
	args.in.ip_instance = ip_instance;
	args.in.ring = ring;
	args.in.timeout = amdgpu_cs_calculate_timeout(timeout_ns);

	/* Handle errors manually here because of timeout */
	r = ioctl(dev->fd, DRM_IOCTL_AMDGPU_WAIT_CS, &args);
	if (r == -1 && (errno == EINTR || errno == EAGAIN)) {
		*busy = true;
		return 0;
	} else if (r)
		return -errno;

	*busy = args.out.status;
	return 0;
}

int amdgpu_cs_query_fence_status(amdgpu_device_handle dev,
				 struct amdgpu_cs_query_fence *fence,
				 uint32_t *expired)
{
	amdgpu_context_handle context;
	uint64_t *signaled_fence;
	uint64_t *expired_fence;
	unsigned ip_type, ip_instance;
	uint32_t ring;
	bool busy = true;
	int r;

	if (NULL == dev)
		return -EINVAL;
	if (NULL == fence)
		return -EINVAL;
	if (NULL == expired)
		return -EINVAL;
	if (NULL == fence->context)
		return -EINVAL;
	if (fence->ip_type >= AMDGPU_HW_IP_NUM)
		return -EINVAL;
	if (fence->ring >= AMDGPU_CS_MAX_RINGS)
		return -EINVAL;

	context = fence->context;
	ip_type = fence->ip_type;
	ip_instance = fence->ip_instance;
	ring = fence->ring;
	signaled_fence = context->fence_ib->cpu;
	signaled_fence += amdgpu_cs_fence_index(ip_type, ring);
	expired_fence = &context->expired_fences[ip_type][ip_instance][ring];
	*expired = false;

	pthread_mutex_lock(&context->sequence_mutex);
	if (fence->fence <= *expired_fence) {
		/* This fence value is expired already. */
		pthread_mutex_unlock(&context->sequence_mutex);
		*expired = true;
		return 0;
	}

	if (fence->fence <= *signaled_fence) {
		/* This fence value is signaled already. */
		*expired_fence = *signaled_fence;
		pthread_mutex_unlock(&context->sequence_mutex);
		amdgpu_cs_pending_gc(context, ip_type, ip_instance, ring,
				     fence->fence);
		*expired = true;
		return 0;
	}

	pthread_mutex_unlock(&context->sequence_mutex);

	r = amdgpu_ioctl_wait_cs(dev, ip_type, ip_instance, ring,
				 fence->fence, fence->timeout_ns, &busy);
	if (!r && !busy) {
		*expired = true;
		pthread_mutex_lock(&context->sequence_mutex);
		/* The thread doesn't hold sequence_mutex. Other thread could
		   update *expired_fence already. Check whether there is a
		   newerly expired fence. */
		if (fence->fence > *expired_fence) {
			*expired_fence = fence->fence;
			pthread_mutex_unlock(&context->sequence_mutex);
			amdgpu_cs_pending_gc(context, ip_type, ip_instance,
					     ring, fence->fence);
		} else {
			pthread_mutex_unlock(&context->sequence_mutex);
		}
	}

	return r;
}

