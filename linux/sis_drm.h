
#ifndef _sis_drm_public_h_
#define _sis_drm_public_h_

typedef struct {
  int context;
  unsigned int offset;
  unsigned int size;
  unsigned int free;
} drm_sis_mem_t;

typedef struct {
  unsigned int offset, size;
} drm_sis_agp_t;

typedef struct {
  unsigned int left, right;
} drm_sis_flip_t;

#endif
