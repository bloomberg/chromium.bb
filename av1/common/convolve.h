/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AV1_COMMON_AV1_CONVOLVE_H_
#define AV1_COMMON_AV1_CONVOLVE_H_
#include "av1/common/filter.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum CONVOLVE_OPT {
  // indicate the results in dst buf is rounded by FILTER_BITS or not
  CONVOLVE_OPT_ROUND,
  CONVOLVE_OPT_NO_ROUND,
} CONVOLVE_OPT;

typedef struct ConvolveParams {
  int ref;
  CONVOLVE_OPT round;
  int32_t *dst;
  int dst_stride;
} ConvolveParams;

static INLINE ConvolveParams get_conv_params(int ref) {
  ConvolveParams conv_params;
  conv_params.ref = ref;
  conv_params.round = CONVOLVE_OPT_ROUND;
  return conv_params;
}

#if CONFIG_CONVOLVE_ROUND
static INLINE ConvolveParams get_conv_params_no_round(int ref, int32_t *dst,
                                                      int dst_stride) {
  ConvolveParams conv_params;
  conv_params.ref = ref;
  conv_params.round = CONVOLVE_OPT_NO_ROUND;
  conv_params.dst = dst;
  conv_params.dst_stride = dst_stride;
  return conv_params;
}

void av1_convolve_rounding(const int32_t *src, int src_stride, uint8_t *dst,
                           int dst_stride, int w, int h);
#endif  // CONFIG_CONVOLVE_ROUND

void av1_convolve(const uint8_t *src, int src_stride, uint8_t *dst,
                  int dst_stride, int w, int h,
#if CONFIG_DUAL_FILTER
                  const InterpFilter *interp_filter,
#else
                  const InterpFilter interp_filter,
#endif
                  const int subpel_x, int xstep, const int subpel_y, int ystep,
                  ConvolveParams *conv_params);

#if CONFIG_AOM_HIGHBITDEPTH
void av1_highbd_convolve(const uint8_t *src, int src_stride, uint8_t *dst,
                         int dst_stride, int w, int h,
#if CONFIG_DUAL_FILTER
                         const InterpFilter *interp_filter,
#else
                         const InterpFilter interp_filter,
#endif
                         const int subpel_x, int xstep, const int subpel_y,
                         int ystep, int avg, int bd);
#endif  // CONFIG_AOM_HIGHBITDEPTH

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_AV1_CONVOLVE_H_
