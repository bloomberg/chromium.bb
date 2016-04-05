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

#ifndef AV1_ENCODER_RD_H_
#define AV1_ENCODER_RD_H_

#include <limits.h>

#include "av1/common/blockd.h"

#include "av1/encoder/block.h"
#include "av1/encoder/context_tree.h"
#include "av1/encoder/cost.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RDDIV_BITS 7
#define RD_EPB_SHIFT 6

#define RDCOST(RM, DM, R, D) \
  (ROUND_POWER_OF_TWO(((int64_t)R) * (RM), AV1_PROB_COST_SHIFT) + (D << DM))
#define QIDX_SKIP_THRESH 115

#define MV_COST_WEIGHT 108
#define MV_COST_WEIGHT_SUB 120

#define INVALID_MV 0x80008000

#define MAX_MODES 30
#define MAX_REFS 6

#define RD_THRESH_MAX_FACT 64
#define RD_THRESH_INC 1

// This enumerator type needs to be kept aligned with the mode order in
// const MODE_DEFINITION av1_mode_order[MAX_MODES] used in the rd code.
typedef enum {
  THR_NEARESTMV,
  THR_NEARESTA,
  THR_NEARESTG,

  THR_DC,

  THR_NEWMV,
  THR_NEWA,
  THR_NEWG,

  THR_NEARMV,
  THR_NEARA,
  THR_NEARG,

  THR_ZEROMV,
  THR_ZEROG,
  THR_ZEROA,

  THR_COMP_NEARESTLA,
  THR_COMP_NEARESTGA,

  THR_TM,

  THR_COMP_NEARLA,
  THR_COMP_NEWLA,
  THR_COMP_NEARGA,
  THR_COMP_NEWGA,

  THR_COMP_ZEROLA,
  THR_COMP_ZEROGA,

  THR_H_PRED,
  THR_V_PRED,
  THR_D135_PRED,
  THR_D207_PRED,
  THR_D153_PRED,
  THR_D63_PRED,
  THR_D117_PRED,
  THR_D45_PRED,
} THR_MODES;

typedef enum {
  THR_LAST,
  THR_GOLD,
  THR_ALTR,
  THR_COMP_LA,
  THR_COMP_GA,
  THR_INTRA,
} THR_MODES_SUB8X8;

typedef struct RD_OPT {
  // Thresh_mult is used to set a threshold for the rd score. A higher value
  // means that we will accept the best mode so far more often. This number
  // is used in combination with the current block size, and thresh_freq_fact
  // to pick a threshold.
  int thresh_mult[MAX_MODES];
  int thresh_mult_sub8x8[MAX_REFS];

  int threshes[MAX_SEGMENTS][BLOCK_SIZES][MAX_MODES];

  int64_t prediction_type_threshes[MAX_REF_FRAMES][REFERENCE_MODES];

  int64_t filter_threshes[MAX_REF_FRAMES][SWITCHABLE_FILTER_CONTEXTS];

  int RDMULT;
  int RDDIV;
} RD_OPT;

typedef struct RD_COST {
  int rate;
  int64_t dist;
  int64_t rdcost;
} RD_COST;

// Reset the rate distortion cost values to maximum (invalid) value.
void av1_rd_cost_reset(RD_COST *rd_cost);
// Initialize the rate distortion cost values to zero.
void av1_rd_cost_init(RD_COST *rd_cost);

struct TileInfo;
struct TileDataEnc;
struct AV1_COMP;
struct macroblock;

int av1_compute_rd_mult(const struct AV1_COMP *cpi, int qindex);

void av1_initialize_rd_consts(struct AV1_COMP *cpi);

void av1_initialize_me_consts(struct AV1_COMP *cpi, MACROBLOCK *x,
                               int qindex);

void av1_model_rd_from_var_lapndz(unsigned int var, unsigned int n,
                                   unsigned int qstep, int *rate,
                                   int64_t *dist);

int av1_get_switchable_rate(const struct AV1_COMP *cpi,
                             const MACROBLOCKD *const xd);

int av1_raster_block_offset(BLOCK_SIZE plane_bsize, int raster_block,
                             int stride);

int16_t *av1_raster_block_offset_int16(BLOCK_SIZE plane_bsize,
                                        int raster_block, int16_t *base);

YV12_BUFFER_CONFIG *av1_get_scaled_ref_frame(const struct AV1_COMP *cpi,
                                              int ref_frame);

void av1_init_me_luts(void);

#if CONFIG_REF_MV
void av1_set_mvcost(MACROBLOCK *x, MV_REFERENCE_FRAME ref_frame);
#endif

void av1_get_entropy_contexts(BLOCK_SIZE bsize, TX_SIZE tx_size,
                               const struct macroblockd_plane *pd,
                               ENTROPY_CONTEXT t_above[16],
                               ENTROPY_CONTEXT t_left[16]);

void av1_set_rd_speed_thresholds(struct AV1_COMP *cpi);

void av1_set_rd_speed_thresholds_sub8x8(struct AV1_COMP *cpi);

void av1_update_rd_thresh_fact(int (*fact)[MAX_MODES], int rd_thresh,
                                int bsize, int best_mode_index);

static INLINE int rd_less_than_thresh(int64_t best_rd, int thresh,
                                      int thresh_fact) {
  return best_rd < ((int64_t)thresh * thresh_fact >> 5) || thresh == INT_MAX;
}

void av1_mv_pred(struct AV1_COMP *cpi, MACROBLOCK *x, uint8_t *ref_y_buffer,
                  int ref_y_stride, int ref_frame, BLOCK_SIZE block_size);

static INLINE void set_error_per_bit(MACROBLOCK *x, int rdmult) {
  x->errorperbit = rdmult >> RD_EPB_SHIFT;
  x->errorperbit += (x->errorperbit == 0);
}

void av1_setup_pred_block(const MACROBLOCKD *xd,
                           struct buf_2d dst[MAX_MB_PLANE],
                           const YV12_BUFFER_CONFIG *src, int mi_row,
                           int mi_col, const struct scale_factors *scale,
                           const struct scale_factors *scale_uv);

int av1_get_intra_cost_penalty(int qindex, int qdelta,
                                aom_bit_depth_t bit_depth);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_RD_H_
