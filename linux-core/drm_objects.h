/**************************************************************************
 *
 * Copyright (c) 2006-2007 Tungsten Graphics, Inc., Cedar Park, TX., USA
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
/*
 * Authors: Thomas Hellström <thomas-at-tungstengraphics-dot-com>
 */

#ifndef _DRM_OBJECTS_H
#define _DRM_OBJECTS_H

struct drm_device;
struct drm_bo_mem_reg;

/***************************************************
 * User space objects. (drm_object.c)
 */

#define drm_user_object_entry(_ptr, _type, _member) container_of(_ptr, _type, _member)

enum drm_object_type {
	drm_fence_type,
	drm_buffer_type,
	drm_lock_type,
	    /*
	     * Add other user space object types here.
	     */
	drm_driver_type0 = 256,
	drm_driver_type1,
	drm_driver_type2,
	drm_driver_type3,
	drm_driver_type4
};

/*
 * A user object is a structure that helps the drm give out user handles
 * to kernel internal objects and to keep track of these objects so that
 * they can be destroyed, for example when the user space process exits.
 * Designed to be accessible using a user space 32-bit handle.
 */

struct drm_user_object {
	struct drm_hash_item hash;
	struct list_head list;
	enum drm_object_type type;
	atomic_t refcount;
	int shareable;
	struct drm_file *owner;
	void (*ref_struct_locked) (struct drm_file *priv,
				   struct drm_user_object *obj,
				   enum drm_ref_type ref_action);
	void (*unref) (struct drm_file *priv, struct drm_user_object *obj,
		       enum drm_ref_type unref_action);
	void (*remove) (struct drm_file *priv, struct drm_user_object *obj);
};

/*
 * A ref object is a structure which is used to
 * keep track of references to user objects and to keep track of these
 * references so that they can be destroyed for example when the user space
 * process exits. Designed to be accessible using a pointer to the _user_ object.
 */

struct drm_ref_object {
	struct drm_hash_item hash;
	struct list_head list;
	atomic_t refcount;
	enum drm_ref_type unref_action;
};

/**
 * Must be called with the struct_mutex held.
 */

extern int drm_add_user_object(struct drm_file *priv, struct drm_user_object *item,
			       int shareable);
/**
 * Must be called with the struct_mutex held.
 */

extern struct drm_user_object *drm_lookup_user_object(struct drm_file *priv,
						 uint32_t key);

/*
 * Must be called with the struct_mutex held. May temporarily release it.
 */

extern int drm_add_ref_object(struct drm_file *priv,
			      struct drm_user_object *referenced_object,
			      enum drm_ref_type ref_action);

/*
 * Must be called with the struct_mutex held.
 */

struct drm_ref_object *drm_lookup_ref_object(struct drm_file *priv,
					struct drm_user_object *referenced_object,
					enum drm_ref_type ref_action);
/*
 * Must be called with the struct_mutex held.
 * If "item" has been obtained by a call to drm_lookup_ref_object. You may not
 * release the struct_mutex before calling drm_remove_ref_object.
 * This function may temporarily release the struct_mutex.
 */

extern void drm_remove_ref_object(struct drm_file *priv, struct drm_ref_object *item);
extern int drm_user_object_ref(struct drm_file *priv, uint32_t user_token,
			       enum drm_object_type type,
			       struct drm_user_object **object);
extern int drm_user_object_unref(struct drm_file *priv, uint32_t user_token,
				 enum drm_object_type type);

/***************************************************
 * Fence objects. (drm_fence.c)
 */

struct drm_fence_object {
	struct drm_user_object base;
	struct drm_device *dev;
	atomic_t usage;

	/*
	 * The below three fields are protected by the fence manager spinlock.
	 */

	struct list_head ring;
	int fence_class;
	uint32_t native_types;
	uint32_t type;
	uint32_t signaled_types;
	uint32_t sequence;
	uint32_t waiting_types;
	uint32_t error;
};

#define _DRM_FENCE_CLASSES 8
#define _DRM_FENCE_TYPE_EXE 0x00

