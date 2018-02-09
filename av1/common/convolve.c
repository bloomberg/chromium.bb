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

#include <assert.h>
#include <string.h>

#include "./aom_dsp_rtcd.h"
#include "./av1_rtcd.h"
#include "av1/common/blockd.h"
#include "av1/common/convolve.h"
#include "av1/common/filter.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/resize.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_ports/mem.h"

#define MAX_BLOCK_WIDTH (MAX_SB_SIZE)
#define MAX_BLOCK_HEIGHT (MAX_SB_SIZE)
#define MAX_STEP (32)

#if CONFIG_HORZONLY_FRAME_SUPERRES

#define UPSCALE_PROC_UNIT 64  // Source step (roughly)
#define UPSCALE_PROC_UNIT_SCALE (UPSCALE_PROC_UNIT / SCALE_NUMERATOR)

void av1_convolve_horiz_rs_c(const uint8_t *src, int src_stride, uint8_t *dst,
                             int dst_stride, int w, int h,
                             const int16_t *x_filters, const int x0_qn,
                             const int x_step_qn) {
  src -= UPSCALE_NORMATIVE_TAPS / 2 - 1;
  for (int y = 0; y < h; ++y) {
    int x_qn = x0_qn;
    for (int x = 0; x < w; ++x) {
      const uint8_t *const src_x = &src[x_qn >> RS_SCALE_SUBPEL_BITS];
      const int x_filter_idx =
          (x_qn & RS_SCALE_SUBPEL_MASK) >> RS_SCALE_EXTRA_BITS;
      assert(x_filter_idx <= RS_SUBPEL_MASK);
      const int16_t *const x_filter =
          &x_filters[x_filter_idx * UPSCALE_NORMATIVE_TAPS];
      int sum = 0;
      for (int k = 0; k < UPSCALE_NORMATIVE_TAPS; ++k)
        sum += src_x[k] * x_filter[k];
      dst[x] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
      x_qn += x_step_qn;
    }
    src += src_stride;
    dst += dst_stride;
  }
}
// TODO(yaowu: remove "const" from pass-by-value params in this and other funcs)
void av1_highbd_convolve_horiz_rs_c(const uint16_t *src, int src_stride,
                                    uint16_t *dst, int dst_stride, int w, int h,
                                    const int16_t *x_filters, const int x0_qn,
                                    const int x_step_qn, int bd) {
  src -= UPSCALE_NORMATIVE_TAPS / 2 - 1;
  for (int y = 0; y < h; ++y) {
    int x_qn = x0_qn;
    for (int x = 0; x < w; ++x) {
      const uint16_t *const src_x = &src[x_qn >> RS_SCALE_SUBPEL_BITS];
      const int x_filter_idx =
          (x_qn & RS_SCALE_SUBPEL_MASK) >> RS_SCALE_EXTRA_BITS;
      assert(x_filter_idx <= RS_SUBPEL_MASK);
      const int16_t *const x_filter =
          &x_filters[x_filter_idx * UPSCALE_NORMATIVE_TAPS];
      int sum = 0;
      for (int k = 0; k < UPSCALE_NORMATIVE_TAPS; ++k)
        sum += src_x[k] * x_filter[k];
      dst[x] = clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd);
      x_qn += x_step_qn;
    }
    src += src_stride;
    dst += dst_stride;
  }
}
#endif  // CONFIG_HORZONLY_FRAME_SUPERRES

