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

#define DRM_FENCE_FLAG_EMIT                0x00000001
#define DRM_FENCE_FLAG_SHAREABLE           0x00000002
/**
 * On hardware with no interrupt events for operation completion,
 * indicates that the kernel should sleep while waiting for any blocking
 * operation to complete rather than spinning.
 *
 * Has no effect otherwise.
 */
#define DRM_FENCE_FLAG_WAIT_LAZY           0x00000004
#define DRM_FENCE_FLAG_NO_USER             0x00000010

/* Reserved for driver use */
#define DRM_FENCE_MASK_DRIVER              0xFF000000

#define DRM_FENCE_TYPE_EXE                 0x00000001

struct drm_fence_arg {
	unsigned int handle;
	unsigned int fence_class;
	unsigned int type;
	unsigned int flags;
	unsigned int signaled;
	unsigned int error;
	unsigned int sequence;
	unsigned int pad64;
	uint64_t expand_pad[2]; /*Future expansion */
};

/* Buffer permissions, referring to how the GPU uses the buffers.
 * these translate to fence types used for the buffers.
 * Typically a texture buffer is read, A destination buffer is write and
 *  a command (batch-) buffer is exe. Can be or-ed together.
 */

#define DRM_BO_FLAG_READ        (1ULL << 0)
#define DRM_BO_FLAG_WRITE       (1ULL << 1)
#define DRM_BO_FLAG_EXE         (1ULL << 2)

/*
 * All of the bits related to access mode
 */
#define DRM_BO_MASK_ACCESS	(DRM_BO_FLAG_READ | DRM_BO_FLAG_WRITE | DRM_BO_FLAG_EXE)
/*
 * Status flags. Can be read to determine the actual state of a buffer.
 * Can also be set in the buffer mask before validation.
 */

/*
 * Mask: Never evict this buffer. Not even with force. This type of buffer is only
 * available to root and must be manually removed before buffer manager shutdown
 * or lock.
 * Flags: Acknowledge
 */
#define DRM_BO_FLAG_NO_EVICT    (1ULL << 4)

/*
 * Mask: Require that the buffer is placed in mappable memory when validated.
 *       If not set the buffer may or may not be in mappable memory when validated.
 * Flags: If set, the buffer is in mappable memory.
 */
#define DRM_BO_FLAG_MAPPABLE    (1ULL << 5)

/* Mask: The buffer should be shareable with other processes.
 * Flags: The buffer is shareable with other processes.
 */
#define DRM_BO_FLAG_SHAREABLE   (1ULL << 6)

/* Mask: If set, place the buffer in cache-coherent memory if available.
 *       If clear, never place the buffer in cache coherent memory if validated.
 * Flags: The buffer is currently in cache-coherent memory.
 */
#define DRM_BO_FLAG_CACHED      (1ULL << 7)

/* Mask: Make sure that every time this buffer is validated,
 *       it ends up on the same location provided that the memory mask is the same.
 *       The buffer will also not be evicted when claiming space for
 *       other buffers. Basically a pinned buffer but it may be thrown out as
 *       part of buffer manager shutdown or locking.
 * Flags: Acknowledge.
 */
#define DRM_BO_FLAG_NO_MOVE     (1ULL << 8)

/*
 * Mask: if set the note the buffer contents are discardable
 * Flags: if set the buffer contents are discardable on migration
 */
#define DRM_BO_FLAG_DISCARDABLE (1ULL << 9)

/* Mask: Make sure the buffer is in cached memory when mapped.  In conjunction
 * with DRM_BO_FLAG_CACHED it also allows the buffer to be bound into the GART
 * with unsnooped PTEs instead of snooped, by using chipset-specific cache
 * flushing at bind time.  A better name might be DRM_BO_FLAG_TT_UNSNOOPED,
 * as the eviction to local memory (TTM unbind) on map is just a side effect
 * to prevent aggressive cache prefetch from the GPU disturbing the cache
 * management that the DRM is doing.
 *
 * Flags: Acknowledge.
 * Buffers allocated with this flag should not be used for suballocators
 * This type may have issues on CPUs with over-aggressive caching
 * http://marc.info/?l=linux-kernel&m=102376926732464&w=2
 */
#define DRM_BO_FLAG_CACHED_MAPPED    (1ULL << 19)


/* Mask: Force DRM_BO_FLAG_CACHED flag strictly also if it is set.
 * Flags: Acknowledge.
 */
#define DRM_BO_FLAG_FORCE_CACHING  (1ULL << 13)