struct drm_fence_class_manager {
	struct list_head ring;
	uint32_t pending_flush;
	uint32_t waiting_types;
	wait_queue_head_t fence_queue;
	uint32_t highest_waiting_sequence;
        uint32_t latest_queued_sequence;
};

struct drm_fence_manager {
	int initialized;
	rwlock_t lock;
	struct drm_fence_class_manager fence_class[_DRM_FENCE_CLASSES];
	uint32_t num_classes;
	atomic_t count;
};

struct drm_fence_driver {
	unsigned long *waiting_jiffies;
	uint32_t num_classes;
	uint32_t wrap_diff;
	uint32_t flush_diff;
	uint32_t sequence_mask;

	/*
	 * Driver implemented functions:
	 * has_irq() : 1 if the hardware can update the indicated type_flags using an
	 * irq handler. 0 if polling is required.
	 *
	 * emit() : Emit a sequence number to the command stream.
	 * Return the sequence number.
	 *
	 * flush() : Make sure the flags indicated in fc->pending_flush will eventually
	 * signal for fc->highest_received_sequence and all preceding sequences.
	 * Acknowledge by clearing the flags fc->pending_flush.
	 *
	 * poll() : Call drm_fence_handler with any new information.
	 *
	 * needed_flush() : Given the current state of the fence->type flags and previusly 
	 * executed or queued flushes, return the type_flags that need flushing.
	 *
	 * wait(): Wait for the "mask" flags to signal on a given fence, performing
	 * whatever's necessary to make this happen.
	 */

	int (*has_irq) (struct drm_device *dev, uint32_t fence_class,
			uint32_t flags);
	int (*emit) (struct drm_device *dev, uint32_t fence_class,
		     uint32_t flags, uint32_t *breadcrumb,
		     uint32_t *native_type);
	void (*flush) (struct drm_device *dev, uint32_t fence_class);
	void (*poll) (struct drm_device *dev, uint32_t fence_class,
		uint32_t types);
	uint32_t (*needed_flush) (struct drm_fence_object *fence);
	int (*wait) (struct drm_fence_object *fence, int lazy,
		     int interruptible, uint32_t mask);
};

extern int drm_fence_wait_polling(struct drm_fence_object *fence, int lazy,
				  int interruptible, uint32_t mask,
				  unsigned long end_jiffies);
extern void drm_fence_handler(struct drm_device *dev, uint32_t fence_class,
			      uint32_t sequence, uint32_t type,
			      uint32_t error);
extern void drm_fence_manager_init(struct drm_device *dev);
extern void drm_fence_manager_takedown(struct drm_device *dev);
extern void drm_fence_flush_old(struct drm_device *dev, uint32_t fence_class,
				uint32_t sequence);
extern int drm_fence_object_flush(struct drm_fence_object *fence,
				  uint32_t type);
extern int drm_fence_object_signaled(struct drm_fence_object *fence,
				     uint32_t type);
extern void drm_fence_usage_deref_locked(struct drm_fence_object **fence);
extern void drm_fence_usage_deref_unlocked(struct drm_fence_object **fence);
extern struct drm_fence_object *drm_fence_reference_locked(struct drm_fence_object *src);
extern void drm_fence_reference_unlocked(struct drm_fence_object **dst,
					 struct drm_fence_object *src);
extern int drm_fence_object_wait(struct drm_fence_object *fence,
				 int lazy, int ignore_signals, uint32_t mask);
extern int drm_fence_object_create(struct drm_device *dev, uint32_t type,
				   uint32_t fence_flags, uint32_t fence_class,
				   struct drm_fence_object **c_fence);
extern int drm_fence_object_emit(struct drm_fence_object *fence,
				 uint32_t fence_flags, uint32_t class,
				 uint32_t type);
extern void drm_fence_fill_arg(struct drm_fence_object *fence,
			       struct drm_fence_arg *arg);

extern int drm_fence_add_user_object(struct drm_file *priv,
				     struct drm_fence_object *fence,
				     int shareable);

