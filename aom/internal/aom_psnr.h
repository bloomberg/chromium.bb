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

#ifndef VPX_INTERNAL_VPX_PSNR_H_
#define VPX_INTERNAL_VPX_PSNR_H_

#ifdef __cplusplus
extern "C" {
#endif

// TODO(dkovalev) change aom_sse_to_psnr signature: double -> int64_t

/*!\brief Converts SSE to PSNR
 *
 * Converts sum of squared errros (SSE) to peak signal-to-noise ratio (PNSR).
 *
 * \param[in]    samples       Number of samples
 * \param[in]    peak          Max sample value
 * \param[in]    sse           Sum of squared errors
 */
double aom_sse_to_psnr(double samples, double peak, double sse);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VPX_INTERNAL_VPX_PSNR_H_