void av1_convolve_horiz_c(const uint8_t *src, int src_stride, uint8_t *dst,
                          int dst_stride, int w, int h,
                          const InterpFilterParams filter_params,
                          const int subpel_x_q4, int x_step_q4,
                          ConvolveParams *conv_params) {
  int filter_size = filter_params.taps;
  assert(conv_params->round == CONVOLVE_OPT_ROUND);
  src -= filter_size / 2 - 1;
  for (int y = 0; y < h; ++y) {
    int x_q4 = subpel_x_q4;
    for (int x = 0; x < w; ++x) {
      const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
      const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
          filter_params, x_q4 & SUBPEL_MASK);
      int sum = 0;
      for (int k = 0; k < filter_size; ++k) sum += src_x[k] * x_filter[k];

      sum = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
      if (conv_params->do_average)
        dst[x] = ROUND_POWER_OF_TWO(dst[x] + sum, 1);
      else
        dst[x] = sum;

      x_q4 += x_step_q4;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

void av1_convolve_horiz_scale(const uint8_t *src, int src_stride, uint8_t *dst,
                              int dst_stride, int w, int h,
                              const InterpFilterParams filter_params,
                              const int subpel_x_qn, int x_step_qn,
                              ConvolveParams *conv_params) {
  int filter_size = filter_params.taps;
  assert(conv_params->round == CONVOLVE_OPT_ROUND);
  src -= filter_size / 2 - 1;
  for (int y = 0; y < h; ++y) {
    int x_qn = subpel_x_qn;
    for (int x = 0; x < w; ++x) {
      const uint8_t *const src_x = &src[x_qn >> SCALE_SUBPEL_BITS];
      const int x_filter_idx = (x_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
      assert(x_filter_idx < SUBPEL_SHIFTS);
      const int16_t *x_filter =
          av1_get_interp_filter_subpel_kernel(filter_params, x_filter_idx);
      int sum = 0;
      for (int k = 0; k < filter_size; ++k) sum += src_x[k] * x_filter[k];

      sum = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
      if (conv_params->do_average)
        dst[x] = ROUND_POWER_OF_TWO(dst[x] + sum, 1);
      else
        dst[x] = sum;

      x_qn += x_step_qn;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

void av1_convolve_vert_c(const uint8_t *src, int src_stride, uint8_t *dst,
                         int dst_stride, int w, int h,
                         const InterpFilterParams filter_params,
                         const int subpel_y_q4, int y_step_q4,
                         ConvolveParams *conv_params) {
  int filter_size = filter_params.taps;
  assert(conv_params->round == CONVOLVE_OPT_ROUND);
  src -= src_stride * (filter_size / 2 - 1);
  for (int x = 0; x < w; ++x) {
    int y_q4 = subpel_y_q4;
    for (int y = 0; y < h; ++y) {
      const uint8_t *const src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
      const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
          filter_params, y_q4 & SUBPEL_MASK);
      int sum = 0;
      for (int k = 0; k < filter_size; ++k)
        sum += src_y[k * src_stride] * y_filter[k];

      sum = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
      if (conv_params->do_average)
        dst[y * dst_stride] = ROUND_POWER_OF_TWO(dst[y * dst_stride] + sum, 1);
      else
        dst[y * dst_stride] = sum;

      y_q4 += y_step_q4;
    }
    ++src;
    ++dst;
  }
}

static void convolve_vert_scale(const uint8_t *src, int src_stride,
                                uint8_t *dst, int dst_stride, int w, int h,
                                const InterpFilterParams filter_params,
                                const int subpel_y_qn, int y_step_qn,
                                ConvolveParams *conv_params) {
  int filter_size = filter_params.taps;
  assert(conv_params->round == CONVOLVE_OPT_ROUND);
  src -= src_stride * (filter_size / 2 - 1);
  for (int x = 0; x < w; ++x) {
    int y_qn = subpel_y_qn;
    for (int y = 0; y < h; ++y) {
      const uint8_t *const src_y =
          &src[(y_qn >> SCALE_SUBPEL_BITS) * src_stride];
      const int y_filter_idx = (y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
      assert(y_filter_idx < SUBPEL_SHIFTS);
      const int16_t *y_filter =
          av1_get_interp_filter_subpel_kernel(filter_params, y_filter_idx);
      int sum = 0;
      for (int k = 0; k < filter_size; ++k)
        sum += src_y[k * src_stride] * y_filter[k];

      sum = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
      if (conv_params->do_average)
        dst[y * dst_stride] = ROUND_POWER_OF_TWO(dst[y * dst_stride] + sum, 1);
      else
        dst[y * dst_stride] = sum;

      y_qn += y_step_qn;
    }
    ++src;
    ++dst;
  }
}

static void convolve_copy(const uint8_t *src, int src_stride, uint8_t *dst,
                          int dst_stride, int w, int h,
                          ConvolveParams *conv_params) {
  assert(conv_params->round == CONVOLVE_OPT_ROUND);
  if (conv_params->do_average == 0) {
    for (int r = 0; r < h; ++r) {
      memcpy(dst, src, w);
      src += src_stride;
      dst += dst_stride;
    }
  } else {
    for (int r = 0; r < h; ++r) {
      for (int c = 0; c < w; ++c) {
        dst[c] = clip_pixel(ROUND_POWER_OF_TWO(dst[c] + src[c], 1));
      }
      src += src_stride;
      dst += dst_stride;
    }
  }
}

static void convolve_horiz_facade(const uint8_t *src, int src_stride,
                                  uint8_t *dst, int dst_stride, int w, int h,
                                  const InterpFilterParams filter_params,
                                  const int subpel_x_q4, int x_step_q4,
                                  ConvolveParams *conv_params) {
  assert(conv_params->round == CONVOLVE_OPT_ROUND);
  if (filter_params.taps == SUBPEL_TAPS) {
    const int16_t *filter_x =
        av1_get_interp_filter_subpel_kernel(filter_params, subpel_x_q4);
    if (conv_params->do_average == 0)
      aom_convolve8_horiz(src, src_stride, dst, dst_stride, filter_x, x_step_q4,
                          NULL, -1, w, h);
    else
      aom_convolve8_avg_horiz(src, src_stride, dst, dst_stride, filter_x,
                              x_step_q4, NULL, -1, w, h);
  } else {
    av1_convolve_horiz(src, src_stride, dst, dst_stride, w, h, filter_params,
                       subpel_x_q4, x_step_q4, conv_params);
  }
}

static void convolve_horiz_facade_c(const uint8_t *src, int src_stride,
                                    uint8_t *dst, int dst_stride, int w, int h,
                                    const InterpFilterParams filter_params,
                                    const int subpel_x_q4, int x_step_q4,
                                    ConvolveParams *conv_params) {
  assert(conv_params->round == CONVOLVE_OPT_ROUND);
  if (filter_params.taps == SUBPEL_TAPS) {
    const int16_t *filter_x =
        av1_get_interp_filter_subpel_kernel(filter_params, subpel_x_q4);
    if (conv_params->do_average == 0)
      aom_convolve8_horiz_c(src, src_stride, dst, dst_stride, filter_x,
                            x_step_q4, NULL, -1, w, h);
    else
      aom_convolve8_avg_horiz_c(src, src_stride, dst, dst_stride, filter_x,
                                x_step_q4, NULL, -1, w, h);
  } else {
    av1_convolve_horiz_c(src, src_stride, dst, dst_stride, w, h, filter_params,
                         subpel_x_q4, x_step_q4, conv_params);
  }
}

void av1_convolve_horiz_facade_scale(const uint8_t *src, int src_stride,
                                     uint8_t *dst, int dst_stride, int w, int h,
                                     const InterpFilterParams filter_params,
                                     const int subpel_x_qn, int x_step_qn,
                                     ConvolveParams *conv_params) {
  assert(conv_params->round == CONVOLVE_OPT_ROUND);
  if (filter_params.taps == SUBPEL_TAPS) {
    const int16_t *filter_x = av1_get_interp_filter_subpel_kernel(
        filter_params, subpel_x_qn >> SCALE_EXTRA_BITS);
    if (conv_params->do_average == 0)
      aom_convolve8_horiz_scale(src, src_stride, dst, dst_stride, filter_x,
                                subpel_x_qn, x_step_qn, NULL, 0, -1, w, h);
    else
      aom_convolve8_avg_horiz_scale(src, src_stride, dst, dst_stride, filter_x,
                                    subpel_x_qn, x_step_qn, NULL, 0, -1, w, h);
  } else {
    av1_convolve_horiz_scale(src, src_stride, dst, dst_stride, w, h,
                             filter_params, subpel_x_qn, x_step_qn,
                             conv_params);
  }
}

static void convolve_vert_facade(const uint8_t *src, int src_stride,
                                 uint8_t *dst, int dst_stride, int w, int h,
                                 const InterpFilterParams filter_params,
                                 const int subpel_y_q4, int y_step_q4,
                                 ConvolveParams *conv_params) {
  assert(conv_params->round == CONVOLVE_OPT_ROUND);
  if (filter_params.taps == SUBPEL_TAPS) {
    const int16_t *filter_y =
        av1_get_interp_filter_subpel_kernel(filter_params, subpel_y_q4);
    if (conv_params->do_average == 0) {
      aom_convolve8_vert(src, src_stride, dst, dst_stride, NULL, -1, filter_y,
                         y_step_q4, w, h);
    } else {
      aom_convolve8_avg_vert(src, src_stride, dst, dst_stride, NULL, -1,
                             filter_y, y_step_q4, w, h);
    }
  } else {
    av1_convolve_vert(src, src_stride, dst, dst_stride, w, h, filter_params,
                      subpel_y_q4, y_step_q4, conv_params);
  }
}

static void convolve_vert_facade_c(const uint8_t *src, int src_stride,
                                   uint8_t *dst, int dst_stride, int w, int h,
                                   const InterpFilterParams filter_params,
                                   const int subpel_y_q4, int y_step_q4,
                                   ConvolveParams *conv_params) {
  assert(conv_params->round == CONVOLVE_OPT_ROUND);
  if (filter_params.taps == SUBPEL_TAPS) {
    const int16_t *filter_y =
        av1_get_interp_filter_subpel_kernel(filter_params, subpel_y_q4);
    if (conv_params->do_average == 0) {
      aom_convolve8_vert_c(src, src_stride, dst, dst_stride, NULL, -1, filter_y,
                           y_step_q4, w, h);
    } else {
      aom_convolve8_avg_vert_c(src, src_stride, dst, dst_stride, NULL, -1,
                               filter_y, y_step_q4, w, h);
    }
  } else {
    av1_convolve_vert_c(src, src_stride, dst, dst_stride, w, h, filter_params,
                        subpel_y_q4, y_step_q4, conv_params);
  }
}

void av1_convolve_vert_facade_scale(const uint8_t *src, int src_stride,
                                    uint8_t *dst, int dst_stride, int w, int h,
                                    const InterpFilterParams filter_params,
                                    const int subpel_y_qn, int y_step_qn,
                                    ConvolveParams *conv_params) {
  assert(conv_params->round == CONVOLVE_OPT_ROUND);
  if (filter_params.taps == SUBPEL_TAPS) {
    const int16_t *filter_y = av1_get_interp_filter_subpel_kernel(
        filter_params, subpel_y_qn >> SCALE_EXTRA_BITS);
    if (conv_params->do_average == 0) {
      aom_convolve8_vert_scale(src, src_stride, dst, dst_stride, NULL, 0, -1,
                               filter_y, subpel_y_qn, y_step_qn, w, h);
    } else {
      aom_convolve8_avg_vert_scale(src, src_stride, dst, dst_stride, NULL, 0,
                                   -1, filter_y, subpel_y_qn, y_step_qn, w, h);
    }
  } else {
    convolve_vert_scale(src, src_stride, dst, dst_stride, w, h, filter_params,
                        subpel_y_qn, y_step_qn, conv_params);
  }
}

void av1_convolve_rounding_c(const int32_t *src, int src_stride, uint8_t *dst,
                             int dst_stride, int w, int h, int bits) {
  for (int r = 0; r < h; ++r) {
    for (int c = 0; c < w; ++c) {
      dst[r * dst_stride + c] =
          clip_pixel(ROUND_POWER_OF_TWO(src[r * src_stride + c], bits));
    }
  }
}

/* When convolve-round is enabled and compound-round is disabled, we use a
   high-precision convolve filter.
   Note: For notes on hardware implementations, including the required
   bit widths for various intermediate values, see the comments above
   av1_warp_affine_c.
*/
void av1_convolve_2d_c(const uint8_t *src, int src_stride, uint8_t *dst0,
                       int dst_stride0, int w, int h,
                       InterpFilterParams *filter_params_x,
                       InterpFilterParams *filter_params_y,
                       const int subpel_x_q4, const int subpel_y_q4,
                       ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  int32_t im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE];
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = w;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const int bd = 8;
  (void)dst0;
  (void)dst_stride0;

  // horizontal filter
  const uint8_t *src_horiz = src - fo_vert * src_stride;
  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
  for (int y = 0; y < im_h; ++y) {
    for (int x = 0; x < w; ++x) {
      int32_t sum = (1 << (bd + FILTER_BITS - 1));
      for (int k = 0; k < filter_params_x->taps; ++k) {
        sum += x_filter[k] * src_horiz[y * src_stride + x - fo_horiz + k];
      }
      assert(0 <= sum && sum < (1 << (bd + FILTER_BITS + 1)));
      im_block[y * im_stride + x] =
          ROUND_POWER_OF_TWO(sum, conv_params->round_0);
    }
  }

  // vertical filter
  int32_t *src_vert = im_block + fo_vert * im_stride;
  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE sum = 1 << offset_bits;
      for (int k = 0; k < filter_params_y->taps; ++k) {
        sum += y_filter[k] * src_vert[(y - fo_vert + k) * im_stride + x];
      }
      assert(0 <= sum && sum < (1 << (offset_bits + 2)));
      CONV_BUF_TYPE res = ROUND_POWER_OF_TWO(sum, conv_params->round_1) -
                          ((1 << (offset_bits - conv_params->round_1)) +
                           (1 << (offset_bits - conv_params->round_1 - 1)));
      if (conv_params->do_average)
        dst[y * dst_stride + x] += res;
      else
        dst[y * dst_stride + x] = res;
    }
  }
}

void av1_convolve_y_c(const uint8_t *src, int src_stride, uint8_t *dst0,
                      int dst_stride0, int w, int h,
                      InterpFilterParams *filter_params_x,
                      InterpFilterParams *filter_params_y,
                      const int subpel_x_q4, const int subpel_y_q4,
                      ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int bits = FILTER_BITS - conv_params->round_0;
  (void)filter_params_x;
  (void)subpel_x_q4;
  (void)dst0;
  (void)dst_stride0;

  // vertical filter
  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE res = 0;
      for (int k = 0; k < filter_params_y->taps; ++k) {
        res += y_filter[k] * src[(y - fo_vert + k) * src_stride + x];
      }
      res *= (1 << bits);
      res = ROUND_POWER_OF_TWO(res, conv_params->round_1);
      if (conv_params->do_average)
        dst[y * dst_stride + x] += res;
      else
        dst[y * dst_stride + x] = res;
    }
  }
}

void av1_convolve_x_c(const uint8_t *src, int src_stride, uint8_t *dst0,
                      int dst_stride0, int w, int h,
                      InterpFilterParams *filter_params_x,
                      InterpFilterParams *filter_params_y,
                      const int subpel_x_q4, const int subpel_y_q4,
                      ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const int bits = FILTER_BITS - conv_params->round_1;
  (void)filter_params_y;
  (void)subpel_y_q4;
  (void)dst0;
  (void)dst_stride0;

  // horizontal filter
  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE res = 0;
      for (int k = 0; k < filter_params_x->taps; ++k) {
        res += x_filter[k] * src[y * src_stride + x - fo_horiz + k];
      }
      res = (1 << bits) * ROUND_POWER_OF_TWO(res, conv_params->round_0);
      if (conv_params->do_average)
        dst[y * dst_stride + x] += res;
      else
        dst[y * dst_stride + x] = res;
    }
  }
}

