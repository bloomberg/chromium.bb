#ifndef _I810_DRM_H_
#define _I810_DRM_H_

/* WARNING: These defines must be the same as what the Xserver uses.
 * if you change them, you must change the defines in the Xserver.
 */

/* Might one day want to support the client-side ringbuffer code again.
 */
#ifndef _I810_DEFINES_
#define _I810_DEFINES_

#define I810_USE_BATCH			1
#define I810_DMA_BUF_ORDER		12
#define I810_DMA_BUF_SZ 		(1<<I810_DMA_BUF_ORDER)
#define I810_DMA_BUF_NR 		256
#define I810_NR_SAREA_CLIPRECTS 	2

/* Each region is a minimum of 64k, and there are at most 64 of them.
 */

#define I810_NR_TEX_REGIONS 64
#define I810_LOG_MIN_TEX_REGION_SIZE 16
#endif

typedef struct _drm_i810_init {
   	enum {
	   	I810_INIT_DMA = 0x01,
	   	I810_CLEANUP_DMA = 0x02
	} func;
   	int ring_map_idx;
      	int buffer_map_idx;
	int sarea_priv_offset;
   	unsigned long ring_start;
   	unsigned long ring_end;
   	unsigned long ring_size;
} drm_i810_init_t;

/* Warning: If you change the SAREA structure you must change the Xserver
 * structure as well */

typedef struct _drm_i810_tex_region {
	unsigned char next, prev; /* indices to form a circular LRU  */
	unsigned char in_use;	/* owned by a client, or free? */
	int age;		/* tracked by clients to update local LRU's */
} drm_i810_tex_region_t;

typedef struct _drm_i810_sarea {
	unsigned int nbox;
	drm_clip_rect_t boxes[I810_NR_SAREA_CLIPRECTS];

	/* Maintain an LRU of contiguous regions of texture space.  If
	 * you think you own a region of texture memory, and it has an
	 * age different to the one you set, then you are mistaken and
	 * it has been stolen by another client.  If global texAge
	 * hasn't changed, there is no need to walk the list.
	 *
	 * These regions can be used as a proxy for the fine-grained
	 * texture information of other clients - by maintaining them
	 * in the same lru which is used to age their own textures,
	 * clients have an approximate lru for the whole of global
	 * texture space, and can make informed decisions as to which
	 * areas to kick out.  There is no need to choose whether to
	 * kick out your own texture or someone else's - simply eject
	 * them all in LRU order.  
	 */
   
	drm_i810_tex_region_t texList[I810_NR_TEX_REGIONS+1]; 
				/* Last elt is sentinal */
        int texAge;		/* last time texture was uploaded */
        int last_enqueue;	/* last time a buffer was enqueued */
	int last_dispatch;	/* age of the most recently dispatched buffer */
	int last_quiescent;     /*  */
	int ctxOwner;		/* last context to upload state */
} drm_i810_sarea_t;

typedef struct _drm_i810_general {
   	int idx;
	int used;
} drm_i810_general_t;

/* These may be placeholders if we have more cliprects than
 * I810_NR_SAREA_CLIPRECTS.  In that case, the client sets discard to
 * false, indicating that the buffer will be dispatched again with a
 * new set of cliprects.
 */
typedef struct _drm_i810_vertex {
   	int idx;		/* buffer index */
	int used;		/* nr bytes in use */
	int discard;		/* client is finished with the buffer? */
} drm_i810_vertex_t;

#endif /* _I810_DRM_H_ */
