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

#ifndef AV1_ENCODER_RDOPT_H_
#define AV1_ENCODER_RDOPT_H_

#include "av1/common/blockd.h"
#include "av1/common/txb_common.h"

#include "av1/encoder/block.h"
#include "av1/encoder/context_tree.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/encodetxb.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_REF_MV_SERCH 3

struct TileInfo;
struct macroblock;
struct RD_STATS;

#if CONFIG_RD_DEBUG
static INLINE void av1_update_txb_coeff_cost(RD_STATS *rd_stats, int plane,
                                             TX_SIZE tx_size, int blk_row,
                                             int blk_col, int txb_coeff_cost) {
  (void)blk_row;
  (void)blk_col;
  (void)tx_size;
  rd_stats->txb_coeff_cost[plane] += txb_coeff_cost;

  {
    const int txb_h = tx_size_high_unit[tx_size];
    const int txb_w = tx_size_wide_unit[tx_size];
    int idx, idy;
    for (idy = 0; idy < txb_h; ++idy)
      for (idx = 0; idx < txb_w; ++idx)
        rd_stats->txb_coeff_cost_map[plane][blk_row + idy][blk_col + idx] = 0;

    rd_stats->txb_coeff_cost_map[plane][blk_row][blk_col] = txb_coeff_cost;
  }
  assert(blk_row < TXB_COEFF_COST_MAP_SIZE);
  assert(blk_col < TXB_COEFF_COST_MAP_SIZE);
}
#endif

typedef enum OUTPUT_STATUS {
  OUTPUT_HAS_PREDICTED_PIXELS,
  OUTPUT_HAS_DECODED_PIXELS
} OUTPUT_STATUS;

// Returns the number of colors in 'src'.
int av1_count_colors(const uint8_t *src, int stride, int rows, int cols,
                     int *val_count);
// Same as av1_count_colors(), but for high-bitdepth mode.
int av1_count_colors_highbd(const uint8_t *src8, int stride, int rows, int cols,
                            int bit_depth, int *val_count);

void av1_dist_block(const struct AV1_COMP *cpi, MACROBLOCK *x, int plane,
                    BLOCK_SIZE plane_bsize, int block, int blk_row, int blk_col,
                    TX_SIZE tx_size, int64_t *out_dist, int64_t *out_sse,
                    OUTPUT_STATUS output_status);

#if CONFIG_DIST_8X8
int64_t av1_dist_8x8(const struct AV1_COMP *const cpi, const MACROBLOCK *x,
                     const uint8_t *src, int src_stride, const uint8_t *dst,
                     int dst_stride, const BLOCK_SIZE tx_bsize, int bsw,
                     int bsh, int visible_w, int visible_h, int qindex);
#endif

static INLINE int av1_cost_coeffs(const struct AV1_COMP *const cpi,
                                  MACROBLOCK *x, int plane, int blk_row,
                                  int blk_col, int block, TX_SIZE tx_size,
                                  const SCAN_ORDER *scan_order,
                                  const ENTROPY_CONTEXT *a,
                                  const ENTROPY_CONTEXT *l,
                                  int use_fast_coef_costing) {
#if TXCOEFF_COST_TIMER
  struct aom_usec_timer timer;
  aom_usec_timer_start(&timer);
#endif
  const AV1_COMMON *const cm = &cpi->common;
  (void)scan_order;
  (void)use_fast_coef_costing;
  const MACROBLOCKD *xd = &x->e_mbd;
  const MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const struct macroblockd_plane *pd = &xd->plane[plane];
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
  TXB_CTX txb_ctx;
  get_txb_ctx(plane_bsize, tx_size, plane, a, l, &txb_ctx);
  const int eob = x->plane[plane].eobs[block];
  int cost;
  if (eob) {
    cost = av1_cost_coeffs_txb(cm, x, plane, blk_row, blk_col, block, tx_size,
                               &txb_ctx);
  } else {
    const TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);
    const PLANE_TYPE plane_type = get_plane_type(plane);
    const LV_MAP_COEFF_COST *const coeff_costs =
        &x->coeff_costs[txs_ctx][plane_type];
    cost = coeff_costs->txb_skip_cost[txb_ctx.txb_skip_ctx][1];
  }
#if TXCOEFF_COST_TIMER
  AV1_COMMON *tmp_cm = (AV1_COMMON *)&cpi->common;
  aom_usec_timer_mark(&timer);
  const int64_t elapsed_time = aom_usec_timer_elapsed(&timer);
  tmp_cm->txcoeff_cost_timer += elapsed_time;
  ++tmp_cm->txcoeff_cost_count;
#endif
  return cost;
}

void av1_rd_pick_intra_mode_sb(const struct AV1_COMP *cpi, struct macroblock *x,
                               int mi_row, int mi_col, struct RD_STATS *rd_cost,
                               BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx,
                               int64_t best_rd);

unsigned int av1_get_sby_perpixel_variance(const struct AV1_COMP *cpi,
                                           const struct buf_2d *ref,
                                           BLOCK_SIZE bs);
unsigned int av1_high_get_sby_perpixel_variance(const struct AV1_COMP *cpi,
                                                const struct buf_2d *ref,
                                                BLOCK_SIZE bs, int bd);

void av1_rd_pick_inter_mode_sb(const struct AV1_COMP *cpi,
                               struct TileDataEnc *tile_data,
                               struct macroblock *x, int mi_row, int mi_col,
                               struct RD_STATS *rd_cost, BLOCK_SIZE bsize,
                               PICK_MODE_CONTEXT *ctx, int64_t best_rd_so_far);

void av1_rd_pick_inter_mode_sb_seg_skip(
    const struct AV1_COMP *cpi, struct TileDataEnc *tile_data,
    struct macroblock *x, int mi_row, int mi_col, struct RD_STATS *rd_cost,
    BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx, int64_t best_rd_so_far);

int av1_internal_image_edge(const struct AV1_COMP *cpi);
int av1_active_h_edge(const struct AV1_COMP *cpi, int mi_row, int mi_step);
int av1_active_v_edge(const struct AV1_COMP *cpi, int mi_col, int mi_step);
int av1_active_edge_sb(const struct AV1_COMP *cpi, int mi_row, int mi_col);

int av1_tx_type_cost(const AV1_COMMON *cm, const MACROBLOCK *x,
                     const MACROBLOCKD *xd, BLOCK_SIZE bsize, int plane,
                     TX_SIZE tx_size, TX_TYPE tx_type);

void av1_inverse_transform_block_facade(MACROBLOCKD *xd, int plane, int block,
                                        int blk_row, int blk_col, int eob,
                                        int reduced_tx_set);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_RDOPT_H_