void av1_convolve_2d_copy_c(const uint8_t *src, int src_stride, uint8_t *dst0,
                            int dst_stride0, int w, int h,
                            InterpFilterParams *filter_params_x,
                            InterpFilterParams *filter_params_y,
                            const int subpel_x_q4, const int subpel_y_q4,
                            ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  const int bits =
      FILTER_BITS * 2 - conv_params->round_1 - conv_params->round_0;

  (void)filter_params_x;
  (void)filter_params_y;
  (void)subpel_x_q4;
  (void)subpel_y_q4;
  (void)dst0;
  (void)dst_stride0;

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE res = src[y * src_stride + x] << bits;
      if (conv_params->do_average)
        dst[y * dst_stride + x] += res;
      else
        dst[y * dst_stride + x] = res;
    }
  }
}

void av1_convolve_2d_sr_c(const uint8_t *src, int src_stride, uint8_t *dst,
                          int dst_stride, int w, int h,
                          InterpFilterParams *filter_params_x,
                          InterpFilterParams *filter_params_y,
                          const int subpel_x_q4, const int subpel_y_q4,
                          ConvolveParams *conv_params) {
  int32_t im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE];
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = w;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const int bd = 8;
  const int bits =
      FILTER_BITS * 2 - conv_params->round_0 - conv_params->round_1;

  // horizontal filter
  const uint8_t *src_horiz = src - fo_vert * src_stride;
  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
  for (int y = 0; y < im_h; ++y) {
    for (int x = 0; x < w; ++x) {
      int32_t sum = (1 << (bd + FILTER_BITS - 1));
      for (int k = 0; k < filter_params_x->taps; ++k) {
        sum += x_filter[k] * src_horiz[y * src_stride + x - fo_horiz + k];
      }
      assert(0 <= sum && sum < (1 << (bd + FILTER_BITS + 1)));
      im_block[y * im_stride + x] =
          ROUND_POWER_OF_TWO(sum, conv_params->round_0);
    }
  }

  // vertical filter
  int32_t *src_vert = im_block + fo_vert * im_stride;
  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE sum = 1 << offset_bits;
      for (int k = 0; k < filter_params_y->taps; ++k) {
        sum += y_filter[k] * src_vert[(y - fo_vert + k) * im_stride + x];
      }
      assert(0 <= sum && sum < (1 << (offset_bits + 2)));
      CONV_BUF_TYPE res = ROUND_POWER_OF_TWO(sum, conv_params->round_1) -
                          ((1 << (offset_bits - conv_params->round_1)) +
                           (1 << (offset_bits - conv_params->round_1 - 1)));
      dst[y * dst_stride + x] = clip_pixel(ROUND_POWER_OF_TWO(res, bits));
    }
  }
}

void av1_convolve_y_sr_c(const uint8_t *src, int src_stride, uint8_t *dst,
                         int dst_stride, int w, int h,
                         InterpFilterParams *filter_params_x,
                         InterpFilterParams *filter_params_y,
                         const int subpel_x_q4, const int subpel_y_q4,
                         ConvolveParams *conv_params) {
  const int fo_vert = filter_params_y->taps / 2 - 1;
  (void)filter_params_x;
  (void)subpel_x_q4;
  (void)conv_params;

  // vertical filter
  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE res = 0;
      for (int k = 0; k < filter_params_y->taps; ++k) {
        res += y_filter[k] * src[(y - fo_vert + k) * src_stride + x];
      }
      dst[y * dst_stride + x] =
          clip_pixel(ROUND_POWER_OF_TWO(res, FILTER_BITS));
    }
  }
}

void av1_convolve_x_sr_c(const uint8_t *src, int src_stride, uint8_t *dst,
                         int dst_stride, int w, int h,
                         InterpFilterParams *filter_params_x,
                         InterpFilterParams *filter_params_y,
                         const int subpel_x_q4, const int subpel_y_q4,
                         ConvolveParams *conv_params) {
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const int bits = FILTER_BITS - conv_params->round_0;
  (void)filter_params_y;
  (void)subpel_y_q4;
  (void)conv_params;

  // horizontal filter
  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE res = 0;
      for (int k = 0; k < filter_params_x->taps; ++k) {
        res += x_filter[k] * src[y * src_stride + x - fo_horiz + k];
      }
      res = ROUND_POWER_OF_TWO(res, conv_params->round_0);
      dst[y * dst_stride + x] = clip_pixel(ROUND_POWER_OF_TWO(res, bits));
    }
  }
}

void av1_convolve_2d_copy_sr_c(const uint8_t *src, int src_stride, uint8_t *dst,
                               int dst_stride, int w, int h,
                               InterpFilterParams *filter_params_x,
                               InterpFilterParams *filter_params_y,
                               const int subpel_x_q4, const int subpel_y_q4,
                               ConvolveParams *conv_params) {
  (void)filter_params_x;
  (void)filter_params_y;
  (void)subpel_x_q4;
  (void)subpel_y_q4;
  (void)conv_params;

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      dst[y * dst_stride + x] = src[y * src_stride + x];
    }
  }
}

#if CONFIG_JNT_COMP
void av1_jnt_convolve_2d_c(const uint8_t *src, int src_stride, uint8_t *dst0,
                           int dst_stride0, int w, int h,
                           InterpFilterParams *filter_params_x,
                           InterpFilterParams *filter_params_y,
                           const int subpel_x_q4, const int subpel_y_q4,
                           ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  int32_t im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE];
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = w;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const int bd = 8;
  (void)dst0;
  (void)dst_stride0;

  // horizontal filter
  const uint8_t *src_horiz = src - fo_vert * src_stride;
  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
  for (int y = 0; y < im_h; ++y) {
    for (int x = 0; x < w; ++x) {
      int32_t sum = (1 << (bd + FILTER_BITS - 1));
      for (int k = 0; k < filter_params_x->taps; ++k) {
        sum += x_filter[k] * src_horiz[y * src_stride + x - fo_horiz + k];
      }
      assert(0 <= sum && sum < (1 << (bd + FILTER_BITS + 1)));
      im_block[y * im_stride + x] =
          ROUND_POWER_OF_TWO(sum, conv_params->round_0);
    }
  }

  // vertical filter
  int32_t *src_vert = im_block + fo_vert * im_stride;
  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE sum = 1 << offset_bits;
      for (int k = 0; k < filter_params_y->taps; ++k) {
        sum += y_filter[k] * src_vert[(y - fo_vert + k) * im_stride + x];
      }
      assert(0 <= sum && sum < (1 << (offset_bits + 2)));
      CONV_BUF_TYPE res = ROUND_POWER_OF_TWO(sum, conv_params->round_1) -
                          ((1 << (offset_bits - conv_params->round_1)) +
                           (1 << (offset_bits - conv_params->round_1 - 1)));
      if (conv_params->use_jnt_comp_avg) {
        if (conv_params->do_average) {
          dst[y * dst_stride + x] += res * conv_params->bck_offset;
        } else {
          dst[y * dst_stride + x] = res * conv_params->fwd_offset;
        }
      } else {
        if (conv_params->do_average)
          dst[y * dst_stride + x] += res;
        else
          dst[y * dst_stride + x] = res;
      }
    }
  }
}

