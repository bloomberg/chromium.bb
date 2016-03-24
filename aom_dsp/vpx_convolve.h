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
#ifndef VPX_DSP_VPX_CONVOLVE_H_
#define VPX_DSP_VPX_CONVOLVE_H_

#include "./vpx_config.h"
#include "aom/vpx_integer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*convolve_fn_t)(const uint8_t *src, ptrdiff_t src_stride,
                              uint8_t *dst, ptrdiff_t dst_stride,
                              const int16_t *filter_x, int x_step_q4,
                              const int16_t *filter_y, int y_step_q4, int w,
                              int h);

#if CONFIG_VPX_HIGHBITDEPTH
typedef void (*highbd_convolve_fn_t)(const uint8_t *src, ptrdiff_t src_stride,
                                     uint8_t *dst, ptrdiff_t dst_stride,
                                     const int16_t *filter_x, int x_step_q4,
                                     const int16_t *filter_y, int y_step_q4,
                                     int w, int h, int bd);
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VPX_DSP_VPX_CONVOLVE_H_
