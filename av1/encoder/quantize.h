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

#ifndef VP10_ENCODER_QUANTIZE_H_
#define VP10_ENCODER_QUANTIZE_H_

#include "./vpx_config.h"
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

void vp10_regular_quantize_b_4x4(MACROBLOCK *x, int plane, int block,
                                 const int16_t *scan, const int16_t *iscan);

struct VP10_COMP;
struct VP10Common;

void vp10_frame_init_quantizer(struct VP10_COMP *cpi);

void vp10_init_plane_quantizers(struct VP10_COMP *cpi, MACROBLOCK *x);

void vp10_init_quantizer(struct VP10_COMP *cpi);

void vp10_set_quantizer(struct VP10Common *cm, int q);

int vp10_quantizer_to_qindex(int quantizer);

int vp10_qindex_to_quantizer(int qindex);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_ENCODER_QUANTIZE_H_