void av1_jnt_convolve_y_c(const uint8_t *src, int src_stride, uint8_t *dst0,
                          int dst_stride0, int w, int h,
                          InterpFilterParams *filter_params_x,
                          InterpFilterParams *filter_params_y,
                          const int subpel_x_q4, const int subpel_y_q4,
                          ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int bits = FILTER_BITS - conv_params->round_0;
  (void)filter_params_x;
  (void)subpel_x_q4;
  (void)dst0;
  (void)dst_stride0;

  // vertical filter
  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE res = 0;
      for (int k = 0; k < filter_params_y->taps; ++k) {
        res += y_filter[k] * src[(y - fo_vert + k) * src_stride + x];
      }
      res *= (1 << bits);
      res = ROUND_POWER_OF_TWO(res, conv_params->round_1);
      if (conv_params->use_jnt_comp_avg) {
        if (conv_params->do_average) {
          dst[y * dst_stride + x] += res * conv_params->bck_offset;
        } else {
          dst[y * dst_stride + x] = res * conv_params->fwd_offset;
        }
      } else {
        if (conv_params->do_average)
          dst[y * dst_stride + x] += res;
        else
          dst[y * dst_stride + x] = res;
      }
    }
  }
}

void av1_jnt_convolve_x_c(const uint8_t *src, int src_stride, uint8_t *dst0,
                          int dst_stride0, int w, int h,
                          InterpFilterParams *filter_params_x,
                          InterpFilterParams *filter_params_y,
                          const int subpel_x_q4, const int subpel_y_q4,
                          ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const int bits = FILTER_BITS - conv_params->round_1;
  (void)filter_params_y;
  (void)subpel_y_q4;
  (void)dst0;
  (void)dst_stride0;

  // horizontal filter
  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE res = 0;
      for (int k = 0; k < filter_params_x->taps; ++k) {
        res += x_filter[k] * src[y * src_stride + x - fo_horiz + k];
      }
      res = (1 << bits) * ROUND_POWER_OF_TWO(res, conv_params->round_0);
      if (conv_params->use_jnt_comp_avg) {
        if (conv_params->do_average) {
          dst[y * dst_stride + x] += res * conv_params->bck_offset;
        } else {
          dst[y * dst_stride + x] = res * conv_params->fwd_offset;
        }
      } else {
        if (conv_params->do_average)
          dst[y * dst_stride + x] += res;
        else
          dst[y * dst_stride + x] = res;
      }
    }
  }
}

void av1_jnt_convolve_2d_copy_c(const uint8_t *src, int src_stride,
                                uint8_t *dst0, int dst_stride0, int w, int h,
                                InterpFilterParams *filter_params_x,
                                InterpFilterParams *filter_params_y,
                                const int subpel_x_q4, const int subpel_y_q4,
                                ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  const int bits =
      FILTER_BITS * 2 - conv_params->round_1 - conv_params->round_0;

  (void)filter_params_x;
  (void)filter_params_y;
  (void)subpel_x_q4;
  (void)subpel_y_q4;
  (void)dst0;
  (void)dst_stride0;

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE res = src[y * src_stride + x] << bits;
      if (conv_params->use_jnt_comp_avg) {
        if (conv_params->do_average) {
          dst[y * dst_stride + x] += res * conv_params->bck_offset;
        } else {
          dst[y * dst_stride + x] = res * conv_params->fwd_offset;
        }
      } else {
        if (conv_params->do_average)
          dst[y * dst_stride + x] += res;
        else
          dst[y * dst_stride + x] = res;
      }
    }
  }
}
#endif  // CONFIG_JNT_COMP

void av1_convolve_2d_scale_c(const uint8_t *src, int src_stride,
                             CONV_BUF_TYPE *dst, int dst_stride, int w, int h,
                             InterpFilterParams *filter_params_x,
                             InterpFilterParams *filter_params_y,
                             const int subpel_x_qn, const int x_step_qn,
                             const int subpel_y_qn, const int y_step_qn,
                             ConvolveParams *conv_params) {
  int32_t im_block[(2 * MAX_SB_SIZE + MAX_FILTER_TAP) * MAX_SB_SIZE];
  int im_h = (((h - 1) * y_step_qn + subpel_y_qn) >> SCALE_SUBPEL_BITS) +
             filter_params_y->taps;
  int im_stride = w;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const int bd = 8;

  // horizontal filter
  const uint8_t *src_horiz = src - fo_vert * src_stride;
  for (int y = 0; y < im_h; ++y) {
    int x_qn = subpel_x_qn;
    for (int x = 0; x < w; ++x, x_qn += x_step_qn) {
      const uint8_t *const src_x = &src_horiz[(x_qn >> SCALE_SUBPEL_BITS)];
      const int x_filter_idx = (x_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
      assert(x_filter_idx < SUBPEL_SHIFTS);
      const int16_t *x_filter =
          av1_get_interp_filter_subpel_kernel(*filter_params_x, x_filter_idx);
      int32_t sum = (1 << (bd + FILTER_BITS - 1));
      for (int k = 0; k < filter_params_x->taps; ++k) {
        sum += x_filter[k] * src_x[k - fo_horiz];
      }
      assert(0 <= sum && sum < (1 << (bd + FILTER_BITS + 1)));
      im_block[y * im_stride + x] =
          ROUND_POWER_OF_TWO(sum, conv_params->round_0);
    }
    src_horiz += src_stride;
  }

  // vertical filter
  int32_t *src_vert = im_block + fo_vert * im_stride;
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  for (int x = 0; x < w; ++x) {
    int y_qn = subpel_y_qn;
    for (int y = 0; y < h; ++y, y_qn += y_step_qn) {
      const int32_t *src_y = &src_vert[(y_qn >> SCALE_SUBPEL_BITS) * im_stride];
      const int y_filter_idx = (y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
      assert(y_filter_idx < SUBPEL_SHIFTS);
      const int16_t *y_filter =
          av1_get_interp_filter_subpel_kernel(*filter_params_y, y_filter_idx);
      CONV_BUF_TYPE sum = 1 << offset_bits;
      for (int k = 0; k < filter_params_y->taps; ++k) {
        sum += y_filter[k] * src_y[(k - fo_vert) * im_stride];
      }
      assert(0 <= sum && sum < (1 << (offset_bits + 2)));
      CONV_BUF_TYPE res = ROUND_POWER_OF_TWO(sum, conv_params->round_1) -
                          ((1 << (offset_bits - conv_params->round_1)) +
                           (1 << (offset_bits - conv_params->round_1 - 1)));
#if CONFIG_JNT_COMP
      if (conv_params->use_jnt_comp_avg) {
        if (conv_params->do_average) {
          dst[y * dst_stride + x] += res * conv_params->bck_offset;
        } else {
          dst[y * dst_stride + x] = res * conv_params->fwd_offset;
        }
      } else {
        if (conv_params->do_average)
          dst[y * dst_stride + x] += res;
        else
          dst[y * dst_stride + x] = res;
      }
#else
      if (conv_params->do_average)
        dst[y * dst_stride + x] += res;
      else
        dst[y * dst_stride + x] = res;
#endif  // CONFIG_JNT_COMP
    }
    src_vert++;
  }
}

void av1_convolve_2d_facade(const uint8_t *src, int src_stride, uint8_t *dst,
                            int dst_stride, int w, int h,
                            InterpFilters interp_filters, const int subpel_x_q4,
                            int x_step_q4, const int subpel_y_q4, int y_step_q4,
                            int scaled, ConvolveParams *conv_params,
                            const struct scale_factors *sf) {
  (void)x_step_q4;
  (void)y_step_q4;
  (void)dst;
  (void)dst_stride;

  InterpFilterParams filter_params_x, filter_params_y;
#if CONFIG_SHORT_FILTER
  av1_get_convolve_filter_params(interp_filters, &filter_params_x,
                                 &filter_params_y, w, h);
#else
  av1_get_convolve_filter_params(interp_filters, &filter_params_x,
                                 &filter_params_y);
#endif

  if (scaled)
    av1_convolve_2d_scale(src, src_stride, conv_params->dst,
                          conv_params->dst_stride, w, h, &filter_params_x,
                          &filter_params_y, subpel_x_q4, x_step_q4, subpel_y_q4,
                          y_step_q4, conv_params);
  else
    sf->convolve[subpel_x_q4 != 0][subpel_y_q4 != 0][conv_params->is_compound](
        src, src_stride, dst, dst_stride, w, h, &filter_params_x,
        &filter_params_y, subpel_x_q4, subpel_y_q4, conv_params);
}

void av1_highbd_convolve_rounding_c(const int32_t *src, int src_stride,
                                    uint8_t *dst8, int dst_stride, int w, int h,
                                    int bits, int bd) {
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  for (int r = 0; r < h; ++r) {
    for (int c = 0; c < w; ++c) {
      dst[r * dst_stride + c] = clip_pixel_highbd(
          ROUND_POWER_OF_TWO(src[r * src_stride + c], bits), bd);
    }
  }
}

void av1_highbd_convolve_2d_c(const uint16_t *src, int src_stride,
                              CONV_BUF_TYPE *dst, int dst_stride, int w, int h,
                              InterpFilterParams *filter_params_x,
                              InterpFilterParams *filter_params_y,
                              const int subpel_x_q4, const int subpel_y_q4,
                              ConvolveParams *conv_params, int bd) {
  int32_t im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE];
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = w;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;

  // horizontal filter
  const uint16_t *src_horiz = src - fo_vert * src_stride;
  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
  for (int y = 0; y < im_h; ++y) {
    for (int x = 0; x < w; ++x) {
      int32_t sum = (1 << (bd + FILTER_BITS - 1));
      for (int k = 0; k < filter_params_x->taps; ++k) {
        sum += x_filter[k] * src_horiz[y * src_stride + x - fo_horiz + k];
      }
      assert(0 <= sum && sum < (1 << (bd + FILTER_BITS + 1)));
      (void)bd;
      im_block[y * im_stride + x] =
          ROUND_POWER_OF_TWO(sum, conv_params->round_0);
    }
  }

  // vertical filter
  int32_t *src_vert = im_block + fo_vert * im_stride;
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE sum = 1 << offset_bits;
      for (int k = 0; k < filter_params_y->taps; ++k) {
        sum += y_filter[k] * src_vert[(y - fo_vert + k) * im_stride + x];
      }
      assert(0 <= sum && sum < (1 << (offset_bits + 2)));
      CONV_BUF_TYPE res = ROUND_POWER_OF_TWO(sum, conv_params->round_1) -
                          ((1 << (offset_bits - conv_params->round_1)) +
                           (1 << (offset_bits - conv_params->round_1 - 1)));
      if (conv_params->do_average)
        dst[y * dst_stride + x] += res;
      else
        dst[y * dst_stride + x] = res;
    }
  }
}

