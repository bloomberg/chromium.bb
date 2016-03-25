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

#ifndef AV1_ENCODER_SKIN_MAP_H_
#define AV1_ENCODER_SKIN_MAP_H_

#include "av1/common/blockd.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AV1_COMP;

// #define OUTPUT_YUV_SKINMAP

int av1_skin_pixel(const uint8_t y, const uint8_t cb, const uint8_t cr);

#ifdef OUTPUT_YUV_SKINMAP
// For viewing skin map on input source.
void av1_compute_skin_map(AV1_COMP *const cpi, FILE *yuv_skinmap_file);
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_SKIN_MAP_H_
