/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_DSP_NOISE_UTIL_H_
#define AOM_DSP_NOISE_UTIL_H_

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// Computes normalized cross correlation of two vectors a and b of length n.
double aom_normalized_cross_correlation(const double *a, const double *b,
                                        int n);

// Validates the correlated noise in the data buffer of size (w, h).
int aom_noise_data_validate(const double *data, int w, int h);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // AOM_DSP_NOISE_UTIL_H_