#if CONFIG_JNT_COMP
void av1_highbd_jnt_convolve_2d_c(const uint16_t *src, int src_stride,
                                  CONV_BUF_TYPE *dst, int dst_stride, int w,
                                  int h, InterpFilterParams *filter_params_x,
                                  InterpFilterParams *filter_params_y,
                                  const int subpel_x_q4, const int subpel_y_q4,
                                  ConvolveParams *conv_params, int bd) {
  int x, y, k;
  int32_t im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE];
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = w;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;

  // horizontal filter
  const uint16_t *src_horiz = src - fo_vert * src_stride;
  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
  for (y = 0; y < im_h; ++y) {
    for (x = 0; x < w; ++x) {
      int32_t sum = (1 << (bd + FILTER_BITS - 1));
      for (k = 0; k < filter_params_x->taps; ++k) {
        sum += x_filter[k] * src_horiz[y * src_stride + x - fo_horiz + k];
      }
      assert(0 <= sum && sum < (1 << (bd + FILTER_BITS + 1)));
      (void)bd;
      im_block[y * im_stride + x] =
          ROUND_POWER_OF_TWO(sum, conv_params->round_0);
    }
  }

  // vertical filter
  int32_t *src_vert = im_block + fo_vert * im_stride;
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
  for (y = 0; y < h; ++y) {
    for (x = 0; x < w; ++x) {
      CONV_BUF_TYPE sum = 1 << offset_bits;
      for (k = 0; k < filter_params_y->taps; ++k) {
        sum += y_filter[k] * src_vert[(y - fo_vert + k) * im_stride + x];
      }
      assert(0 <= sum && sum < (1 << (offset_bits + 2)));
      CONV_BUF_TYPE res = ROUND_POWER_OF_TWO(sum, conv_params->round_1) -
                          ((1 << (offset_bits - conv_params->round_1)) +
                           (1 << (offset_bits - conv_params->round_1 - 1)));
      if (conv_params->use_jnt_comp_avg) {
        if (conv_params->do_average) {
          dst[y * dst_stride + x] += res * conv_params->bck_offset;
        } else {
          dst[y * dst_stride + x] = res * conv_params->fwd_offset;
        }
      } else {
        if (conv_params->do_average)
          dst[y * dst_stride + x] += res;
        else
          dst[y * dst_stride + x] = res;
      }
    }
  }
}
#endif  // CONFIG_JNT_COMP

