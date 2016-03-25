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

#ifndef VPX_DSP_VARIANCE_H_
#define VPX_DSP_VARIANCE_H_

#include "./aom_config.h"

#include "aom/aom_integer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FILTER_BITS 7
#define FILTER_WEIGHT 128

typedef unsigned int (*aom_sad_fn_t)(const uint8_t *a, int a_stride,
                                     const uint8_t *b_ptr, int b_stride);

typedef unsigned int (*aom_sad_avg_fn_t)(const uint8_t *a_ptr, int a_stride,
                                         const uint8_t *b_ptr, int b_stride,
                                         const uint8_t *second_pred);

typedef void (*vp8_copy32xn_fn_t)(const uint8_t *a, int a_stride, uint8_t *b,
                                  int b_stride, int n);

typedef void (*aom_sad_multi_fn_t)(const uint8_t *a, int a_stride,
                                   const uint8_t *b, int b_stride,
                                   unsigned int *sad_array);

typedef void (*aom_sad_multi_d_fn_t)(const uint8_t *a, int a_stride,
                                     const uint8_t *const b_array[],
                                     int b_stride, unsigned int *sad_array);

typedef unsigned int (*aom_variance_fn_t)(const uint8_t *a, int a_stride,
                                          const uint8_t *b, int b_stride,
                                          unsigned int *sse);

typedef unsigned int (*aom_subpixvariance_fn_t)(const uint8_t *a, int a_stride,
                                                int xoffset, int yoffset,
                                                const uint8_t *b, int b_stride,
                                                unsigned int *sse);

typedef unsigned int (*aom_subp_avg_variance_fn_t)(
    const uint8_t *a_ptr, int a_stride, int xoffset, int yoffset,
    const uint8_t *b_ptr, int b_stride, unsigned int *sse,
    const uint8_t *second_pred);
#if CONFIG_VP10
typedef struct aom_variance_vtable {
  aom_sad_fn_t sdf;
  aom_sad_avg_fn_t sdaf;
  aom_variance_fn_t vf;
  aom_subpixvariance_fn_t svf;
  aom_subp_avg_variance_fn_t svaf;
  aom_sad_multi_fn_t sdx3f;
  aom_sad_multi_fn_t sdx8f;
  aom_sad_multi_d_fn_t sdx4df;
} aom_variance_fn_ptr_t;
#endif  // CONFIG_VP10

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VPX_DSP_VARIANCE_H_
