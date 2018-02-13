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

typedef int32_t CONV_BUF_TYPE;

typedef struct ConvolveParams {
  int ref;
  int do_average;
  CONVOLVE_OPT round;
  CONV_BUF_TYPE *dst;
  int dst_stride;
  int round_0;
  int round_1;
  int plane;
  int do_post_rounding;
  int is_compound;
#if CONFIG_JNT_COMP
  int use_jnt_comp_avg;
  int fwd_offset;
  int bck_offset;
#endif
} ConvolveParams;

#define ROUND0_BITS (5 - 2 * (CONFIG_HIGHPRECISION_INTBUF))

#if CONFIG_LOWPRECISION_BLEND == 1
#define COMPOUND_ROUND1_BITS (4 + 2 * (CONFIG_HIGHPRECISION_INTBUF))
#elif CONFIG_LOWPRECISION_BLEND == 2
#define COMPOUND_ROUND1_BITS (5 + 2 * (CONFIG_HIGHPRECISION_INTBUF))
#else
#define COMPOUND_ROUND1_BITS 0
#endif  // CONFIG_LOWPRECISION_BLEND

typedef void (*aom_convolve_fn_t)(const uint8_t *src, int src_stride,
                                  uint8_t *dst, int dst_stride, int w, int h,
                                  InterpFilterParams *filter_params_x,
                                  InterpFilterParams *filter_params_y,
                                  const int subpel_x_q4, const int subpel_y_q4,
                                  ConvolveParams *conv_params);

static INLINE void av1_get_convolve_filter_params(InterpFilters interp_filters,
                                                  InterpFilterParams *params_x,
                                                  InterpFilterParams *params_y
#if CONFIG_SHORT_FILTER
                                                  ,
                                                  int w, int h
#endif
) {
#if CONFIG_DUAL_FILTER
  InterpFilter filter_x = av1_extract_interp_filter(interp_filters, 1);
  InterpFilter filter_y = av1_extract_interp_filter(interp_filters, 0);
#else
  InterpFilter filter_x = av1_extract_interp_filter(interp_filters, 0);
  InterpFilter filter_y = av1_extract_interp_filter(interp_filters, 0);
#endif
#if CONFIG_SHORT_FILTER
  *params_x = av1_get_interp_filter_params_with_block_size(filter_x, w);
  *params_y = av1_get_interp_filter_params_with_block_size(filter_y, h);
#else
  *params_x = av1_get_interp_filter_params(filter_x);
  *params_y = av1_get_interp_filter_params(filter_y);
#endif
}

struct AV1Common;
struct scale_factors;

void av1_convolve_2d_facade(const uint8_t *src, int src_stride, uint8_t *dst,
                            int dst_stride, int w, int h,
                            InterpFilters interp_filters, const int subpel_x_q4,
                            int x_step_q4, const int subpel_y_q4, int y_step_q4,
                            int scaled, ConvolveParams *conv_params,
                            const struct scale_factors *sf);

static INLINE ConvolveParams get_conv_params_round(int ref, int do_average,
                                                   int plane, int bd) {
  ConvolveParams conv_params;
  conv_params.ref = ref;
  conv_params.do_average = do_average;
  conv_params.plane = plane;
  conv_params.round = CONVOLVE_OPT_ROUND;
  conv_params.round_0 = ROUND0_BITS;
  conv_params.round_1 = 0;
  conv_params.do_post_rounding = 0;
  conv_params.is_compound = 0;
  conv_params.dst = NULL;
  conv_params.dst_stride = 0;
  const int intbufrange = bd + FILTER_BITS - conv_params.round_0 + 2;
  if (bd < 12) assert(intbufrange <= 16);
  if (intbufrange > 16) {
    conv_params.round_0 += intbufrange - 16;
  }
  return conv_params;
}

static INLINE ConvolveParams get_conv_params_no_round(int ref, int do_average,
                                                      int plane, int32_t *dst,
                                                      int dst_stride,
                                                      int is_compound, int bd) {
  ConvolveParams conv_params;
  conv_params.ref = ref;
  conv_params.do_average = do_average;
  conv_params.round = CONVOLVE_OPT_NO_ROUND;
  conv_params.is_compound = is_compound;
  conv_params.round_0 = ROUND0_BITS;
#if CONFIG_LOWPRECISION_BLEND
  conv_params.round_1 = is_compound ? COMPOUND_ROUND1_BITS
                                    : 2 * FILTER_BITS - conv_params.round_0;
#else
  conv_params.round_1 = 0;
#endif
  const int intbufrange = bd + FILTER_BITS - conv_params.round_0 + 2;
  if (bd < 12) assert(intbufrange <= 16);
  if (intbufrange > 16) {
    conv_params.round_0 += intbufrange - 16;
    if (is_compound && conv_params.round_1 > 0)
      conv_params.round_1 -= intbufrange - 16;
  }
  // TODO(yunqing): The following dst should only be valid while
  // is_compound = 1;
  conv_params.dst = dst;
  conv_params.dst_stride = dst_stride;
  conv_params.plane = plane;
  conv_params.do_post_rounding = 0;
  return conv_params;
}

static INLINE ConvolveParams get_conv_params(int ref, int do_average, int plane,
                                             int bd) {
  return get_conv_params_no_round(ref, do_average, plane, NULL, 0, 0, bd);
}

void av1_highbd_convolve_2d_facade(const uint8_t *src8, int src_stride,
                                   uint8_t *dst, int dst_stride, int w, int h,
                                   InterpFilters interp_filters,
                                   const int subpel_x_q4, int x_step_q4,
                                   const int subpel_y_q4, int y_step_q4,
                                   int scaled, ConvolveParams *conv_params,
                                   int bd);

void av1_convolve(const uint8_t *src, int src_stride, uint8_t *dst,
                  int dst_stride, int w, int h, InterpFilters interp_filters,
                  const int subpel_x, int xstep, const int subpel_y, int ystep,
                  ConvolveParams *conv_params);

void av1_convolve_c(const uint8_t *src, int src_stride, uint8_t *dst,
                    int dst_stride, int w, int h, InterpFilters interp_filters,
                    const int subpel_x, int xstep, const int subpel_y,
                    int ystep, ConvolveParams *conv_params);

void av1_convolve_scale(const uint8_t *src, int src_stride, uint8_t *dst,
                        int dst_stride, int w, int h,
                        InterpFilters interp_filters, const int subpel_x,
                        int xstep, const int subpel_y, int ystep,
                        ConvolveParams *conv_params);

void av1_highbd_convolve(const uint8_t *src, int src_stride, uint8_t *dst,
                         int dst_stride, int w, int h,
                         InterpFilters interp_filters, const int subpel_x,
                         int xstep, const int subpel_y, int ystep, int avg,
                         int bd);

void av1_highbd_convolve_scale(const uint8_t *src, int src_stride, uint8_t *dst,
                               int dst_stride, int w, int h,
                               InterpFilters interp_filters, const int subpel_x,
                               int xstep, const int subpel_y, int ystep,
                               int avg, int bd);
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_AV1_CONVOLVE_H_