void av1_highbd_convolve_2d_scale_c(const uint16_t *src, int src_stride,
                                    CONV_BUF_TYPE *dst, int dst_stride, int w,
                                    int h, InterpFilterParams *filter_params_x,
                                    InterpFilterParams *filter_params_y,
                                    const int subpel_x_qn, const int x_step_qn,
                                    const int subpel_y_qn, const int y_step_qn,
                                    ConvolveParams *conv_params, int bd) {
  int32_t im_block[(2 * MAX_SB_SIZE + MAX_FILTER_TAP) * MAX_SB_SIZE];
  int im_h = (((h - 1) * y_step_qn + subpel_y_qn) >> SCALE_SUBPEL_BITS) +
             filter_params_y->taps;
  int im_stride = w;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;

  // horizontal filter
  const uint16_t *src_horiz = src - fo_vert * src_stride;
  for (int y = 0; y < im_h; ++y) {
    int x_qn = subpel_x_qn;
    for (int x = 0; x < w; ++x, x_qn += x_step_qn) {
      const uint16_t *const src_x = &src_horiz[(x_qn >> SCALE_SUBPEL_BITS)];
      const int x_filter_idx = (x_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
      assert(x_filter_idx < SUBPEL_SHIFTS);
      const int16_t *x_filter =
          av1_get_interp_filter_subpel_kernel(*filter_params_x, x_filter_idx);
      int32_t sum = (1 << (bd + FILTER_BITS - 1));
      for (int k = 0; k < filter_params_x->taps; ++k) {
        sum += x_filter[k] * src_x[k - fo_horiz];
      }
      assert(0 <= sum && sum < (1 << (bd + FILTER_BITS + 1)));
      im_block[y * im_stride + x] =
          ROUND_POWER_OF_TWO(sum, conv_params->round_0);
    }
    src_horiz += src_stride;
  }

  // vertical filter
  int32_t *src_vert = im_block + fo_vert * im_stride;
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  for (int x = 0; x < w; ++x) {
    int y_qn = subpel_y_qn;
    for (int y = 0; y < h; ++y, y_qn += y_step_qn) {
      const int32_t *src_y = &src_vert[(y_qn >> SCALE_SUBPEL_BITS) * im_stride];
      const int y_filter_idx = (y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
      assert(y_filter_idx < SUBPEL_SHIFTS);
      const int16_t *y_filter =
          av1_get_interp_filter_subpel_kernel(*filter_params_y, y_filter_idx);
      CONV_BUF_TYPE sum = 1 << offset_bits;
      for (int k = 0; k < filter_params_y->taps; ++k) {
        sum += y_filter[k] * src_y[(k - fo_vert) * im_stride];
      }
      assert(0 <= sum && sum < (1 << (offset_bits + 2)));
      CONV_BUF_TYPE res = ROUND_POWER_OF_TWO(sum, conv_params->round_1) -
                          ((1 << (offset_bits - conv_params->round_1)) +
                           (1 << (offset_bits - conv_params->round_1 - 1)));
#if CONFIG_JNT_COMP
      if (conv_params->use_jnt_comp_avg) {
        if (conv_params->do_average) {
          dst[y * dst_stride + x] += res * conv_params->bck_offset;
        } else {
          dst[y * dst_stride + x] = res * conv_params->fwd_offset;
        }
      } else {
        if (conv_params->do_average)
          dst[y * dst_stride + x] += res;
        else
          dst[y * dst_stride + x] = res;
      }
#else
      if (conv_params->do_average)
        dst[y * dst_stride + x] += res;
      else
        dst[y * dst_stride + x] = res;
#endif  // CONFIG_JNT_COMP
    }
    src_vert++;
  }
}

void av1_highbd_convolve_2d_facade(const uint8_t *src8, int src_stride,
                                   uint8_t *dst, int dst_stride, int w, int h,
                                   InterpFilters interp_filters,
                                   const int subpel_x_q4, int x_step_q4,
                                   const int subpel_y_q4, int y_step_q4,
                                   int scaled, ConvolveParams *conv_params,
                                   int bd) {
  (void)dst;
  (void)dst_stride;

  InterpFilterParams filter_params_x, filter_params_y;
#if CONFIG_SHORT_FILTER
  av1_get_convolve_filter_params(interp_filters, &filter_params_x,
                                 &filter_params_y, w, h);
#else
  av1_get_convolve_filter_params(interp_filters, &filter_params_x,
                                 &filter_params_y);
#endif

  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  if (filter_params_y.taps < filter_params_x.taps) {
    uint16_t tr_src[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) *
                    (MAX_SB_SIZE + MAX_FILTER_TAP - 1)];
    int tr_src_stride = MAX_SB_SIZE + MAX_FILTER_TAP - 1;
    CONV_BUF_TYPE tr_dst[MAX_SB_SIZE * MAX_SB_SIZE];
    int tr_dst_stride = MAX_SB_SIZE;
    int fo_vert = filter_params_y.taps / 2 - 1;
    int fo_horiz = filter_params_x.taps / 2 - 1;

    transpose_uint16(
        tr_src, tr_src_stride, src - fo_vert * src_stride - fo_horiz,
        src_stride, w + filter_params_x.taps - 1, h + filter_params_y.taps - 1);
    transpose_int32(tr_dst, tr_dst_stride, conv_params->dst,
                    conv_params->dst_stride, w, h);

// horizontal and vertical parameters are swapped because of the transpose
#if CONFIG_JNT_COMP
    if (scaled)
      av1_highbd_convolve_2d_scale(
          tr_src + fo_horiz * tr_src_stride + fo_vert, tr_src_stride, tr_dst,
          tr_dst_stride, h, w, &filter_params_y, &filter_params_x, subpel_y_q4,
          y_step_q4, subpel_x_q4, x_step_q4, conv_params, bd);
    else
      av1_highbd_jnt_convolve_2d(tr_src + fo_horiz * tr_src_stride + fo_vert,
                                 tr_src_stride, tr_dst, tr_dst_stride, h, w,
                                 &filter_params_y, &filter_params_x,
                                 subpel_y_q4, subpel_x_q4, conv_params, bd);
#else
    if (scaled)
      av1_highbd_convolve_2d_scale(
          tr_src + fo_horiz * tr_src_stride + fo_vert, tr_src_stride, tr_dst,
          tr_dst_stride, h, w, &filter_params_y, &filter_params_x, subpel_y_q4,
          y_step_q4, subpel_x_q4, x_step_q4, conv_params, bd);
    else
      av1_highbd_convolve_2d(tr_src + fo_horiz * tr_src_stride + fo_vert,
                             tr_src_stride, tr_dst, tr_dst_stride, h, w,
                             &filter_params_y, &filter_params_x, subpel_y_q4,
                             subpel_x_q4, conv_params, bd);
#endif  // CONFIG_JNT_COMP
    transpose_int32(conv_params->dst, conv_params->dst_stride, tr_dst,
                    tr_dst_stride, h, w);
  } else {
#if CONFIG_JNT_COMP
    if (scaled)
      av1_highbd_convolve_2d_scale(
          src, src_stride, conv_params->dst, conv_params->dst_stride, w, h,
          &filter_params_x, &filter_params_y, subpel_x_q4, x_step_q4,
          subpel_y_q4, y_step_q4, conv_params, bd);
    else
      av1_highbd_jnt_convolve_2d(src, src_stride, conv_params->dst,
                                 conv_params->dst_stride, w, h,
                                 &filter_params_x, &filter_params_y,
                                 subpel_x_q4, subpel_y_q4, conv_params, bd);
#else
    if (scaled)
      av1_highbd_convolve_2d_scale(
          src, src_stride, conv_params->dst, conv_params->dst_stride, w, h,
          &filter_params_x, &filter_params_y, subpel_x_q4, x_step_q4,
          subpel_y_q4, y_step_q4, conv_params, bd);
    else
      av1_highbd_convolve_2d(src, src_stride, conv_params->dst,
                             conv_params->dst_stride, w, h, &filter_params_x,
                             &filter_params_y, subpel_x_q4, subpel_y_q4,
                             conv_params, bd);
#endif  // CONFIG_JNT_COMP
  }
}

typedef void (*ConvolveFunc)(const uint8_t *src, int src_stride, uint8_t *dst,
                             int dst_stride, int w, int h,
                             const InterpFilterParams filter_params,
                             const int subpel_q4, int step_q4,
                             ConvolveParams *conv_params);

static void convolve_helper(const uint8_t *src, int src_stride, uint8_t *dst,
                            int dst_stride, int w, int h,
                            const InterpFilters interp_filters,
                            const int subpel_x_q4, int x_step_q4,
                            const int subpel_y_q4, int y_step_q4,
                            ConvolveParams *conv_params,
                            ConvolveFunc convolve_horiz,
                            ConvolveFunc convolve_vert) {
  int ignore_horiz = x_step_q4 == SUBPEL_SHIFTS && subpel_x_q4 == 0;
  int ignore_vert = y_step_q4 == SUBPEL_SHIFTS && subpel_y_q4 == 0;

  InterpFilterParams filter_params_x, filter_params_y;
#if CONFIG_SHORT_FILTER
  av1_get_convolve_filter_params(interp_filters, &filter_params_x,
                                 &filter_params_y, w, h);
#else
  av1_get_convolve_filter_params(interp_filters, &filter_params_x,
                                 &filter_params_y);
#endif

  assert(conv_params->round == CONVOLVE_OPT_ROUND);

  assert(w <= MAX_BLOCK_WIDTH);
  assert(h <= MAX_BLOCK_HEIGHT);
  assert(y_step_q4 <= MAX_STEP);
  assert(x_step_q4 <= MAX_STEP);

  if (ignore_horiz && ignore_vert) {
    convolve_copy(src, src_stride, dst, dst_stride, w, h, conv_params);
  } else if (ignore_vert) {
    assert(filter_params_x.taps <= MAX_FILTER_TAP);
    convolve_horiz(src, src_stride, dst, dst_stride, w, h, filter_params_x,
                   subpel_x_q4, x_step_q4, conv_params);
  } else if (ignore_horiz) {
    assert(filter_params_y.taps <= MAX_FILTER_TAP);
    convolve_vert(src, src_stride, dst, dst_stride, w, h, filter_params_y,
                  subpel_y_q4, y_step_q4, conv_params);
  } else {
    // temp's size is set to a 256 aligned value to facilitate SIMD
    // implementation. The value is greater than (maximum possible intermediate
    // height or width) * MAX_SB_SIZE
    DECLARE_ALIGNED(16, uint8_t,
                    temp[((MAX_SB_SIZE * 2 + 16) + 16) * MAX_SB_SIZE]);
    int max_intermediate_size = ((MAX_SB_SIZE * 2 + 16) + 16);
    int filter_size;
    const int temp_stride = MAX_SB_SIZE;
    ConvolveParams temp_conv_params;
    temp_conv_params.ref = 0;
    temp_conv_params.do_average = 0;
    temp_conv_params.round = CONVOLVE_OPT_ROUND;
    filter_size = filter_params_y.taps;
    const int intermediate_height =
        (((h - 1) * y_step_q4 + subpel_y_q4) >> SUBPEL_BITS) + filter_size;
    assert(intermediate_height <= max_intermediate_size);
    (void)max_intermediate_size;

    assert(filter_params_x.taps <= MAX_FILTER_TAP);

    convolve_horiz(src - src_stride * (filter_size / 2 - 1), src_stride, temp,
                   temp_stride, w, intermediate_height, filter_params_x,
                   subpel_x_q4, x_step_q4, &temp_conv_params);

    assert(filter_params_y.taps <= MAX_FILTER_TAP);

    convolve_vert(temp + temp_stride * (filter_size / 2 - 1), temp_stride, dst,
                  dst_stride, w, h, filter_params_y, subpel_y_q4, y_step_q4,
                  conv_params);
  }
}

static void convolve_scale_helper(const uint8_t *src, int src_stride,
                                  uint8_t *dst, int dst_stride, int w, int h,
                                  const InterpFilters interp_filters,
                                  const int subpel_x_qn, int x_step_qn,
                                  const int subpel_y_qn, int y_step_qn,
                                  ConvolveParams *conv_params,
                                  ConvolveFunc convolve_horiz,
                                  ConvolveFunc convolve_vert) {
  int ignore_horiz = x_step_qn == SCALE_SUBPEL_SHIFTS && subpel_x_qn == 0;
  int ignore_vert = y_step_qn == SCALE_SUBPEL_SHIFTS && subpel_y_qn == 0;

  InterpFilterParams filter_params_x, filter_params_y;

#if CONFIG_SHORT_FILTER
  av1_get_convolve_filter_params(interp_filters, &filter_params_x,
                                 &filter_params_y, w, h);
#else
  av1_get_convolve_filter_params(interp_filters, &filter_params_x,
                                 &filter_params_y);
#endif
  assert(conv_params->round == CONVOLVE_OPT_ROUND);

  assert(w <= MAX_BLOCK_WIDTH);
  assert(h <= MAX_BLOCK_HEIGHT);
  assert(y_step_qn <= (MAX_STEP << SCALE_EXTRA_BITS));
  assert(x_step_qn <= (MAX_STEP << SCALE_EXTRA_BITS));

  if (ignore_horiz && ignore_vert) {
    convolve_copy(src, src_stride, dst, dst_stride, w, h, conv_params);
  } else if (ignore_vert) {
    assert(filter_params_x.taps <= MAX_FILTER_TAP);
    convolve_horiz(src, src_stride, dst, dst_stride, w, h, filter_params_x,
                   subpel_x_qn, x_step_qn, conv_params);
  } else if (ignore_horiz) {
    assert(filter_params_y.taps <= MAX_FILTER_TAP);
    convolve_vert(src, src_stride, dst, dst_stride, w, h, filter_params_y,
                  subpel_y_qn, y_step_qn, conv_params);
  } else {
    // temp's size is set to a 256 aligned value to facilitate SIMD
    // implementation. The value is greater than (maximum possible intermediate
    // height or width) * MAX_SB_SIZE
    DECLARE_ALIGNED(16, uint8_t,
                    temp[((MAX_SB_SIZE * 2 + 16) + 16) * MAX_SB_SIZE]);
    int max_intermediate_size = ((MAX_SB_SIZE * 2 + 16) + 16);
    int filter_size;
    const int temp_stride = MAX_SB_SIZE;
    ConvolveParams temp_conv_params;
    temp_conv_params.ref = 0;
    temp_conv_params.do_average = 0;
    temp_conv_params.round = CONVOLVE_OPT_ROUND;
    filter_size = filter_params_y.taps;
    const int intermediate_height =
        (((h - 1) * y_step_qn + subpel_y_qn) >> SCALE_SUBPEL_BITS) +
        filter_size;
    assert(intermediate_height <= max_intermediate_size);
    (void)max_intermediate_size;

    assert(filter_params_x.taps <= MAX_FILTER_TAP);

    convolve_horiz(src - src_stride * (filter_size / 2 - 1), src_stride, temp,
                   temp_stride, w, intermediate_height, filter_params_x,
                   subpel_x_qn, x_step_qn, &temp_conv_params);

    assert(filter_params_y.taps <= MAX_FILTER_TAP);

    convolve_vert(temp + temp_stride * (filter_size / 2 - 1), temp_stride, dst,
                  dst_stride, w, h, filter_params_y, subpel_y_qn, y_step_qn,
                  conv_params);
  }
}

void av1_convolve(const uint8_t *src, int src_stride, uint8_t *dst,
                  int dst_stride, int w, int h, InterpFilters interp_filters,
                  const int subpel_x_q4, int x_step_q4, const int subpel_y_q4,
                  int y_step_q4, ConvolveParams *conv_params) {
  convolve_helper(src, src_stride, dst, dst_stride, w, h, interp_filters,
                  subpel_x_q4, x_step_q4, subpel_y_q4, y_step_q4, conv_params,
                  convolve_horiz_facade, convolve_vert_facade);
}

void av1_convolve_c(const uint8_t *src, int src_stride, uint8_t *dst,
                    int dst_stride, int w, int h, InterpFilters interp_filters,
                    const int subpel_x_q4, int x_step_q4, const int subpel_y_q4,
                    int y_step_q4, ConvolveParams *conv_params) {
  convolve_helper(src, src_stride, dst, dst_stride, w, h, interp_filters,
                  subpel_x_q4, x_step_q4, subpel_y_q4, y_step_q4, conv_params,
                  convolve_horiz_facade_c, convolve_vert_facade_c);
}

void av1_convolve_scale(const uint8_t *src, int src_stride, uint8_t *dst,
                        int dst_stride, int w, int h,
                        InterpFilters interp_filters, const int subpel_x_qn,
                        int x_step_qn, const int subpel_y_qn, int y_step_qn,
                        ConvolveParams *conv_params) {
  convolve_scale_helper(src, src_stride, dst, dst_stride, w, h, interp_filters,
                        subpel_x_qn, x_step_qn, subpel_y_qn, y_step_qn,
                        conv_params, av1_convolve_horiz_facade_scale,
                        av1_convolve_vert_facade_scale);
}

void av1_highbd_convolve_horiz_c(const uint16_t *src, int src_stride,
                                 uint16_t *dst, int dst_stride, int w, int h,
                                 const InterpFilterParams filter_params,
                                 const int subpel_x_q4, int x_step_q4, int avg,
                                 int bd) {
  int filter_size = filter_params.taps;
  src -= filter_size / 2 - 1;
  for (int y = 0; y < h; ++y) {
    int x_q4 = subpel_x_q4;
    for (int x = 0; x < w; ++x) {
      const uint16_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
      const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
          filter_params, x_q4 & SUBPEL_MASK);
      int sum = 0;
      for (int k = 0; k < filter_size; ++k) sum += src_x[k] * x_filter[k];
      if (avg)
        dst[x] = ROUND_POWER_OF_TWO(
            dst[x] +
                clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd),
            1);
      else
        dst[x] = clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd);
      x_q4 += x_step_q4;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

static void highbd_convolve_horiz_scale(const uint16_t *src, int src_stride,
                                        uint16_t *dst, int dst_stride, int w,
                                        int h,
                                        const InterpFilterParams filter_params,
                                        const int subpel_x_qn, int x_step_qn,
                                        int avg, int bd) {
  int filter_size = filter_params.taps;
  src -= filter_size / 2 - 1;
  for (int y = 0; y < h; ++y) {
    int x_qn = subpel_x_qn;
    for (int x = 0; x < w; ++x) {
      const uint16_t *const src_x = &src[x_qn >> SCALE_SUBPEL_BITS];
      const int x_filter_idx = (x_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
      assert(x_filter_idx < SUBPEL_SHIFTS);
      const int16_t *x_filter =
          av1_get_interp_filter_subpel_kernel(filter_params, x_filter_idx);
      int sum = 0;
      for (int k = 0; k < filter_size; ++k) sum += src_x[k] * x_filter[k];
      if (avg)
        dst[x] = ROUND_POWER_OF_TWO(
            dst[x] +
                clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd),
            1);
      else
        dst[x] = clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd);
      x_qn += x_step_qn;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

void av1_highbd_convolve_vert_c(const uint16_t *src, int src_stride,
                                uint16_t *dst, int dst_stride, int w, int h,
                                const InterpFilterParams filter_params,
                                const int subpel_y_q4, int y_step_q4, int avg,
                                int bd) {
  int filter_size = filter_params.taps;
  src -= src_stride * (filter_size / 2 - 1);

  for (int x = 0; x < w; ++x) {
    int y_q4 = subpel_y_q4;
    for (int y = 0; y < h; ++y) {
      const uint16_t *const src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
      const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
          filter_params, y_q4 & SUBPEL_MASK);
      int sum = 0;
      for (int k = 0; k < filter_size; ++k)
        sum += src_y[k * src_stride] * y_filter[k];
      if (avg) {
        dst[y * dst_stride] = ROUND_POWER_OF_TWO(
            dst[y * dst_stride] +
                clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd),
            1);
      } else {
        dst[y * dst_stride] =
            clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd);
      }
      y_q4 += y_step_q4;
    }
    ++src;
    ++dst;
  }
}