extern int drm_fence_create_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *file_priv);
extern int drm_fence_destroy_ioctl(struct drm_device *dev, void *data,
				   struct drm_file *file_priv);
extern int drm_fence_reference_ioctl(struct drm_device *dev, void *data,
				     struct drm_file *file_priv);
extern int drm_fence_unreference_ioctl(struct drm_device *dev, void *data,
				       struct drm_file *file_priv);
extern int drm_fence_signaled_ioctl(struct drm_device *dev, void *data,
				    struct drm_file *file_priv);
extern int drm_fence_flush_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
extern int drm_fence_wait_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv);
extern int drm_fence_emit_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv);
extern int drm_fence_buffers_ioctl(struct drm_device *dev, void *data,
				   struct drm_file *file_priv);
/**************************************************
 *TTMs
 */

/*
 * The ttm backend GTT interface. (In our case AGP).
 * Any similar type of device (PCIE?)
 * needs only to implement these functions to be usable with the TTM interface.
 * The AGP backend implementation lives in drm_agpsupport.c
 * basically maps these calls to available functions in agpgart.
 * Each drm device driver gets an
 * additional function pointer that creates these types,
 * so that the device can choose the correct aperture.
 * (Multiple AGP apertures, etc.)
 * Most device drivers will let this point to the standard AGP implementation.
 */

#define DRM_BE_FLAG_NEEDS_FREE     0x00000001
#define DRM_BE_FLAG_BOUND_CACHED   0x00000002

struct drm_ttm_backend;
struct drm_ttm_backend_func {
	int (*needs_ub_cache_adjust) (struct drm_ttm_backend *backend);
	int (*populate) (struct drm_ttm_backend *backend,
			 unsigned long num_pages, struct page **pages,
			 struct page *dummy_read_page);
	void (*clear) (struct drm_ttm_backend *backend);
	int (*bind) (struct drm_ttm_backend *backend,
		     struct drm_bo_mem_reg *bo_mem);
	int (*unbind) (struct drm_ttm_backend *backend);
	void (*destroy) (struct drm_ttm_backend *backend);
};


struct drm_ttm_backend {
	struct drm_device *dev;
	uint32_t flags;
	struct drm_ttm_backend_func *func;
};

struct drm_ttm {
	struct page *dummy_read_page;
	struct page **pages;
	uint32_t page_flags;
	unsigned long num_pages;
	atomic_t vma_count;
	struct drm_device *dev;
	int destroy;
	uint32_t mapping_offset;
	struct drm_ttm_backend *be;
	enum {
		ttm_bound,
		ttm_evicted,
		ttm_unbound,
		ttm_unpopulated,
	} state;

};

extern struct drm_ttm *drm_ttm_create(struct drm_device *dev, unsigned long size,
				      uint32_t page_flags,
				      struct page *dummy_read_page);
extern int drm_ttm_bind(struct drm_ttm *ttm, struct drm_bo_mem_reg *bo_mem);
extern void drm_ttm_unbind(struct drm_ttm *ttm);
extern void drm_ttm_evict(struct drm_ttm *ttm);
extern void drm_ttm_fixup_caching(struct drm_ttm *ttm);
extern struct page *drm_ttm_get_page(struct drm_ttm *ttm, int index);
extern void drm_ttm_cache_flush(void);
extern int drm_ttm_populate(struct drm_ttm *ttm);
extern int drm_ttm_set_user(struct drm_ttm *ttm,
			    struct task_struct *tsk,
			    unsigned long start,
			    unsigned long num_pages);

/*
 * Destroy a ttm. The user normally calls drmRmMap or a similar IOCTL to do
 * this which calls this function iff there are no vmas referencing it anymore.
 * Otherwise it is called when the last vma exits.
 */

extern int drm_ttm_destroy(struct drm_ttm *ttm);

#define DRM_FLAG_MASKED(_old, _new, _mask) {\
(_old) ^= (((_old) ^ (_new)) & (_mask)); \
}

