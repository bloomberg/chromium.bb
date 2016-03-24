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

#ifndef VP10_ENCODER_AQ_COMPLEXITY_H_
#define VP10_ENCODER_AQ_COMPLEXITY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "av1/common/enums.h"

struct VP10_COMP;
struct macroblock;

// Select a segment for the current Block.
void vp10_caq_select_segment(struct VP10_COMP *cpi, struct macroblock *,
                             BLOCK_SIZE bs, int mi_row, int mi_col,
                             int projected_rate);

// This function sets up a set of segments with delta Q values around
// the baseline frame quantizer.
void vp10_setup_in_frame_q_adj(struct VP10_COMP *cpi);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_ENCODER_AQ_COMPLEXITY_H_