static void highbd_convolve_vert_scale(const uint16_t *src, int src_stride,
                                       uint16_t *dst, int dst_stride, int w,
                                       int h,
                                       const InterpFilterParams filter_params,
                                       const int subpel_y_qn, int y_step_qn,
                                       int avg, int bd) {
  int filter_size = filter_params.taps;
  src -= src_stride * (filter_size / 2 - 1);

  for (int x = 0; x < w; ++x) {
    int y_qn = subpel_y_qn;
    for (int y = 0; y < h; ++y) {
      const uint16_t *const src_y =
          &src[(y_qn >> SCALE_SUBPEL_BITS) * src_stride];
      const int y_filter_idx = (y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
      assert(y_filter_idx < SUBPEL_SHIFTS);
      const int16_t *y_filter =
          av1_get_interp_filter_subpel_kernel(filter_params, y_filter_idx);
      int sum = 0;
      for (int k = 0; k < filter_size; ++k)
        sum += src_y[k * src_stride] * y_filter[k];
      if (avg) {
        dst[y * dst_stride] = ROUND_POWER_OF_TWO(
            dst[y * dst_stride] +
                clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd),
            1);
      } else {
        dst[y * dst_stride] =
            clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd);
      }
      y_qn += y_step_qn;
    }
    ++src;
    ++dst;
  }
}

static void highbd_convolve_copy(const uint16_t *src, int src_stride,
                                 uint16_t *dst, int dst_stride, int w, int h,
                                 int avg, int bd) {
  if (avg == 0) {
    for (int r = 0; r < h; ++r) {
      memcpy(dst, src, w * sizeof(*src));
      src += src_stride;
      dst += dst_stride;
    }
  } else {
    for (int r = 0; r < h; ++r) {
      for (int c = 0; c < w; ++c) {
        dst[c] = clip_pixel_highbd(ROUND_POWER_OF_TWO(dst[c] + src[c], 1), bd);
      }
      src += src_stride;
      dst += dst_stride;
    }
  }
}

static void highbd_convolve_horiz_facade(const uint8_t *src8, int src_stride,
                                         uint8_t *dst8, int dst_stride, int w,
                                         int h,
                                         const InterpFilterParams filter_params,
                                         const int subpel_x_q4, int x_step_q4,
                                         int avg, int bd) {
  if (filter_params.taps == SUBPEL_TAPS) {
    const int16_t *filter_x =
        av1_get_interp_filter_subpel_kernel(filter_params, subpel_x_q4);
    if (avg == 0)
      aom_highbd_convolve8_horiz(src8, src_stride, dst8, dst_stride, filter_x,
                                 x_step_q4, NULL, -1, w, h, bd);
    else
      aom_highbd_convolve8_avg_horiz(src8, src_stride, dst8, dst_stride,
                                     filter_x, x_step_q4, NULL, -1, w, h, bd);
  } else {
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
    av1_highbd_convolve_horiz(src, src_stride, dst, dst_stride, w, h,
                              filter_params, subpel_x_q4, x_step_q4, avg, bd);
  }
}