#define DRM_TTM_MASK_FLAGS ((1 << PAGE_SHIFT) - 1)
#define DRM_TTM_MASK_PFN (0xFFFFFFFFU - DRM_TTM_MASK_FLAGS)

/*
 * Page flags.
 */

/*
 * This ttm should not be cached by the CPU
 */
#define DRM_TTM_PAGE_UNCACHED   (1 << 0)
/*
 * This flat is not used at this time; I don't know what the
 * intent was
 */
#define DRM_TTM_PAGE_USED       (1 << 1)
/*
 * This flat is not used at this time; I don't know what the
 * intent was
 */
#define DRM_TTM_PAGE_BOUND      (1 << 2)
/*
 * This flat is not used at this time; I don't know what the
 * intent was
 */
#define DRM_TTM_PAGE_PRESENT    (1 << 3)
/*
 * The array of page pointers was allocated with vmalloc
 * instead of drm_calloc.
 */
#define DRM_TTM_PAGE_VMALLOC    (1 << 4)
/*
 * This ttm is mapped from user space
 */
#define DRM_TTM_PAGE_USER       (1 << 5)
/*
 * This ttm will be written to by the GPU
 */
#define DRM_TTM_PAGE_WRITE	(1 << 6)
/*
 * This ttm was mapped to the GPU, and so the contents may have
 * been modified
 */
#define DRM_TTM_PAGE_USER_DIRTY (1 << 7)
/*
 * This flag is not used at this time; I don't know what the
 * intent was.
 */
#define DRM_TTM_PAGE_USER_DMA   (1 << 8)

/***************************************************
 * Buffer objects. (drm_bo.c, drm_bo_move.c)
 */

struct drm_bo_mem_reg {
	struct drm_mm_node *mm_node;
	unsigned long size;
	unsigned long num_pages;
	uint32_t page_alignment;
	uint32_t mem_type;
	/*
	 * Current buffer status flags, indicating
	 * where the buffer is located and which
	 * access modes are in effect
	 */
	uint64_t flags;
	/**
	 * These are the flags proposed for
	 * a validate operation. If the
	 * validate succeeds, they'll get moved
	 * into the flags field
	 */
	uint64_t proposed_flags;
	
	uint32_t desired_tile_stride;
	uint32_t hw_tile_stride;
};

enum drm_bo_type {
	/*
	 * drm_bo_type_device are 'normal' drm allocations,
	 * pages are allocated from within the kernel automatically
	 * and the objects can be mmap'd from the drm device. Each
	 * drm_bo_type_device object has a unique name which can be
	 * used by other processes to share access to the underlying
	 * buffer.
	 */
	drm_bo_type_device,
	/*
	 * drm_bo_type_user are buffers of pages that already exist
	 * in the process address space. They are more limited than
	 * drm_bo_type_device buffers in that they must always
	 * remain cached (as we assume the user pages are mapped cached),
	 * and they are not sharable to other processes through DRM
	 * (although, regular shared memory should still work fine).
	 */
	drm_bo_type_user,
	/*
	 * drm_bo_type_kernel are buffers that exist solely for use
	 * within the kernel. The pages cannot be mapped into the
	 * process. One obvious use would be for the ring
	 * buffer where user access would not (ideally) be required.
	 */
	drm_bo_type_kernel,
};

struct drm_buffer_object {
	struct drm_device *dev;
	struct drm_user_object base;

	/*
	 * If there is a possibility that the usage variable is zero,
	 * then dev->struct_mutext should be locked before incrementing it.
	 */

	atomic_t usage;
	unsigned long buffer_start;
	enum drm_bo_type type;
	unsigned long offset;
	atomic_t mapped;
	struct drm_bo_mem_reg mem;

	struct list_head lru;
	struct list_head ddestroy;

	uint32_t fence_type;
	uint32_t fence_class;
	uint32_t new_fence_type;
	uint32_t new_fence_class;
	struct drm_fence_object *fence;
	uint32_t priv_flags;
	wait_queue_head_t event_queue;
	struct mutex mutex;
	unsigned long num_pages;

