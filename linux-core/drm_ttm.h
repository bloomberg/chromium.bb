#ifndef _DRM_TTM_H
#define _DRM_TTM_H
#define DRM_HAS_TTM

/*
 * The backend GART interface. (In our case AGP). Any similar type of device (PCIE?)
 * needs only to implement these functions to be usable with the "TTM" interface.
 * The AGP backend implementation lives in drm_agpsupport.c 
 * basically maps these calls to available functions in agpgart. Each drm device driver gets an
 * additional function pointer that creates these types, 
 * so that the device can choose the correct aperture.
 * (Multiple AGP apertures, etc.) 
 * Most device drivers will let this point to the standard AGP implementation.
 */

typedef struct drm_ttm_backend {
	unsigned long aperture_base;
	void *private;
	int (*needs_cache_adjust) (struct drm_ttm_backend * backend);
	int (*populate) (struct drm_ttm_backend * backend,
			 unsigned long num_pages, struct page ** pages);
	void (*clear) (struct drm_ttm_backend * backend);
	int (*bind) (struct drm_ttm_backend * backend, unsigned long offset);
	int (*unbind) (struct drm_ttm_backend * backend);
	void (*destroy) (struct drm_ttm_backend * backend);
} drm_ttm_backend_t;

#define DRM_FLUSH_READ  (0x01)
#define DRM_FLUSH_WRITE (0x02)
#define DRM_FLUSH_EXE   (0x04)

typedef struct drm_ttm_backend_list {
        drm_hash_item_t hash;
	uint32_t flags;
	atomic_t refcount;
	struct list_head head;
	drm_ttm_backend_t *be;
	unsigned page_offset;
	unsigned num_pages;
	struct drm_ttm *owner;
	drm_file_t *anon_owner;
	struct page **anon_pages;
	int anon_locked;
	enum {
		ttm_bound,
		ttm_evicted,
		ttm_unbound
	} state;
} drm_ttm_backend_list_t;

typedef struct drm_ttm_vma_list {
	struct list_head head;
	pgprot_t orig_protection;
	struct vm_area_struct *vma;
	drm_map_t *map;
} drm_ttm_vma_list_t;

typedef struct drm_ttm {
	struct list_head p_mm_list;
	atomic_t shared_count;
	uint32_t mm_list_seq;
	unsigned long aperture_base;
	struct page **pages;
	uint32_t *page_flags;
	unsigned long lhandle;
	unsigned long num_pages;
	drm_ttm_vma_list_t *vma_list;
	struct drm_device *dev;
	drm_ttm_backend_list_t *be_list;
	atomic_t vma_count;
	atomic_t unfinished_regions;
	drm_file_t *owner;
	int destroy;
	int mmap_sem_locked;
} drm_ttm_t;

/*
 * Initialize a ttm. Currently the size is fixed. Currently drmAddMap calls this function
 * and creates a DRM map of type _DRM_TTM, and returns a reference to that map to the 
 * caller.
 */

drm_ttm_t *drm_init_ttm(struct drm_device *dev, unsigned long size);

/*
 * Bind a part of the ttm starting at page_offset size n_pages into the GTT, at
 * aperture offset aper_offset. The region handle will be used to reference this
 * bound region in the future. Note that the region may be the whole ttm. 
 * Regions should not overlap.
 * This function sets all affected pages as noncacheable and flushes cashes and TLB.
 */

int drm_create_ttm_region(drm_ttm_t * ttm, unsigned long page_offset,
			  unsigned long n_pages, int cached,
			  drm_ttm_backend_list_t ** region);

int drm_bind_ttm_region(drm_ttm_backend_list_t * region,
			unsigned long aper_offset);

/*
 * Unbind a ttm region. Restores caching policy. Flushes caches and TLB.
 */

void drm_unbind_ttm_region(drm_ttm_backend_list_t * entry);
void drm_destroy_ttm_region(drm_ttm_backend_list_t * entry);

/*
 * Evict a ttm region. Keeps Aperture caching policy.
 */

int drm_evict_ttm_region(drm_ttm_backend_list_t * entry);

/*
 * Rebind an already evicted region into a possibly new location in the aperture.
 */

int drm_rebind_ttm_region(drm_ttm_backend_list_t * entry,
			  unsigned long aper_offset);

/*
 * Destroy a ttm. The user normally calls drmRmMap or a similar IOCTL to do this, 
 * which calls this function iff there are no vmas referencing it anymore. Otherwise it is called
 * when the last vma exits.
 */

extern int drm_destroy_ttm(drm_ttm_t * ttm);
extern void drm_user_destroy_region(drm_ttm_backend_list_t * entry);
extern int drm_ttm_add_mm_to_list(drm_ttm_t * ttm, struct mm_struct *mm);
extern void drm_ttm_delete_mm(drm_ttm_t * ttm, struct mm_struct *mm);
extern void drm_ttm_fence_before_destroy(drm_ttm_t * ttm);
extern void drm_fence_unfenced_region(drm_ttm_backend_list_t * entry);

extern int drm_ttm_ioctl(DRM_IOCTL_ARGS);
extern int drm_mm_init_ioctl(DRM_IOCTL_ARGS);
extern int drm_mm_fence_ioctl(DRM_IOCTL_ARGS);

#define DRM_MASK_VAL(dest, mask, val)			\
  (dest) = ((dest) & ~(mask)) | ((val) & (mask));

#define DRM_TTM_MASK_FLAGS ((1 << PAGE_SHIFT) - 1)
#define DRM_TTM_MASK_PFN (0xFFFFFFFFU - DRM_TTM_MASK_FLAGS)

/*
 * Page flags.
 */

#define DRM_TTM_PAGE_UNCACHED 0x01
#define DRM_TTM_PAGE_USED     0x02
#define DRM_TTM_PAGE_BOUND    0x04
#define DRM_TTM_PAGE_PRESENT  0x08

#endif