/*
 * Mask: Force DRM_BO_FLAG_MAPPABLE flag strictly also if it is clear.
 * Flags: Acknowledge.
 */
#define DRM_BO_FLAG_FORCE_MAPPABLE (1ULL << 14)
#define DRM_BO_FLAG_TILE           (1ULL << 15)

/*
 * Buffer has been mapped or touched since creation
 * for VRAM we don't need to migrate, just fill with 0s for non-dirty
 */
#define DRM_BO_FLAG_CLEAN  (1ULL << 16)

/*
 * Memory type flags that can be or'ed together in the mask, but only
 * one appears in flags.
 */

/* System memory */
#define DRM_BO_FLAG_MEM_LOCAL  (1ULL << 24)
/* Translation table memory */
#define DRM_BO_FLAG_MEM_TT     (1ULL << 25)
/* Vram memory */
#define DRM_BO_FLAG_MEM_VRAM   (1ULL << 26)
/* Up to the driver to define. */
#define DRM_BO_FLAG_MEM_PRIV0  (1ULL << 27)
#define DRM_BO_FLAG_MEM_PRIV1  (1ULL << 28)
#define DRM_BO_FLAG_MEM_PRIV2  (1ULL << 29)
#define DRM_BO_FLAG_MEM_PRIV3  (1ULL << 30)
#define DRM_BO_FLAG_MEM_PRIV4  (1ULL << 31)
/* We can add more of these now with a 64-bit flag type */

/*
 * This is a mask covering all of the memory type flags; easier to just
 * use a single constant than a bunch of | values. It covers
 * DRM_BO_FLAG_MEM_LOCAL through DRM_BO_FLAG_MEM_PRIV4
 */
#define DRM_BO_MASK_MEM         0x00000000FF000000ULL
/*
 * This adds all of the CPU-mapping options in with the memory
 * type to label all bits which change how the page gets mapped
 */
#define DRM_BO_MASK_MEMTYPE     (DRM_BO_MASK_MEM | \
				 DRM_BO_FLAG_CACHED_MAPPED | \
				 DRM_BO_FLAG_CACHED | \
				 DRM_BO_FLAG_MAPPABLE)
				 
/* Driver-private flags */
#define DRM_BO_MASK_DRIVER      0xFFFF000000000000ULL

/*
 * Don't block on validate and map. Instead, return EBUSY.
 */
#define DRM_BO_HINT_DONT_BLOCK  0x00000002
/*
 * Don't place this buffer on the unfenced list. This means
 * that the buffer will not end up having a fence associated
 * with it as a result of this operation
 */
#define DRM_BO_HINT_DONT_FENCE  0x00000004
/**
 * On hardware with no interrupt events for operation completion,
 * indicates that the kernel should sleep while waiting for any blocking
 * operation to complete rather than spinning.
 *
 * Has no effect otherwise.
 */
#define DRM_BO_HINT_WAIT_LAZY   0x00000008
/*
 * The client has compute relocations refering to this buffer using the
 * offset in the presumed_offset field. If that offset ends up matching
 * where this buffer lands, the kernel is free to skip executing those
 * relocations
 */
#define DRM_BO_HINT_PRESUMED_OFFSET 0x00000010

#define DRM_BO_MEM_LOCAL 0
#define DRM_BO_MEM_TT 1
#define DRM_BO_MEM_VRAM 2
#define DRM_BO_MEM_PRIV0 3
#define DRM_BO_MEM_PRIV1 4
#define DRM_BO_MEM_PRIV2 5
#define DRM_BO_MEM_PRIV3 6
#define DRM_BO_MEM_PRIV4 7

#define DRM_BO_MEM_TYPES 8 /* For now. */

#define DRM_BO_LOCK_UNLOCK_BM       (1 << 0)
#define DRM_BO_LOCK_IGNORE_NO_EVICT (1 << 1)


/***************************************************
 * Fence objects. (drm_fence.c)
 */

