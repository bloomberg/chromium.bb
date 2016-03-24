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

#ifndef VP10_ENCODER_AQ_VARIANCE_H_
#define VP10_ENCODER_AQ_VARIANCE_H_

#include "av1/encoder/encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

unsigned int vp10_vaq_segment_id(int energy);
void vp10_vaq_frame_setup(VP10_COMP *cpi);

int vp10_block_energy(VP10_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bs);
double vp10_log_block_var(VP10_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bs);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_ENCODER_AQ_VARIANCE_H_
