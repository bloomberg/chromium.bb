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
#include "av1/common/convolve.h"
#include "av1/common/filter.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_ports/mem.h"

#define MAX_BLOCK_WIDTH (MAX_SB_SIZE)
#define MAX_BLOCK_HEIGHT (MAX_SB_SIZE)
#define MAX_STEP (32)
#define MAX_FILTER_TAP (12)

void av1_convolve_horiz_c(const uint8_t *src, int src_stride, uint8_t *dst,
                          int dst_stride, int w, int h,
                          const InterpFilterParams filter_params,
                          const int subpel_x_q4, int x_step_q4, int avg) {
  int x, y;
  int filter_size = filter_params.taps;
  src -= filter_size / 2 - 1;
  for (y = 0; y < h; ++y) {
    int x_q4 = subpel_x_q4;
    for (x = 0; x < w; ++x) {
      const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
      const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
          filter_params, x_q4 & SUBPEL_MASK);
      int k, sum = 0;
      for (k = 0; k < filter_size; ++k) sum += src_x[k] * x_filter[k];
      if (avg) {
        dst[x] = ROUND_POWER_OF_TWO(
            dst[x] + clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS)), 1);
      } else {
        dst[x] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
      }
      x_q4 += x_step_q4;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

void av1_convolve_vert_c(const uint8_t *src, int src_stride, uint8_t *dst,
                         int dst_stride, int w, int h,
                         const InterpFilterParams filter_params,
                         const int subpel_y_q4, int y_step_q4, int avg) {
  int x, y;
  int filter_size = filter_params.taps;
  src -= src_stride * (filter_size / 2 - 1);

  for (x = 0; x < w; ++x) {
    int y_q4 = subpel_y_q4;
    for (y = 0; y < h; ++y) {
      const uint8_t *const src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
      const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
          filter_params, y_q4 & SUBPEL_MASK);
      int k, sum = 0;
      for (k = 0; k < filter_size; ++k)
        sum += src_y[k * src_stride] * y_filter[k];
      if (avg) {
        dst[y * dst_stride] = ROUND_POWER_OF_TWO(
            dst[y * dst_stride] +
                clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS)),
            1);
      } else {
        dst[y * dst_stride] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
      }
      y_q4 += y_step_q4;
    }
    ++src;
    ++dst;
  }
}

static void convolve_copy(const uint8_t *src, int src_stride, uint8_t *dst,
                          int dst_stride, int w, int h, int avg) {
  if (avg == 0) {
    int r;
    for (r = 0; r < h; ++r) {
      memcpy(dst, src, w);
      src += src_stride;
      dst += dst_stride;
    }
  } else {
    int r, c;
    for (r = 0; r < h; ++r) {
      for (c = 0; c < w; ++c) {
        dst[c] = clip_pixel(ROUND_POWER_OF_TWO(dst[c] + src[c], 1));
      }
      src += src_stride;
      dst += dst_stride;
    }
  }
}

void av1_convolve_horiz_facade(const uint8_t *src, int src_stride, uint8_t *dst,
                               int dst_stride, int w, int h,
                               const InterpFilterParams filter_params,
                               const int subpel_x_q4, int x_step_q4, int avg) {
  if (filter_params.taps == SUBPEL_TAPS) {
    const int16_t *filter_x =
        av1_get_interp_filter_subpel_kernel(filter_params, subpel_x_q4);
    if (avg == 0)
      aom_convolve8_horiz(src, src_stride, dst, dst_stride, filter_x, x_step_q4,
                          NULL, -1, w, h);
    else
      aom_convolve8_avg_horiz(src, src_stride, dst, dst_stride, filter_x,
                              x_step_q4, NULL, -1, w, h);
  } else {
    av1_convolve_horiz(src, src_stride, dst, dst_stride, w, h, filter_params,
                       subpel_x_q4, x_step_q4, avg);
  }
}

void av1_convolve_vert_facade(const uint8_t *src, int src_stride, uint8_t *dst,
                              int dst_stride, int w, int h,
                              const InterpFilterParams filter_params,
                              const int subpel_y_q4, int y_step_q4, int avg) {
  if (filter_params.taps == SUBPEL_TAPS) {
    const int16_t *filter_y =
        av1_get_interp_filter_subpel_kernel(filter_params, subpel_y_q4);
    if (avg == 0) {
      aom_convolve8_vert(src, src_stride, dst, dst_stride, NULL, -1, filter_y,
                         y_step_q4, w, h);
    } else {
      aom_convolve8_avg_vert(src, src_stride, dst, dst_stride, NULL, -1,
                             filter_y, y_step_q4, w, h);
    }
  } else {
    av1_convolve_vert(src, src_stride, dst, dst_stride, w, h, filter_params,
                      subpel_y_q4, y_step_q4, avg);
  }
}

