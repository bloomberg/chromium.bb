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

/***************************************************
 * User space objects. (drm_object.c)
 */

#define drm_user_object_entry(_ptr, _type, _member) container_of(_ptr, _type, _member)

enum drm_object_type {
	drm_fence_type,
	drm_buffer_type,
	drm_ttm_type
	    /*
	     * Add other user space object types here.
	     */
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
	void (*ref_struct_locked) (struct drm_file * priv,
				   struct drm_user_object * obj,
				   drm_ref_t ref_action);
	void (*unref) (struct drm_file * priv, struct drm_user_object * obj,
		       drm_ref_t unref_action);
	void (*remove) (struct drm_file * priv, struct drm_user_object * obj);
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
	drm_ref_t unref_action;
};

/**
 * Must be called with the struct_mutex held.
 */

extern int drm_add_user_object(struct drm_file * priv, struct drm_user_object * item,
			       int shareable);
/**
 * Must be called with the struct_mutex held.
 */

extern struct drm_user_object *drm_lookup_user_object(struct drm_file * priv,
						 uint32_t key);

/*
 * Must be called with the struct_mutex held.
 * If "item" has been obtained by a call to drm_lookup_user_object. You may not
 * release the struct_mutex before calling drm_remove_ref_object.
 * This function may temporarily release the struct_mutex.
 */

extern int drm_remove_user_object(struct drm_file * priv, struct drm_user_object * item);

/*
 * Must be called with the struct_mutex held. May temporarily release it.
 */

extern int drm_add_ref_object(struct drm_file * priv,
			      struct drm_user_object * referenced_object,
			      drm_ref_t ref_action);

/*
 * Must be called with the struct_mutex held.
 */

struct drm_ref_object *drm_lookup_ref_object(struct drm_file * priv,
					struct drm_user_object * referenced_object,
					drm_ref_t ref_action);
/*
 * Must be called with the struct_mutex held.
 * If "item" has been obtained by a call to drm_lookup_ref_object. You may not
 * release the struct_mutex before calling drm_remove_ref_object.
 * This function may temporarily release the struct_mutex.
 */

extern void drm_remove_ref_object(struct drm_file * priv, struct drm_ref_object * item);
extern int drm_user_object_ref(struct drm_file * priv, uint32_t user_token,
			       enum drm_object_type type,
			       struct drm_user_object ** object);
extern int drm_user_object_unref(struct drm_file * priv, uint32_t user_token,
				 enum drm_object_type type);

/***************************************************
 * Fence objects. (drm_fence.c)
 */

typedef struct drm_fence_object {
	struct drm_user_object base;
        struct drm_device *dev;
	atomic_t usage;

	/*
	 * The below three fields are protected by the fence manager spinlock.
	 */

	struct list_head ring;
	int class;
	uint32_t native_type;
	uint32_t type;
	uint32_t signaled;
	uint32_t sequence;
	uint32_t flush_mask;
	uint32_t submitted_flush;
} drm_fence_object_t;

#define _DRM_FENCE_CLASSES 8
#define _DRM_FENCE_TYPE_EXE 0x00

typedef struct drm_fence_class_manager {
	struct list_head ring;
	uint32_t pending_flush;
	wait_queue_head_t fence_queue;
	int pending_exe_flush;
	uint32_t last_exe_flush;
	uint32_t exe_flush_sequence;
} drm_fence_class_manager_t;

typedef struct drm_fence_manager {
	int initialized;
	rwlock_t lock;
	drm_fence_class_manager_t class[_DRM_FENCE_CLASSES];
	uint32_t num_classes;
	atomic_t count;
} drm_fence_manager_t;

typedef struct drm_fence_driver {
	uint32_t num_classes;
	uint32_t wrap_diff;
	uint32_t flush_diff;
	uint32_t sequence_mask;
	int lazy_capable;
	int (*has_irq) (struct drm_device * dev, uint32_t class,
			uint32_t flags);
	int (*emit) (struct drm_device * dev, uint32_t class, uint32_t flags,
		     uint32_t * breadcrumb, uint32_t * native_type);
	void (*poke_flush) (struct drm_device * dev, uint32_t class);
} drm_fence_driver_t;

