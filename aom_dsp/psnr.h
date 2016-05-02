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

#ifndef AOM_DSP_PSNR_H_
#define AOM_DSP_PSNR_H_

#include "aom_scale/yv12config.h"

#define MAX_PSNR 100.0

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  double psnr[4];       // total/y/u/v
  uint64_t sse[4];      // total/y/u/v
  uint32_t samples[4];  // total/y/u/v
} PSNR_STATS;

/*!\brief Converts SSE to PSNR
*
* Converts sum of squared errros (SSE) to peak signal-to-noise ratio (PNSR).
*
* \param[in]    samples       Number of samples
* \param[in]    peak          Max sample value
* \param[in]    sse           Sum of squared errors
*/
double aom_sse_to_psnr(double samples, double peak, double sse);
int64_t aom_get_y_sse(const YV12_BUFFER_CONFIG *a, const YV12_BUFFER_CONFIG *b);
#if CONFIG_AOM_HIGHBITDEPTH
int64_t aom_highbd_get_y_sse(const YV12_BUFFER_CONFIG *a,
                             const YV12_BUFFER_CONFIG *b);
void calc_highbd_psnr(const YV12_BUFFER_CONFIG *a, const YV12_BUFFER_CONFIG *b,
                      PSNR_STATS *psnr, unsigned int bit_depth,
                      unsigned int in_bit_depth);
int64_t highbd_get_sse_shift(const uint8_t *a8, int a_stride, const uint8_t *b8,
                             int b_stride, int width, int height,
                             unsigned int input_shift);
#endif
void calc_psnr(const YV12_BUFFER_CONFIG *a, const YV12_BUFFER_CONFIG *b,
               PSNR_STATS *psnr);

int64_t highbd_get_sse(const uint8_t *a, int a_stride, const uint8_t *b,
                       int b_stride, int width, int height);
#ifdef __cplusplus
}  // extern "C"
#endif
#endif  // AOM_DSP_PSNR_H_