void av1_convolve(const uint8_t *src, int src_stride, uint8_t *dst,
                  int dst_stride, int w, int h,
#if CONFIG_DUAL_FILTER
                  const InterpFilter *interp_filter,
#else
                  const InterpFilter interp_filter,
#endif
                  const int subpel_x_q4, int x_step_q4, const int subpel_y_q4,
                  int y_step_q4, int ref_idx) {
  int ignore_horiz = x_step_q4 == 16 && subpel_x_q4 == 0;
  int ignore_vert = y_step_q4 == 16 && subpel_y_q4 == 0;

  assert(w <= MAX_BLOCK_WIDTH);
  assert(h <= MAX_BLOCK_HEIGHT);
  assert(y_step_q4 <= MAX_STEP);
  assert(x_step_q4 <= MAX_STEP);

  if (ignore_horiz && ignore_vert) {
    convolve_copy(src, src_stride, dst, dst_stride, w, h, ref_idx);
  } else if (ignore_vert) {
#if CONFIG_DUAL_FILTER
    InterpFilterParams filter_params =
        av1_get_interp_filter_params(interp_filter[1 + 2 * ref_idx]);
#else
    InterpFilterParams filter_params =
        av1_get_interp_filter_params(interp_filter);
#endif
    assert(filter_params.taps <= MAX_FILTER_TAP);
    av1_convolve_horiz_facade(src, src_stride, dst, dst_stride, w, h,
                              filter_params, subpel_x_q4, x_step_q4, ref_idx);
  } else if (ignore_horiz) {
#if CONFIG_DUAL_FILTER
    InterpFilterParams filter_params =
        av1_get_interp_filter_params(interp_filter[2 * ref_idx]);
#else
    InterpFilterParams filter_params =
        av1_get_interp_filter_params(interp_filter);
#endif
    assert(filter_params.taps <= MAX_FILTER_TAP);
    av1_convolve_vert_facade(src, src_stride, dst, dst_stride, w, h,
                             filter_params, subpel_y_q4, y_step_q4, ref_idx);
  } else {
    // temp's size is set to a 256 aligned value to facilitate SIMD
    // implementation. The value is greater than (maximum possible intermediate
    // height or width) * MAX_SB_SIZE
    DECLARE_ALIGNED(16, uint8_t,
                    temp[((MAX_SB_SIZE * 2 + 16) + 16) * MAX_SB_SIZE]);
    int max_intermediate_size = ((MAX_SB_SIZE * 2 + 16) + 16);
    int filter_size;
    InterpFilterParams filter_params;
#if CONFIG_DUAL_FILTER
    InterpFilterParams filter_params_x =
        av1_get_interp_filter_params(interp_filter[1 + 2 * ref_idx]);
    InterpFilterParams filter_params_y =
        av1_get_interp_filter_params(interp_filter[0 + 2 * ref_idx]);
    if (interp_filter[0 + 2 * ref_idx] == MULTITAP_SHARP &&
        interp_filter[1 + 2 * ref_idx] == MULTITAP_SHARP) {
      // Avoid two directions both using 12-tap filter.
      // This will reduce hardware implementation cost.
      filter_params_y = av1_get_interp_filter_params(EIGHTTAP_SHARP);
    }
#endif

#if CONFIG_DUAL_FILTER
    // we do filter with fewer taps first to reduce hardware implementation
    // complexity
    if (filter_params_y.taps < filter_params_x.taps) {
      int intermediate_width;
      int temp_stride = max_intermediate_size;
      filter_params = filter_params_y;
      filter_size = filter_params_x.taps;
      intermediate_width =
          (((w - 1) * x_step_q4 + subpel_x_q4) >> SUBPEL_BITS) + filter_size;
      assert(intermediate_width <= max_intermediate_size);

      assert(filter_params.taps <= MAX_FILTER_TAP);

      av1_convolve_vert_facade(src - (filter_size / 2 - 1), src_stride, temp,
                               temp_stride, intermediate_width, h,
                               filter_params, subpel_y_q4, y_step_q4, 0);

      filter_params = filter_params_x;
      assert(filter_params.taps <= MAX_FILTER_TAP);

      av1_convolve_horiz_facade(temp + (filter_size / 2 - 1), temp_stride, dst,
                                dst_stride, w, h, filter_params, subpel_x_q4,
                                x_step_q4, ref_idx);
    } else
#endif  // CONFIG_DUAL_FILTER
    {
      int intermediate_height;
      int temp_stride = MAX_SB_SIZE;
#if CONFIG_DUAL_FILTER
      filter_params = filter_params_x;
      filter_size = filter_params_y.taps;
#else
      filter_params = av1_get_interp_filter_params(interp_filter);
      filter_size = filter_params.taps;
#endif
      intermediate_height =
          (((h - 1) * y_step_q4 + subpel_y_q4) >> SUBPEL_BITS) + filter_size;
      assert(intermediate_height <= max_intermediate_size);
      (void)max_intermediate_size;

      assert(filter_params.taps <= MAX_FILTER_TAP);

      av1_convolve_horiz_facade(src - src_stride * (filter_size / 2 - 1),
                                src_stride, temp, temp_stride, w,
                                intermediate_height, filter_params, subpel_x_q4,
                                x_step_q4, 0);

#if CONFIG_DUAL_FILTER
      filter_params = filter_params_y;
#endif
      assert(filter_params.taps <= MAX_FILTER_TAP);

      av1_convolve_vert_facade(temp + temp_stride * (filter_size / 2 - 1),
                               temp_stride, dst, dst_stride, w, h,
                               filter_params, subpel_y_q4, y_step_q4, ref_idx);
    }
  }
}