extern void drm_fence_handler(struct drm_device *dev, uint32_t class,
			      uint32_t sequence, uint32_t type);
extern void drm_fence_manager_init(struct drm_device *dev);
extern void drm_fence_manager_takedown(struct drm_device *dev);
extern void drm_fence_flush_old(struct drm_device *dev, uint32_t class,
				uint32_t sequence);
extern int drm_fence_object_flush(drm_fence_object_t * fence, uint32_t type);
extern int drm_fence_object_signaled(drm_fence_object_t * fence,
				     uint32_t type, int flush);
extern void drm_fence_usage_deref_locked(drm_fence_object_t ** fence);
extern void drm_fence_usage_deref_unlocked(drm_fence_object_t ** fence);
extern struct drm_fence_object *drm_fence_reference_locked(struct drm_fence_object *src);
extern void drm_fence_reference_unlocked(struct drm_fence_object **dst,
					 struct drm_fence_object *src);
extern int drm_fence_object_wait(drm_fence_object_t * fence,
				 int lazy, int ignore_signals, uint32_t mask);
extern int drm_fence_object_create(struct drm_device *dev, uint32_t type,
				   uint32_t fence_flags, uint32_t class,
				   drm_fence_object_t ** c_fence);
extern int drm_fence_add_user_object(struct drm_file * priv,
				     drm_fence_object_t * fence, int shareable);

extern int drm_fence_create_ioctl(DRM_IOCTL_ARGS);
extern int drm_fence_destroy_ioctl(DRM_IOCTL_ARGS);
extern int drm_fence_reference_ioctl(DRM_IOCTL_ARGS);
extern int drm_fence_unreference_ioctl(DRM_IOCTL_ARGS);
extern int drm_fence_signaled_ioctl(DRM_IOCTL_ARGS);
extern int drm_fence_flush_ioctl(DRM_IOCTL_ARGS);
extern int drm_fence_wait_ioctl(DRM_IOCTL_ARGS);
extern int drm_fence_emit_ioctl(DRM_IOCTL_ARGS);
extern int drm_fence_buffers_ioctl(DRM_IOCTL_ARGS);
/**************************************************
 *TTMs
 */

/*
 * The ttm backend GTT interface. (In our case AGP).
 * Any similar type of device (PCIE?)
 * needs only to implement these functions to be usable with the "TTM" interface.
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
typedef struct drm_ttm_backend_func {
	int (*needs_ub_cache_adjust) (struct drm_ttm_backend * backend);
	int (*populate) (struct drm_ttm_backend * backend,
			 unsigned long num_pages, struct page ** pages);
	void (*clear) (struct drm_ttm_backend * backend);
	int (*bind) (struct drm_ttm_backend * backend,
		     unsigned long offset, int cached);
	int (*unbind) (struct drm_ttm_backend * backend);
	void (*destroy) (struct drm_ttm_backend * backend);
} drm_ttm_backend_func_t;


typedef struct drm_ttm_backend {
	uint32_t flags;
	int mem_type;
	drm_ttm_backend_func_t *func;
} drm_ttm_backend_t;

typedef struct drm_ttm {
	struct page **pages;
	uint32_t page_flags;
	unsigned long num_pages;
	unsigned long aper_offset;
	atomic_t vma_count;
	struct drm_device *dev;
	int destroy;
	uint32_t mapping_offset;
	drm_ttm_backend_t *be;
	enum {
		ttm_bound,
		ttm_evicted,
		ttm_unbound,
		ttm_unpopulated,
	} state;

} drm_ttm_t;

extern drm_ttm_t *drm_ttm_init(struct drm_device *dev, unsigned long size);
extern int drm_bind_ttm(drm_ttm_t * ttm, int cached, unsigned long aper_offset);
extern void drm_ttm_unbind(drm_ttm_t * ttm);
extern void drm_ttm_evict(drm_ttm_t * ttm);
extern void drm_ttm_fixup_caching(drm_ttm_t * ttm);
extern struct page *drm_ttm_get_page(drm_ttm_t * ttm, int index);

/*
 * Destroy a ttm. The user normally calls drmRmMap or a similar IOCTL to do this,
 * which calls this function iff there are no vmas referencing it anymore. Otherwise it is called
 * when the last vma exits.
 */

