#ifndef AV1_COMMON_CONVOLVE_H_
#define AV1_COMMON_CONVOLVE_H_
#include "av1/common/filter.h"

#ifdef __cplusplus
extern "C" {
#endif

void av1_convolve(const uint8_t *src, int src_stride, uint8_t *dst,
                  int dst_stride, int w, int h,
                  const InterpFilter *interp_filter, const int subpel_x,
                  const int subpel_y, int xstep, int ystep, int avg);

#if CONFIG_AOM_HIGHBITDEPTH
void av1_highbd_convolve(const uint8_t *src, int src_stride, uint8_t *dst,
                         int dst_stride, int w, int h,
                         const InterpFilter *interp_filter, const int subpel_x,
                         const int subpel_y, int xstep, int ystep, int avg,
                         int bd);
#endif  // CONFIG_AOM_HIGHBITDEPTH

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_AOM_CONVOLVE_H_