void av1_convolve_init_c(void) {
  // A placeholder for SIMD initialization
  return;
}

#if CONFIG_AOM_HIGHBITDEPTH
void av1_highbd_convolve_horiz_c(const uint16_t *src, int src_stride,
                                 uint16_t *dst, int dst_stride, int w, int h,
                                 const InterpFilterParams filter_params,
                                 const int subpel_x_q4, int x_step_q4, int avg,
                                 int bd) {
  int x, y;
  int filter_size = filter_params.taps;
  src -= filter_size / 2 - 1;
  for (y = 0; y < h; ++y) {
    int x_q4 = subpel_x_q4;
    for (x = 0; x < w; ++x) {
      const uint16_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
      const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
          filter_params, x_q4 & SUBPEL_MASK);
      int k, sum = 0;
      for (k = 0; k < filter_size; ++k) sum += src_x[k] * x_filter[k];
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

void av1_highbd_convolve_vert_c(const uint16_t *src, int src_stride,
                                uint16_t *dst, int dst_stride, int w, int h,
                                const InterpFilterParams filter_params,
                                const int subpel_y_q4, int y_step_q4, int avg,
                                int bd) {
  int x, y;
  int filter_size = filter_params.taps;
  src -= src_stride * (filter_size / 2 - 1);

  for (x = 0; x < w; ++x) {
    int y_q4 = subpel_y_q4;
    for (y = 0; y < h; ++y) {
      const uint16_t *const src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
      const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
          filter_params, y_q4 & SUBPEL_MASK);
      int k, sum = 0;
      for (k = 0; k < filter_size; ++k)
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

static void highbd_convolve_copy(const uint16_t *src, int src_stride,
                                 uint16_t *dst, int dst_stride, int w, int h,
                                 int avg, int bd) {
  if (avg == 0) {
    int r;
    for (r = 0; r < h; ++r) {
      memcpy(dst, src, w * sizeof(*src));
      src += src_stride;
      dst += dst_stride;
    }
  } else {
    int r, c;
    for (r = 0; r < h; ++r) {
      for (c = 0; c < w; ++c) {
        dst[c] = clip_pixel_highbd(ROUND_POWER_OF_TWO(dst[c] + src[c], 1), bd);
      }
      src += src_stride;
      dst += dst_stride;
    }
  }
}

void av1_highbd_convolve_horiz_facade(const uint8_t *src8, int src_stride,
                                      uint8_t *dst8, int dst_stride, int w,
                                      int h,
                                      const InterpFilterParams filter_params,
                                      const int subpel_x_q4, int x_step_q4,
                                      int avg, int bd) {
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
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
    av1_highbd_convolve_horiz(src, src_stride, dst, dst_stride, w, h,
                              filter_params, subpel_x_q4, x_step_q4, avg, bd);
  }
}

void av1_highbd_convolve_vert_facade(const uint8_t *src8, int src_stride,
                                     uint8_t *dst8, int dst_stride, int w,
                                     int h,
                                     const InterpFilterParams filter_params,
                                     const int subpel_y_q4, int y_step_q4,
                                     int avg, int bd) {
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);

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
    av1_highbd_convolve_vert(src, src_stride, dst, dst_stride, w, h,
                             filter_params, subpel_y_q4, y_step_q4, avg, bd);
  }
}

void av1_highbd_convolve(const uint8_t *src8, int src_stride, uint8_t *dst8,
                         int dst_stride, int w, int h,
#if CONFIG_DUAL_FILTER
                         const InterpFilter *interp_filter,
#else
                         const InterpFilter interp_filter,
#endif
                         const int subpel_x_q4, int x_step_q4,
                         const int subpel_y_q4, int y_step_q4, int ref_idx,
                         int bd) {
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  int ignore_horiz = x_step_q4 == 16 && subpel_x_q4 == 0;
  int ignore_vert = y_step_q4 == 16 && subpel_y_q4 == 0;

  assert(w <= MAX_BLOCK_WIDTH);
  assert(h <= MAX_BLOCK_HEIGHT);
  assert(y_step_q4 <= MAX_STEP);
  assert(x_step_q4 <= MAX_STEP);

  if (ignore_horiz && ignore_vert) {
    highbd_convolve_copy(src, src_stride, dst, dst_stride, w, h, ref_idx, bd);
  } else if (ignore_vert) {
#if CONFIG_DUAL_FILTER
    InterpFilterParams filter_params =
        av1_get_interp_filter_params(interp_filter[1 + 2 * ref_idx]);
#else
    InterpFilterParams filter_params =
        av1_get_interp_filter_params(interp_filter);
#endif
    av1_highbd_convolve_horiz_facade(src8, src_stride, dst8, dst_stride, w, h,
                                     filter_params, subpel_x_q4, x_step_q4,
                                     ref_idx, bd);
  } else if (ignore_horiz) {
#if CONFIG_DUAL_FILTER
    InterpFilterParams filter_params =
        av1_get_interp_filter_params(interp_filter[0 + 2 * ref_idx]);
#else
    InterpFilterParams filter_params =
        av1_get_interp_filter_params(interp_filter);
#endif
    av1_highbd_convolve_vert_facade(src8, src_stride, dst8, dst_stride, w, h,
                                    filter_params, subpel_y_q4, y_step_q4,
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
    InterpFilterParams filter_params;
#if CONFIG_DUAL_FILTER
    InterpFilterParams filter_params_x =
        av1_get_interp_filter_params(interp_filter[1 + 2 * ref_idx]);
    InterpFilterParams filter_params_y =
        av1_get_interp_filter_params(interp_filter[0 + 2 * ref_idx]);
    if (interp_filter[0 + 2 * ref_idx] == MULTITAP_SHARP &&
        interp_filter[1 + 2 * ref_idx] == MULTITAP_SHARP) {
      // Avoid two directions both using 12-tap filter.
      // This will reduce hardware implementation cost.
      filter_params_y = av1_get_interp_filter_params(EIGHTTAP_SHARP);
    }
#endif

#if CONFIG_DUAL_FILTER
    if (filter_params_y.taps < filter_params_x.taps) {
      int intermediate_width;
      int temp_stride = max_intermediate_size;
      filter_params = filter_params_y;
      filter_size = filter_params_x.taps;
      intermediate_width =
          (((w - 1) * x_step_q4 + subpel_x_q4) >> SUBPEL_BITS) + filter_size;
      assert(intermediate_width <= max_intermediate_size);

      assert(filter_params.taps <= MAX_FILTER_TAP);

      av1_highbd_convolve_vert_facade(
          src8 - (filter_size / 2 - 1), src_stride, temp8, temp_stride,
          intermediate_width, h, filter_params, subpel_y_q4, y_step_q4, 0, bd);

      filter_params = filter_params_x;
      assert(filter_params.taps <= MAX_FILTER_TAP);

      av1_highbd_convolve_horiz_facade(
          temp8 + (filter_size / 2 - 1), temp_stride, dst8, dst_stride, w, h,
          filter_params, subpel_x_q4, x_step_q4, ref_idx, bd);
    } else
#endif  // CONFIG_DUAL_FILTER
    {
      int intermediate_height;
      int temp_stride = MAX_SB_SIZE;
#if CONFIG_DUAL_FILTER
      filter_params = filter_params_x;
      filter_size = filter_params_y.taps;
#else
      filter_params = av1_get_interp_filter_params(interp_filter);
      filter_size = filter_params.taps;
#endif
      intermediate_height =
          (((h - 1) * y_step_q4 + subpel_y_q4) >> SUBPEL_BITS) + filter_size;
      assert(intermediate_height <= max_intermediate_size);
      (void)max_intermediate_size;

      av1_highbd_convolve_horiz_facade(
          src8 - src_stride * (filter_size / 2 - 1), src_stride, temp8,
          temp_stride, w, intermediate_height, filter_params, subpel_x_q4,
          x_step_q4, 0, bd);

#if CONFIG_DUAL_FILTER
      filter_params = filter_params_y;
#endif
      filter_size = filter_params.taps;
      assert(filter_params.taps <= MAX_FILTER_TAP);

      av1_highbd_convolve_vert_facade(
          temp8 + temp_stride * (filter_size / 2 - 1), temp_stride, dst8,
          dst_stride, w, h, filter_params, subpel_y_q4, y_step_q4, ref_idx, bd);
    }
  }
}
#endif  // CONFIG_AOM_HIGHBITDEPTH
