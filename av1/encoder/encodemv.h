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

#ifndef VP10_ENCODER_ENCODEMV_H_
#define VP10_ENCODER_ENCODEMV_H_

#include "av1/encoder/encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

void vp10_entropy_mv_init(void);

void vp10_write_nmv_probs(VP10_COMMON *cm, int usehp, vpx_writer *w,
                          nmv_context_counts *const counts);

void vp10_encode_mv(VP10_COMP *cpi, vpx_writer *w, const MV *mv, const MV *ref,
                    const nmv_context *mvctx, int usehp);

void vp10_build_nmv_cost_table(int *mvjoint, int *mvcost[2],
                               const nmv_context *mvctx, int usehp);

void vp10_update_mv_count(ThreadData *td);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_ENCODER_ENCODEMV_H_