struct drm_fence_object {
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

/**
 * This structure associates a set of flags and methods with a drm_ttm
 * object, and will also be subclassed by the particular backend.
 *
 * \sa #drm_agp_ttm_backend
 */
struct drm_ttm_backend {
	struct drm_device *dev;
	uint32_t flags;
	struct drm_ttm_backend_func *func;
};

struct drm_ttm {
	struct page *dummy_read_page;
	struct page **pages;
	long first_himem_page;
	long last_lomem_page;
	uint32_t page_flags;
	unsigned long num_pages;
	atomic_t vma_count;
	struct drm_device *dev;
	int destroy;
	uint32_t mapping_offset;
	struct drm_ttm_backend *be;
	unsigned long highest_lomem_entry;
	unsigned long lowest_himem_entry;
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
extern void drm_ttm_cache_flush(struct page *pages[], unsigned long num_pages);
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
#define DRM_TTM_PAGEDIR_VMALLOC (1 << 4)
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

/*
 * This flag indicates that a flag called with bo->mutex held has
 * temporarily released the buffer object mutex, (usually to wait for something).
 * and thus any post-lock validation needs to be rerun.
 */

#define _DRM_BO_FLAG_UNLOCKED 0x00000004

struct drm_mem_type_manager {
	int has_type;
	int use_type;
	int kern_init_type;
	struct drm_mm manager;
	struct list_head lru;
	struct list_head pinned;
	uint32_t flags;
	uint32_t drm_bus_maptype;
	unsigned long gpu_offset;
	unsigned long io_offset;
	unsigned long io_size;
	void *io_addr;
	uint64_t size; /* size of managed area for reporting to userspace */
};

struct drm_bo_lock {
  //	struct drm_user_object base;
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
int drm_bo_do_validate(struct drm_buffer_object *bo,
		       uint64_t flags, uint64_t mask, uint32_t hint,
		       uint32_t fence_class);
extern int drm_bo_set_pin(struct drm_device *dev, struct drm_buffer_object *bo, int pin);
extern int drm_bo_driver_finish(struct drm_device *dev);
extern int drm_bo_driver_init(struct drm_device *dev);
extern int drm_bo_pci_offset(struct drm_device *dev,
			     struct drm_bo_mem_reg *mem,
			     unsigned long *bus_base,
			     unsigned long *bus_offset,
			     unsigned long *bus_size);
extern int drm_mem_reg_is_pci(struct drm_device *dev, struct drm_bo_mem_reg *mem);

extern int drm_bo_add_user_object(struct drm_file *file_priv,
				  struct drm_buffer_object *bo, int shareable);
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
extern int drm_bo_wait(struct drm_buffer_object *bo, int lazy, int interruptible,
		       int no_wait, int check_unfenced);
extern int drm_bo_mem_space(struct drm_buffer_object *bo,
			    struct drm_bo_mem_reg *mem, int no_wait);
extern int drm_bo_move_buffer(struct drm_buffer_object *bo,
			      uint64_t new_mem_flags,
			      int no_wait, int move_unfenced);
extern int drm_bo_clean_mm(struct drm_device *dev, unsigned mem_type, int kern_clean);
extern int drm_bo_init_mm(struct drm_device *dev, unsigned type,
			  unsigned long p_offset, unsigned long p_size,
			  int kern_init);
extern struct drm_buffer_object *drm_lookup_buffer_object(struct drm_file *file_priv,
							  uint32_t handle,
							  int check_owner);
extern int drm_bo_evict_cached(struct drm_buffer_object *bo);

extern void drm_bo_takedown_vm_locked(struct drm_buffer_object *bo);
extern void drm_bo_evict_mm(struct drm_device *dev, int mem_type, int no_wait);
/*
 * Buffer object memory move- and map helpers.
 * drm_bo_move.c
 */

extern int drm_bo_add_ttm(struct drm_buffer_object *bo);
extern int drm_bo_move_ttm(struct drm_buffer_object *bo,
			   int evict, int no_wait,
			   struct drm_bo_mem_reg *new_mem);
extern int drm_bo_move_memcpy(struct drm_buffer_object *bo,
			      int evict,
			      int no_wait, struct drm_bo_mem_reg *new_mem);
extern int drm_bo_move_zero(struct drm_buffer_object *bo,
			    int evict, int no_wait, struct drm_bo_mem_reg *new_mem);
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

extern int drm_mem_reg_ioremap(struct drm_device *dev, struct drm_bo_mem_reg * mem,
			       void **virtual);
extern void drm_mem_reg_iounmap(struct drm_device *dev, struct drm_bo_mem_reg * mem,
				void *virtual);
#ifdef CONFIG_DEBUG_MUTEXES
#define DRM_ASSERT_LOCKED(_mutex)					\
	BUG_ON(!mutex_is_locked(_mutex) ||				\
	       ((_mutex)->owner != current_thread_info()))
#else
#define DRM_ASSERT_LOCKED(_mutex)
#endif
#endif