	/* For pinned buffers */
	struct drm_mm_node *pinned_node;
	uint32_t pinned_mem_type;
	struct list_head pinned_lru;

	/* For vm */
	struct drm_ttm *ttm;
	struct drm_map_list map_list;
	uint32_t memory_type;
	unsigned long bus_offset;
	uint32_t vm_flags;
	void *iomap;

#ifdef DRM_ODD_MM_COMPAT
	/* dev->struct_mutex only protected. */
	struct list_head vma_list;
	struct list_head p_mm_list;
#endif

};

#define _DRM_BO_FLAG_UNFENCED 0x00000001
#define _DRM_BO_FLAG_EVICTED  0x00000002

struct drm_mem_type_manager {
	int has_type;
	int use_type;
	struct drm_mm manager;
	struct list_head lru;
	struct list_head pinned;
	uint32_t flags;
	uint32_t drm_bus_maptype;
	unsigned long gpu_offset;
	unsigned long io_offset;
	unsigned long io_size;
	void *io_addr;
};

struct drm_bo_lock {
	struct drm_user_object base;
	wait_queue_head_t queue;
	atomic_t write_lock_pending;
	atomic_t readers;
};

#define _DRM_FLAG_MEMTYPE_FIXED     0x00000001	/* Fixed (on-card) PCI memory */
#define _DRM_FLAG_MEMTYPE_MAPPABLE  0x00000002	/* Memory mappable */
#define _DRM_FLAG_MEMTYPE_CACHED    0x00000004	/* Cached binding */
#define _DRM_FLAG_NEEDS_IOREMAP     0x00000008	/* Fixed memory needs ioremap
						   before kernel access. */
#define _DRM_FLAG_MEMTYPE_CMA       0x00000010	/* Can't map aperture */
#define _DRM_FLAG_MEMTYPE_CSELECT   0x00000020	/* Select caching */

struct drm_buffer_manager {
	struct drm_bo_lock bm_lock;
	struct mutex evict_mutex;
	int nice_mode;
	int initialized;
	struct drm_file *last_to_validate;
	struct drm_mem_type_manager man[DRM_BO_MEM_TYPES];
	struct list_head unfenced;
	struct list_head ddestroy;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
	struct work_struct wq;
#else
	struct delayed_work wq;
#endif
	uint32_t fence_type;
	unsigned long cur_pages;
	atomic_t count;
	struct page *dummy_read_page;
};

struct drm_bo_driver {
	const uint32_t *mem_type_prio;
	const uint32_t *mem_busy_prio;
	uint32_t num_mem_type_prio;
	uint32_t num_mem_busy_prio;
	struct drm_ttm_backend *(*create_ttm_backend_entry)
	 (struct drm_device *dev);
	int (*fence_type) (struct drm_buffer_object *bo, uint32_t *fclass,
			   uint32_t *type);
	int (*invalidate_caches) (struct drm_device *dev, uint64_t flags);
	int (*init_mem_type) (struct drm_device *dev, uint32_t type,
			      struct drm_mem_type_manager *man);
	/*
	 * evict_flags:
	 *
	 * @bo: the buffer object to be evicted
	 *
	 * Return the bo flags for a buffer which is not mapped to the hardware.
	 * These will be placed in proposed_flags so that when the move is
	 * finished, they'll end up in bo->mem.flags
	 */
	uint64_t(*evict_flags) (struct drm_buffer_object *bo);
	/*
	 * move:
	 *
	 * @bo: the buffer to move
	 *
	 * @evict: whether this motion is evicting the buffer from
	 * the graphics address space
	 *
	 * @no_wait: whether this should give up and return -EBUSY
	 * if this move would require sleeping
	 *
	 * @new_mem: the new memory region receiving the buffer
	 *
	 * Move a buffer between two memory regions.
	 */
	int (*move) (struct drm_buffer_object *bo,
		     int evict, int no_wait, struct drm_bo_mem_reg *new_mem);
	/*
	 * ttm_cache_flush
	 */
	void (*ttm_cache_flush)(struct drm_ttm *ttm);

