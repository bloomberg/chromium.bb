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

#ifndef AV1_ENCODER_QUANTIZE_H_
#define AV1_ENCODER_QUANTIZE_H_

#include "./aom_config.h"
#include "av1/common/quant_common.h"
#include "av1/encoder/block.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  DECLARE_ALIGNED(16, int16_t, y_quant[QINDEX_RANGE][8]);
  DECLARE_ALIGNED(16, int16_t, y_quant_shift[QINDEX_RANGE][8]);
  DECLARE_ALIGNED(16, int16_t, y_zbin[QINDEX_RANGE][8]);
  DECLARE_ALIGNED(16, int16_t, y_round[QINDEX_RANGE][8]);

  // TODO(jingning): in progress of re-working the quantization. will decide
  // if we want to deprecate the current use of y_quant.
  DECLARE_ALIGNED(16, int16_t, y_quant_fp[QINDEX_RANGE][8]);
  DECLARE_ALIGNED(16, int16_t, uv_quant_fp[QINDEX_RANGE][8]);
  DECLARE_ALIGNED(16, int16_t, y_round_fp[QINDEX_RANGE][8]);
  DECLARE_ALIGNED(16, int16_t, uv_round_fp[QINDEX_RANGE][8]);

  DECLARE_ALIGNED(16, int16_t, uv_quant[QINDEX_RANGE][8]);
  DECLARE_ALIGNED(16, int16_t, uv_quant_shift[QINDEX_RANGE][8]);
  DECLARE_ALIGNED(16, int16_t, uv_zbin[QINDEX_RANGE][8]);
  DECLARE_ALIGNED(16, int16_t, uv_round[QINDEX_RANGE][8]);
} QUANTS;

void av1_regular_quantize_b_4x4(MACROBLOCK *x, int plane, int block,
                                const int16_t *scan, const int16_t *iscan);

struct AV1_COMP;
struct AV1Common;

void av1_frame_init_quantizer(struct AV1_COMP *cpi);

void av1_init_plane_quantizers(struct AV1_COMP *cpi, MACROBLOCK *x);

void av1_init_quantizer(struct AV1_COMP *cpi);

void av1_set_quantizer(struct AV1Common *cm, int q);

int av1_quantizer_to_qindex(int quantizer);

int av1_qindex_to_quantizer(int qindex);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_QUANTIZE_H_
