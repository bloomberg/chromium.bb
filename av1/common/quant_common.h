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

#ifndef AV1_COMMON_QUANT_COMMON_H_
#define AV1_COMMON_QUANT_COMMON_H_

#include "aom/aom_codec.h"
#include "av1/common/seg_common.h"
#include "av1/common/enums.h"
#include "av1/common/entropy.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MINQ 0
#define MAXQ 255
#define QINDEX_RANGE (MAXQ - MINQ + 1)
#define QINDEX_BITS 8
#if CONFIG_AOM_QM
// Total number of QM sets stored
#define QM_LEVEL_BITS 4
#define NUM_QM_LEVELS (1 << QM_LEVEL_BITS)
/* Range of QMS is between first and last value, with offset applied to inter
 * blocks*/
#define DEFAULT_QM_FIRST 5
#define DEFAULT_QM_LAST 9
#endif

struct AV1Common;

int16_t av1_dc_quant_Q3(int qindex, int delta, aom_bit_depth_t bit_depth);
int16_t av1_ac_quant_Q3(int qindex, int delta, aom_bit_depth_t bit_depth);
int16_t av1_dc_quant_QTX(int qindex, int delta, aom_bit_depth_t bit_depth);
int16_t av1_ac_quant_QTX(int qindex, int delta, aom_bit_depth_t bit_depth);
int16_t av1_qindex_from_ac_Q3(int ac_Q3, aom_bit_depth_t bit_depth);

int av1_get_qindex(const struct segmentation *seg, int segment_id,
#if CONFIG_Q_SEGMENTATION
                   int q_segment_id, int base_qindex);
#else
                   int base_qindex);
#endif
#if CONFIG_AOM_QM
// Reduce the large number of quantizers to a smaller number of levels for which
// different matrices may be defined
static INLINE int aom_get_qmlevel(int qindex, int first, int last) {
  return first + (qindex * (last + 1 - first)) / QINDEX_RANGE;
}
void aom_qm_init(struct AV1Common *cm);
qm_val_t *aom_iqmatrix(struct AV1Common *cm, int qindex, int comp,
                       TX_SIZE tx_size);
qm_val_t *aom_qmatrix(struct AV1Common *cm, int qindex, int comp,
                      TX_SIZE tx_size);

static inline TX_SIZE get_qm_tx_size(TX_SIZE tx_size) {
#if CONFIG_TX64X64
  if (tx_size == TX_64X64 || tx_size == TX_64X32 || tx_size == TX_32X64) {
    return TX_32X32;
  }
  if (tx_size == TX_16X64) {
    return TX_16X32;
  }
  if (tx_size == TX_64X16) {
    return TX_32X16;
  }
#endif  // CONFIG_TX64X64
  return tx_size;
}
#endif  // CONFIG_AOM_QM

#if CONFIG_NEW_QUANT

#define QUANT_PROFILES 4
#define QUANT_RANGES 2
#define NUQ_KNOTS 3

typedef tran_low_t dequant_val_type_nuq[NUQ_KNOTS + 1];
typedef tran_low_t cuml_bins_type_nuq[NUQ_KNOTS];
void av1_get_dequant_val_nuq(int q, int band, tran_low_t *dq,
                             tran_low_t *cuml_bins, int dq_off_index);
tran_low_t av1_dequant_abscoeff_nuq(int v, int q, const tran_low_t *dq);
tran_low_t av1_dequant_coeff_nuq(int v, int q, const tran_low_t *dq);

static INLINE int qindex_to_qrange(int qindex) {
  return (qindex < 140 ? 1 : 0);
}

static INLINE int get_dq_profile(int qindex, int is_inter,
                                 PLANE_TYPE plane_type) {
  // intra/inter, Y/UV, ctx, qrange
  static const int dq_profile_lookup[REF_TYPES][PLANE_TYPES][QUANT_RANGES] = {
    {
        // intra
        { 2, 1 },  // Y
        { 3, 1 },  // UV
    },
    {
        // inter
        { 2, 1 },  // Y
        { 3, 1 },  // UV
    },
  };
  if (!qindex) return 0;  // lossless
  return dq_profile_lookup[is_inter][plane_type][qindex_to_qrange(qindex)];
}
#endif  // CONFIG_NEW_QUANT

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_QUANT_COMMON_H_