	/*
	 * command_stream_barrier
	 *
	 * @dev: The drm device.
	 *
	 * @bo: The buffer object to validate.
	 *
	 * @new_fence_class: The new fence class for the buffer object.
	 *
	 * @new_fence_type: The new fence type for the buffer object.
	 *
	 * @no_wait: whether this should give up and return -EBUSY
	 * if this operation would require sleeping
	 *
	 * Insert a command stream barrier that makes sure that the
	 * buffer is idle once the commands associated with the
	 * current validation are starting to execute. If an error
	 * condition is returned, or the function pointer is NULL,
	 * the drm core will force buffer idle
	 * during validation.
	 */

	int (*command_stream_barrier) (struct drm_buffer_object *bo,
				       uint32_t new_fence_class,
				       uint32_t new_fence_type,
				       int no_wait);				       
};

/*
 * buffer objects (drm_bo.c)
 */

extern int drm_bo_create_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
extern int drm_bo_destroy_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
extern int drm_bo_map_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
extern int drm_bo_unmap_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
extern int drm_bo_reference_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
extern int drm_bo_unreference_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
extern int drm_bo_wait_idle_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
extern int drm_bo_info_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
extern int drm_bo_setstatus_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
extern int drm_mm_init_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
extern int drm_mm_takedown_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
extern int drm_mm_lock_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
extern int drm_mm_unlock_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
extern int drm_bo_version_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
extern int drm_bo_driver_finish(struct drm_device *dev);
extern int drm_bo_driver_init(struct drm_device *dev);
extern int drm_bo_pci_offset(struct drm_device *dev,
			     struct drm_bo_mem_reg *mem,
			     unsigned long *bus_base,
			     unsigned long *bus_offset,
			     unsigned long *bus_size);
extern int drm_mem_reg_is_pci(struct drm_device *dev, struct drm_bo_mem_reg *mem);

extern void drm_bo_usage_deref_locked(struct drm_buffer_object **bo);
extern void drm_bo_usage_deref_unlocked(struct drm_buffer_object **bo);
extern void drm_putback_buffer_objects(struct drm_device *dev);
extern int drm_fence_buffer_objects(struct drm_device *dev,
				    struct list_head *list,
				    uint32_t fence_flags,
				    struct drm_fence_object *fence,
				    struct drm_fence_object **used_fence);
extern void drm_bo_add_to_lru(struct drm_buffer_object *bo);
extern int drm_buffer_object_create(struct drm_device *dev, unsigned long size,
				    enum drm_bo_type type, uint64_t flags,
				    uint32_t hint, uint32_t page_alignment,
				    unsigned long buffer_start,
				    struct drm_buffer_object **bo);
extern int drm_bo_wait(struct drm_buffer_object *bo, int lazy, int ignore_signals,
		       int no_wait);
extern int drm_bo_mem_space(struct drm_buffer_object *bo,
			    struct drm_bo_mem_reg *mem, int no_wait);
extern int drm_bo_move_buffer(struct drm_buffer_object *bo,
			      uint64_t new_mem_flags,
			      int no_wait, int move_unfenced);
extern int drm_bo_clean_mm(struct drm_device *dev, unsigned mem_type);
extern int drm_bo_init_mm(struct drm_device *dev, unsigned type,
			  unsigned long p_offset, unsigned long p_size);
extern int drm_bo_handle_validate(struct drm_file *file_priv, uint32_t handle,
				  uint64_t flags, uint64_t mask, uint32_t hint,
				  uint32_t fence_class, int use_old_fence_class,
				  struct drm_bo_info_rep *rep,
				  struct drm_buffer_object **bo_rep);
extern struct drm_buffer_object *drm_lookup_buffer_object(struct drm_file *file_priv,
							  uint32_t handle,
							  int check_owner);