extern int drm_destroy_ttm(drm_ttm_t * ttm);

#define DRM_FLAG_MASKED(_old, _new, _mask) {\
(_old) ^= (((_old) ^ (_new)) & (_mask)); \
}

#define DRM_TTM_MASK_FLAGS ((1 << PAGE_SHIFT) - 1)
#define DRM_TTM_MASK_PFN (0xFFFFFFFFU - DRM_TTM_MASK_FLAGS)

/*
 * Page flags.
 */

#define DRM_TTM_PAGE_UNCACHED 0x01
#define DRM_TTM_PAGE_USED     0x02
#define DRM_TTM_PAGE_BOUND    0x04
#define DRM_TTM_PAGE_PRESENT  0x08
#define DRM_TTM_PAGE_VMALLOC  0x10

/***************************************************
 * Buffer objects. (drm_bo.c, drm_bo_move.c)
 */

typedef struct drm_bo_mem_reg {
	struct drm_mm_node *mm_node;
	unsigned long size;
	unsigned long num_pages;
	uint32_t page_alignment;
	uint32_t mem_type;
	uint64_t flags;
	uint64_t mask;
} drm_bo_mem_reg_t;

typedef struct drm_buffer_object {
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
	drm_bo_mem_reg_t mem;

	struct list_head lru;
	struct list_head ddestroy;

	uint32_t fence_type;
	uint32_t fence_class;
	drm_fence_object_t *fence;
	uint32_t priv_flags;
	wait_queue_head_t event_queue;
	struct mutex mutex;

	/* For pinned buffers */
	struct drm_mm_node *pinned_node;
	uint32_t pinned_mem_type;
	struct list_head pinned_lru;

	/* For vm */

	drm_ttm_t *ttm;
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

} drm_buffer_object_t;

#define _DRM_BO_FLAG_UNFENCED 0x00000001
#define _DRM_BO_FLAG_EVICTED  0x00000002

typedef struct drm_mem_type_manager {
	int has_type;
	int use_type;
	struct drm_mm manager;
	struct list_head lru;
	struct list_head pinned;
	uint32_t flags;
	uint32_t drm_bus_maptype;
	unsigned long io_offset;
	unsigned long io_size;
	void *io_addr;
} drm_mem_type_manager_t;

#define _DRM_FLAG_MEMTYPE_FIXED     0x00000001	/* Fixed (on-card) PCI memory */
#define _DRM_FLAG_MEMTYPE_MAPPABLE  0x00000002	/* Memory mappable */
#define _DRM_FLAG_MEMTYPE_CACHED    0x00000004	/* Cached binding */
#define _DRM_FLAG_NEEDS_IOREMAP     0x00000008	/* Fixed memory needs ioremap
						   before kernel access. */
#define _DRM_FLAG_MEMTYPE_CMA       0x00000010	/* Can't map aperture */
#define _DRM_FLAG_MEMTYPE_CSELECT   0x00000020	/* Select caching */

typedef struct drm_buffer_manager {
	struct mutex init_mutex;
	struct mutex evict_mutex;
	int nice_mode;
	int initialized;
	struct drm_file *last_to_validate;
	drm_mem_type_manager_t man[DRM_BO_MEM_TYPES];
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
} drm_buffer_manager_t;