static void highbd_convolve_horiz_facade_scale(
    const uint8_t *src8, int src_stride, uint8_t *dst8, int dst_stride, int w,
    int h, const InterpFilterParams filter_params, const int subpel_x_qn,
    int x_step_qn, int avg, int bd) {
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  // TODO(debargha): Add special functions for filter_params.taps == SUBPEL_TAPS
  // as in the function above.
  highbd_convolve_horiz_scale(src, src_stride, dst, dst_stride, w, h,
                              filter_params, subpel_x_qn, x_step_qn, avg, bd);
}

static void highbd_convolve_vert_facade(const uint8_t *src8, int src_stride,
                                        uint8_t *dst8, int dst_stride, int w,
                                        int h,
                                        const InterpFilterParams filter_params,
                                        const int subpel_y_q4, int y_step_q4,
                                        int avg, int bd) {
  if (filter_params.taps == SUBPEL_TAPS) {
    const int16_t *filter_y =
        av1_get_interp_filter_subpel_kernel(filter_params, subpel_y_q4);
    if (avg == 0) {
      aom_highbd_convolve8_vert(src8, src_stride, dst8, dst_stride, NULL, -1,
                                filter_y, y_step_q4, w, h, bd);
    } else {
      aom_highbd_convolve8_avg_vert(src8, src_stride, dst8, dst_stride, NULL,
                                    -1, filter_y, y_step_q4, w, h, bd);
    }
  } else {
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);

    av1_highbd_convolve_vert(src, src_stride, dst, dst_stride, w, h,
                             filter_params, subpel_y_q4, y_step_q4, avg, bd);
  }
}

static void highbd_convolve_vert_facade_scale(
    const uint8_t *src8, int src_stride, uint8_t *dst8, int dst_stride, int w,
    int h, const InterpFilterParams filter_params, const int subpel_y_qn,
    int y_step_qn, int avg, int bd) {
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  // TODO(debargha): Add special functions for filter_params.taps == SUBPEL_TAPS
  // as in the function above.
  highbd_convolve_vert_scale(src, src_stride, dst, dst_stride, w, h,
                             filter_params, subpel_y_qn, y_step_qn, avg, bd);
}

void av1_highbd_convolve(const uint8_t *src8, int src_stride, uint8_t *dst8,
                         int dst_stride, int w, int h,
                         InterpFilters interp_filters, const int subpel_x_q4,
                         int x_step_q4, const int subpel_y_q4, int y_step_q4,
                         int ref_idx, int bd) {
  int ignore_horiz = x_step_q4 == SUBPEL_SHIFTS && subpel_x_q4 == 0;
  int ignore_vert = y_step_q4 == SUBPEL_SHIFTS && subpel_y_q4 == 0;

  assert(w <= MAX_BLOCK_WIDTH);
  assert(h <= MAX_BLOCK_HEIGHT);
  assert(y_step_q4 <= MAX_STEP);
  assert(x_step_q4 <= MAX_STEP);

  if (ignore_horiz && ignore_vert) {
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
    highbd_convolve_copy(src, src_stride, dst, dst_stride, w, h, ref_idx, bd);
    return;
  }

  InterpFilterParams filter_params_x, filter_params_y;
#if CONFIG_SHORT_FILTER
  av1_get_convolve_filter_params(interp_filters, &filter_params_x,
                                 &filter_params_y, w, h);
#else
  av1_get_convolve_filter_params(interp_filters, &filter_params_x,
                                 &filter_params_y);
#endif

  if (ignore_vert) {
    highbd_convolve_horiz_facade(src8, src_stride, dst8, dst_stride, w, h,
                                 filter_params_x, subpel_x_q4, x_step_q4,
                                 ref_idx, bd);
  } else if (ignore_horiz) {
    highbd_convolve_vert_facade(src8, src_stride, dst8, dst_stride, w, h,
                                filter_params_y, subpel_y_q4, y_step_q4,
                                ref_idx, bd);
  } else {
    // temp's size is set to a 256 aligned value to facilitate SIMD
    // implementation. The value is greater than (maximum possible intermediate
    // height or width) * MAX_SB_SIZE
    DECLARE_ALIGNED(16, uint16_t,
                    temp[((MAX_SB_SIZE * 2 + 16) + 16) * MAX_SB_SIZE]);
    uint8_t *temp8 = CONVERT_TO_BYTEPTR(temp);
    int max_intermediate_size = ((MAX_SB_SIZE * 2 + 16) + 16);
    int filter_size;
    const int temp_stride = MAX_SB_SIZE;
    filter_size = filter_params_y.taps;

    const int intermediate_height =
        (((h - 1) * y_step_q4 + subpel_y_q4) >> SUBPEL_BITS) + filter_size;
    assert(intermediate_height <= max_intermediate_size);
    (void)max_intermediate_size;

    highbd_convolve_horiz_facade(src8 - src_stride * (filter_size / 2 - 1),
                                 src_stride, temp8, temp_stride, w,
                                 intermediate_height, filter_params_x,
                                 subpel_x_q4, x_step_q4, 0, bd);

    filter_size = filter_params_y.taps;
    assert(filter_params_y.taps <= MAX_FILTER_TAP);

    highbd_convolve_vert_facade(
        temp8 + temp_stride * (filter_size / 2 - 1), temp_stride, dst8,
        dst_stride, w, h, filter_params_y, subpel_y_q4, y_step_q4, ref_idx, bd);
  }
}

void av1_highbd_convolve_scale(const uint8_t *src8, int src_stride,
                               uint8_t *dst8, int dst_stride, int w, int h,
                               InterpFilters interp_filters,
                               const int subpel_x_qn, int x_step_qn,
                               const int subpel_y_qn, int y_step_qn,
                               int ref_idx, int bd) {
  int ignore_horiz = x_step_qn == SCALE_SUBPEL_SHIFTS && subpel_x_qn == 0;
  int ignore_vert = y_step_qn == SCALE_SUBPEL_SHIFTS && subpel_y_qn == 0;

  assert(w <= MAX_BLOCK_WIDTH);
  assert(h <= MAX_BLOCK_HEIGHT);
  assert(y_step_qn <= (MAX_STEP << SCALE_EXTRA_BITS));
  assert(x_step_qn <= (MAX_STEP << SCALE_EXTRA_BITS));

  if (ignore_horiz && ignore_vert) {
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
    highbd_convolve_copy(src, src_stride, dst, dst_stride, w, h, ref_idx, bd);
    return;
  }

  InterpFilterParams filter_params_x, filter_params_y;
#if CONFIG_SHORT_FILTER
  av1_get_convolve_filter_params(interp_filters, &filter_params_x,
                                 &filter_params_y, w, h);
#else
  av1_get_convolve_filter_params(interp_filters, &filter_params_x,
                                 &filter_params_y);
#endif

  if (ignore_vert) {
    highbd_convolve_horiz_facade_scale(src8, src_stride, dst8, dst_stride, w, h,
                                       filter_params_x, subpel_x_qn, x_step_qn,
                                       ref_idx, bd);
  } else if (ignore_horiz) {
    highbd_convolve_vert_facade_scale(src8, src_stride, dst8, dst_stride, w, h,
                                      filter_params_y, subpel_y_qn, y_step_qn,
                                      ref_idx, bd);
  } else {
    // temp's size is set to a 256 aligned value to facilitate SIMD
    // implementation. The value is greater than (maximum possible intermediate
    // height or width) * MAX_SB_SIZE
    DECLARE_ALIGNED(16, uint16_t,
                    temp[((MAX_SB_SIZE * 2 + 16) + 16) * MAX_SB_SIZE]);
    uint8_t *temp8 = CONVERT_TO_BYTEPTR(temp);
    int max_intermediate_size = ((MAX_SB_SIZE * 2 + 16) + 16);
    int filter_size;
    const int temp_stride = MAX_SB_SIZE;
    filter_size = filter_params_y.taps;
    const int intermediate_height =
        (((h - 1) * y_step_qn + subpel_y_qn) >> SCALE_SUBPEL_BITS) +
        filter_size;
    assert(intermediate_height <= max_intermediate_size);
    (void)max_intermediate_size;

    highbd_convolve_horiz_facade_scale(
        src8 - src_stride * (filter_size / 2 - 1), src_stride, temp8,
        temp_stride, w, intermediate_height, filter_params_x, subpel_x_qn,
        x_step_qn, 0, bd);

    filter_size = filter_params_y.taps;
    assert(filter_params_y.taps <= MAX_FILTER_TAP);

    highbd_convolve_vert_facade_scale(
        temp8 + temp_stride * (filter_size / 2 - 1), temp_stride, dst8,
        dst_stride, w, h, filter_params_y, subpel_y_qn, y_step_qn, ref_idx, bd);
  }
}