extern int drm_bo_do_validate(struct drm_buffer_object *bo,
			      uint64_t flags, uint64_t mask, uint32_t hint,
			      uint32_t fence_class,
			      struct drm_bo_info_rep *rep);

/*
 * Buffer object memory move- and map helpers.
 * drm_bo_move.c
 */

extern int drm_bo_move_ttm(struct drm_buffer_object *bo,
			   int evict, int no_wait,
			   struct drm_bo_mem_reg *new_mem);
extern int drm_bo_move_memcpy(struct drm_buffer_object *bo,
			      int evict,
			      int no_wait, struct drm_bo_mem_reg *new_mem);
extern int drm_bo_move_accel_cleanup(struct drm_buffer_object *bo,
				     int evict, int no_wait,
				     uint32_t fence_class, uint32_t fence_type,
				     uint32_t fence_flags,
				     struct drm_bo_mem_reg *new_mem);
extern int drm_bo_same_page(unsigned long offset, unsigned long offset2);
extern unsigned long drm_bo_offset_end(unsigned long offset,
				       unsigned long end);

struct drm_bo_kmap_obj {
	void *virtual;
	struct page *page;
	enum {
		bo_map_iomap,
		bo_map_vmap,
		bo_map_kmap,
		bo_map_premapped,
	} bo_kmap_type;
};

static inline void *drm_bmo_virtual(struct drm_bo_kmap_obj *map, int *is_iomem)
{
	*is_iomem = (map->bo_kmap_type == bo_map_iomap ||
		     map->bo_kmap_type == bo_map_premapped);
	return map->virtual;
}
extern void drm_bo_kunmap(struct drm_bo_kmap_obj *map);
extern int drm_bo_kmap(struct drm_buffer_object *bo, unsigned long start_page,
		       unsigned long num_pages, struct drm_bo_kmap_obj *map);
extern int drm_bo_pfn_prot(struct drm_buffer_object *bo,
			   unsigned long dst_offset,
			   unsigned long *pfn,
			   pgprot_t *prot);


/*
 * drm_regman.c
 */

struct drm_reg {
	struct list_head head;
	struct drm_fence_object *fence;
	uint32_t fence_type;
	uint32_t new_fence_type;
};

struct drm_reg_manager {
	struct list_head free;
	struct list_head lru;
	struct list_head unfenced;

	int (*reg_reusable)(const struct drm_reg *reg, const void *data);
	void (*reg_destroy)(struct drm_reg *reg);
};

extern int drm_regs_alloc(struct drm_reg_manager *manager,
			  const void *data,
			  uint32_t fence_class,
			  uint32_t fence_type,
			  int interruptible,
			  int no_wait,
			  struct drm_reg **reg);

extern void drm_regs_fence(struct drm_reg_manager *regs,
			   struct drm_fence_object *fence);

extern void drm_regs_free(struct drm_reg_manager *manager);
extern void drm_regs_add(struct drm_reg_manager *manager, struct drm_reg *reg);
extern void drm_regs_init(struct drm_reg_manager *manager,
			  int (*reg_reusable)(const struct drm_reg *,
					      const void *),
			  void (*reg_destroy)(struct drm_reg *));

/*
 * drm_bo_lock.c
 * Simple replacement for the hardware lock on buffer manager init and clean.
 */


extern void drm_bo_init_lock(struct drm_bo_lock *lock);
extern void drm_bo_read_unlock(struct drm_bo_lock *lock);
extern int drm_bo_read_lock(struct drm_bo_lock *lock);
extern int drm_bo_write_lock(struct drm_bo_lock *lock,
			     struct drm_file *file_priv);

extern int drm_bo_write_unlock(struct drm_bo_lock *lock,
			       struct drm_file *file_priv);

#ifdef CONFIG_DEBUG_MUTEXES
#define DRM_ASSERT_LOCKED(_mutex)					\
	BUG_ON(!mutex_is_locked(_mutex) ||				\
	       ((_mutex)->owner != current_thread_info()))
#else
#define DRM_ASSERT_LOCKED(_mutex)
#endif
#endif