typedef struct drm_bo_driver {
	const uint32_t *mem_type_prio;
	const uint32_t *mem_busy_prio;
	uint32_t num_mem_type_prio;
	uint32_t num_mem_busy_prio;
	drm_ttm_backend_t *(*create_ttm_backend_entry)
	 (struct drm_device * dev);
	int (*fence_type) (struct drm_buffer_object *bo, uint32_t * type);
	int (*invalidate_caches) (struct drm_device * dev, uint64_t flags);
	int (*init_mem_type) (struct drm_device * dev, uint32_t type,
			      drm_mem_type_manager_t * man);
	 uint32_t(*evict_mask) (struct drm_buffer_object *bo);
	int (*move) (struct drm_buffer_object * bo,
		     int evict, int no_wait, struct drm_bo_mem_reg * new_mem);
} drm_bo_driver_t;

/*
 * buffer objects (drm_bo.c)
 */

extern int drm_bo_create_ioctl(DRM_IOCTL_ARGS);
extern int drm_bo_destroy_ioctl(DRM_IOCTL_ARGS);
extern int drm_bo_map_ioctl(DRM_IOCTL_ARGS);
extern int drm_bo_unmap_ioctl(DRM_IOCTL_ARGS);
extern int drm_bo_reference_ioctl(DRM_IOCTL_ARGS);
extern int drm_bo_unreference_ioctl(DRM_IOCTL_ARGS);
extern int drm_bo_wait_idle_ioctl(DRM_IOCTL_ARGS);
extern int drm_bo_info_ioctl(DRM_IOCTL_ARGS);
extern int drm_bo_op_ioctl(DRM_IOCTL_ARGS);


extern int drm_mm_init_ioctl(DRM_IOCTL_ARGS);
extern int drm_mm_takedown_ioctl(DRM_IOCTL_ARGS);
extern int drm_mm_lock_ioctl(DRM_IOCTL_ARGS);
extern int drm_mm_unlock_ioctl(DRM_IOCTL_ARGS);
extern int drm_bo_driver_finish(struct drm_device *dev);
extern int drm_bo_driver_init(struct drm_device *dev);
extern int drm_bo_pci_offset(struct drm_device *dev,
			     drm_bo_mem_reg_t * mem,
			     unsigned long *bus_base,
			     unsigned long *bus_offset,
			     unsigned long *bus_size);
extern int drm_mem_reg_is_pci(struct drm_device *dev, drm_bo_mem_reg_t * mem);

extern void drm_bo_usage_deref_locked(drm_buffer_object_t ** bo);
extern int drm_fence_buffer_objects(struct drm_file * priv,
				    struct list_head *list,
				    uint32_t fence_flags,
				    drm_fence_object_t * fence,
				    drm_fence_object_t ** used_fence);
extern void drm_bo_add_to_lru(drm_buffer_object_t * bo);
extern int drm_bo_wait(drm_buffer_object_t * bo, int lazy, int ignore_signals,
		       int no_wait);
extern int drm_bo_mem_space(drm_buffer_object_t * bo,
			    drm_bo_mem_reg_t * mem, int no_wait);
extern int drm_bo_move_buffer(drm_buffer_object_t * bo, uint32_t new_mem_flags,
			      int no_wait, int move_unfenced);

/*
 * Buffer object memory move helpers.
 * drm_bo_move.c
 */

extern int drm_bo_move_ttm(drm_buffer_object_t * bo,
			   int evict, int no_wait, drm_bo_mem_reg_t * new_mem);
extern int drm_bo_move_memcpy(drm_buffer_object_t * bo,
			      int evict,
			      int no_wait, drm_bo_mem_reg_t * new_mem);
extern int drm_bo_move_accel_cleanup(drm_buffer_object_t * bo,
				     int evict,
				     int no_wait,
				     uint32_t fence_class,
				     uint32_t fence_type,
				     uint32_t fence_flags,
				     drm_bo_mem_reg_t * new_mem);

#ifdef CONFIG_DEBUG_MUTEXES
#define DRM_ASSERT_LOCKED(_mutex)					\
	BUG_ON(!mutex_is_locked(_mutex) ||				\
	       ((_mutex)->owner != current_thread_info()))
#else
#define DRM_ASSERT_LOCKED(_mutex)
#endif

#endif
