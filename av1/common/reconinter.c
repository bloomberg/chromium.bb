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

#include <assert.h>

#include "./aom_scale_rtcd.h"
#include "./aom_config.h"

#include "aom/aom_integer.h"

#include "av1/common/blockd.h"
#include "av1/common/reconinter.h"
#include "av1/common/reconintra.h"

#if CONFIG_AOM_HIGHBITDEPTH
void av1_highbd_build_inter_predictor(
    const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride,
    const MV *src_mv, const struct scale_factors *sf, int w, int h, int ref,
    const InterpFilter *interp_filter, enum mv_precision precision, int x,
    int y, int bd) {
  const int is_q4 = precision == MV_PRECISION_Q4;
  const MV mv_q4 = { is_q4 ? src_mv->row : src_mv->row * 2,
                     is_q4 ? src_mv->col : src_mv->col * 2 };
  MV32 mv = av1_scale_mv(&mv_q4, x, y, sf);
  const int subpel_x = mv.col & SUBPEL_MASK;
  const int subpel_y = mv.row & SUBPEL_MASK;

  src += (mv.row >> SUBPEL_BITS) * src_stride + (mv.col >> SUBPEL_BITS);

  high_inter_predictor(src, src_stride, dst, dst_stride, subpel_x, subpel_y, sf,
                       w, h, ref, interp_filter, sf->x_step_q4, sf->y_step_q4,
                       bd);
}
#endif  // CONFIG_AOM_HIGHBITDEPTH

void av1_build_inter_predictor(const uint8_t *src, int src_stride, uint8_t *dst,
                               int dst_stride, const MV *src_mv,
                               const struct scale_factors *sf, int w, int h,
                               int ref, const InterpFilter *interp_filter,
                               enum mv_precision precision, int x, int y) {
  const int is_q4 = precision == MV_PRECISION_Q4;
  const MV mv_q4 = { is_q4 ? src_mv->row : src_mv->row * 2,
                     is_q4 ? src_mv->col : src_mv->col * 2 };
  MV32 mv = av1_scale_mv(&mv_q4, x, y, sf);
  const int subpel_x = mv.col & SUBPEL_MASK;
  const int subpel_y = mv.row & SUBPEL_MASK;

  src += (mv.row >> SUBPEL_BITS) * src_stride + (mv.col >> SUBPEL_BITS);

  inter_predictor(src, src_stride, dst, dst_stride, subpel_x, subpel_y, sf, w,
                  h, ref, interp_filter, sf->x_step_q4, sf->y_step_q4);
}

void build_inter_predictors(MACROBLOCKD *xd, int plane,
#if CONFIG_MOTION_VAR
                            int mi_col_offset, int mi_row_offset,
#endif  // CONFIG_MOTION_VAR
                            int block, int bw, int bh, int x, int y, int w,
                            int h, int mi_x, int mi_y) {
  struct macroblockd_plane *const pd = &xd->plane[plane];
#if CONFIG_MOTION_VAR
  const MODE_INFO *mi = xd->mi[mi_col_offset + xd->mi_stride * mi_row_offset];
#else
  const MODE_INFO *mi = xd->mi[0];
#endif  // CONFIG_MOTION_VAR
  const int is_compound = has_second_ref(&mi->mbmi);
  int ref;

  for (ref = 0; ref < 1 + is_compound; ++ref) {
    const struct scale_factors *const sf = &xd->block_refs[ref]->sf;
    struct buf_2d *const pre_buf = &pd->pre[ref];
    struct buf_2d *const dst_buf = &pd->dst;
    uint8_t *const dst = dst_buf->buf + dst_buf->stride * y + x;
    const MV mv = mi->mbmi.sb_type < BLOCK_8X8
                      ? average_split_mvs(pd, mi, ref, block)
                      : mi->mbmi.mv[ref].as_mv;

    // TODO(jkoleszar): This clamping is done in the incorrect place for the
    // scaling case. It needs to be done on the scaled MV, not the pre-scaling
    // MV. Note however that it performs the subsampling aware scaling so
    // that the result is always q4.
    // mv_precision precision is MV_PRECISION_Q4.
    const MV mv_q4 = clamp_mv_to_umv_border_sb(
        xd, &mv, bw, bh, pd->subsampling_x, pd->subsampling_y);

    uint8_t *pre;
    MV32 scaled_mv;
    int xs, ys, subpel_x, subpel_y;
    const int is_scaled = av1_is_scaled(sf);

    if (is_scaled) {
      pre = pre_buf->buf + scaled_buffer_offset(x, y, pre_buf->stride, sf);
      scaled_mv = av1_scale_mv(&mv_q4, mi_x + x, mi_y + y, sf);
      xs = sf->x_step_q4;
      ys = sf->y_step_q4;
    } else {
      pre = pre_buf->buf + (y * pre_buf->stride + x);
      scaled_mv.row = mv_q4.row;
      scaled_mv.col = mv_q4.col;
      xs = ys = 16;
    }
    subpel_x = scaled_mv.col & SUBPEL_MASK;
    subpel_y = scaled_mv.row & SUBPEL_MASK;
    pre += (scaled_mv.row >> SUBPEL_BITS) * pre_buf->stride +
           (scaled_mv.col >> SUBPEL_BITS);

#if CONFIG_AOM_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      high_inter_predictor(pre, pre_buf->stride, dst, dst_buf->stride, subpel_x,
                           subpel_y, sf, w, h, ref, &mi->mbmi.interp_filter, xs,
                           ys, xd->bd);
    } else {
      inter_predictor(pre, pre_buf->stride, dst, dst_buf->stride, subpel_x,
                      subpel_y, sf, w, h, ref, &mi->mbmi.interp_filter, xs, ys);
    }
#else
    inter_predictor(pre, pre_buf->stride, dst, dst_buf->stride, subpel_x,
                    subpel_y, sf, w, h, ref, &mi->mbmi.interp_filter, xs, ys);
#endif  // CONFIG_AOM_HIGHBITDEPTH
  }
}

void av1_build_inter_predictor_sub8x8(MACROBLOCKD *xd, int plane, int i, int ir,
                                      int ic, int mi_row, int mi_col) {
  struct macroblockd_plane *const pd = &xd->plane[plane];
  MODE_INFO *const mi = xd->mi[0];
  const BLOCK_SIZE plane_bsize = get_plane_block_size(mi->mbmi.sb_type, pd);
  const int width = 4 * num_4x4_blocks_wide_lookup[plane_bsize];
  const int height = 4 * num_4x4_blocks_high_lookup[plane_bsize];

  uint8_t *const dst = &pd->dst.buf[(ir * pd->dst.stride + ic) << 2];
  int ref;
  const int is_compound = has_second_ref(&mi->mbmi);

  for (ref = 0; ref < 1 + is_compound; ++ref) {
    const uint8_t *pre =
        &pd->pre[ref].buf[(ir * pd->pre[ref].stride + ic) << 2];
#if CONFIG_AOM_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      av1_highbd_build_inter_predictor(
          pre, pd->pre[ref].stride, dst, pd->dst.stride,
          &mi->bmi[i].as_mv[ref].as_mv, &xd->block_refs[ref]->sf, width, height,
          ref, &mi->mbmi.interp_filter, MV_PRECISION_Q3,
          mi_col * MI_SIZE + 4 * ic, mi_row * MI_SIZE + 4 * ir, xd->bd);
    } else {
      av1_build_inter_predictor(
          pre, pd->pre[ref].stride, dst, pd->dst.stride,
          &mi->bmi[i].as_mv[ref].as_mv, &xd->block_refs[ref]->sf, width, height,
          ref, &mi->mbmi.interp_filter, MV_PRECISION_Q3,
          mi_col * MI_SIZE + 4 * ic, mi_row * MI_SIZE + 4 * ir);
    }
#else
    av1_build_inter_predictor(
        pre, pd->pre[ref].stride, dst, pd->dst.stride,
        &mi->bmi[i].as_mv[ref].as_mv, &xd->block_refs[ref]->sf, width, height,
        ref, &mi->mbmi.interp_filter, MV_PRECISION_Q3,
        mi_col * MI_SIZE + 4 * ic, mi_row * MI_SIZE + 4 * ir);
#endif  // CONFIG_AOM_HIGHBITDEPTH
  }
}

static void build_inter_predictors_for_planes(MACROBLOCKD *xd, BLOCK_SIZE bsize,
                                              int mi_row, int mi_col,
                                              int plane_from, int plane_to) {
  int plane;
  const int mi_x = mi_col * MI_SIZE;
  const int mi_y = mi_row * MI_SIZE;
  for (plane = plane_from; plane <= plane_to; ++plane) {
    const struct macroblockd_plane *pd = &xd->plane[plane];
    const int bw = 4 * num_4x4_blocks_wide_lookup[bsize] >> pd->subsampling_x;
    const int bh = 4 * num_4x4_blocks_high_lookup[bsize] >> pd->subsampling_y;

    if (xd->mi[0]->mbmi.sb_type < BLOCK_8X8) {
      const PARTITION_TYPE bp = bsize - xd->mi[0]->mbmi.sb_type;
      const int have_vsplit = bp != PARTITION_HORZ;
      const int have_hsplit = bp != PARTITION_VERT;
      const int num_4x4_w = 2 >> ((!have_vsplit) | pd->subsampling_x);
      const int num_4x4_h = 2 >> ((!have_hsplit) | pd->subsampling_y);
      const int pw = 8 >> (have_vsplit | pd->subsampling_x);
      const int ph = 8 >> (have_hsplit | pd->subsampling_y);
      int x, y;
      assert(bp != PARTITION_NONE && bp < PARTITION_TYPES);
      assert(bsize == BLOCK_8X8);
      assert(pw * num_4x4_w == bw && ph * num_4x4_h == bh);
      for (y = 0; y < num_4x4_h; ++y)
        for (x = 0; x < num_4x4_w; ++x)
          build_inter_predictors(xd, plane,
#if CONFIG_MOTION_VAR
                                 0, 0,
#endif  // CONFIG_MOTION_VAR
                                 y * 2 + x, bw, bh, 4 * x, 4 * y, pw, ph, mi_x,
                                 mi_y);
    } else {
      build_inter_predictors(xd, plane,
#if CONFIG_MOTION_VAR
                             0, 0,
#endif  // CONFIG_MOTION_VAR
                             0, bw, bh, 0, 0, bw, bh, mi_x, mi_y);
    }
  }
}

void av1_build_inter_predictors_sby(MACROBLOCKD *xd, int mi_row, int mi_col,
                                    BLOCK_SIZE bsize) {
  build_inter_predictors_for_planes(xd, bsize, mi_row, mi_col, 0, 0);
}

void av1_build_inter_predictors_sbp(MACROBLOCKD *xd, int mi_row, int mi_col,
                                    BLOCK_SIZE bsize, int plane) {
  build_inter_predictors_for_planes(xd, bsize, mi_row, mi_col, plane, plane);
}

void av1_build_inter_predictors_sbuv(MACROBLOCKD *xd, int mi_row, int mi_col,
                                     BLOCK_SIZE bsize) {
  build_inter_predictors_for_planes(xd, bsize, mi_row, mi_col, 1,
                                    MAX_MB_PLANE - 1);
}

void av1_build_inter_predictors_sb(MACROBLOCKD *xd, int mi_row, int mi_col,
                                   BLOCK_SIZE bsize) {
  build_inter_predictors_for_planes(xd, bsize, mi_row, mi_col, 0,
                                    MAX_MB_PLANE - 1);
}

void av1_setup_dst_planes(struct macroblockd_plane planes[MAX_MB_PLANE],
                          const YV12_BUFFER_CONFIG *src, int mi_row,
                          int mi_col) {
  uint8_t *const buffers[MAX_MB_PLANE] = { src->y_buffer, src->u_buffer,
                                           src->v_buffer };
  const int strides[MAX_MB_PLANE] = { src->y_stride, src->uv_stride,
                                      src->uv_stride };
  int i;

  for (i = 0; i < MAX_MB_PLANE; ++i) {
    struct macroblockd_plane *const pd = &planes[i];
    setup_pred_plane(&pd->dst, buffers[i], strides[i], mi_row, mi_col, NULL,
                     pd->subsampling_x, pd->subsampling_y);
  }
}

void av1_setup_pre_planes(MACROBLOCKD *xd, int idx,
                          const YV12_BUFFER_CONFIG *src, int mi_row, int mi_col,
                          const struct scale_factors *sf) {
  if (src != NULL) {
    int i;
    uint8_t *const buffers[MAX_MB_PLANE] = { src->y_buffer, src->u_buffer,
                                             src->v_buffer };
    const int strides[MAX_MB_PLANE] = { src->y_stride, src->uv_stride,
                                        src->uv_stride };
    for (i = 0; i < MAX_MB_PLANE; ++i) {
      struct macroblockd_plane *const pd = &xd->plane[i];
      setup_pred_plane(&pd->pre[idx], buffers[i], strides[i], mi_row, mi_col,
                       sf, pd->subsampling_x, pd->subsampling_y);
    }
  }
}

#if CONFIG_MOTION_VAR
#define OBMC_MASK_PREC_BITS 6
// obmc_mask_N[is_neighbor_predictor][overlap_position]
static const uint8_t obmc_mask_1[2][1] = { { 55 }, { 9 } };

static const uint8_t obmc_mask_2[2][2] = { { 45, 62 }, { 19, 2 } };

static const uint8_t obmc_mask_4[2][4] = { { 39, 50, 59, 64 },
                                           { 25, 14, 5, 0 } };

static const uint8_t obmc_mask_8[2][8] = { { 36, 42, 48, 53, 57, 61, 63, 64 },
                                           { 28, 22, 16, 11, 7, 3, 1, 0 } };

static const uint8_t obmc_mask_16[2][16] = {
  { 34, 37, 40, 43, 46, 49, 52, 54, 56, 58, 60, 61, 63, 64, 64, 64 },
  { 30, 27, 24, 21, 18, 15, 12, 10, 8, 6, 4, 3, 1, 0, 0, 0 }
};

static const uint8_t obmc_mask_32[2][32] = {
  { 33, 35, 36, 38, 40, 41, 43, 44, 45, 47, 48, 50, 51, 52, 53, 55, 56, 57, 58,
    59, 60, 60, 61, 62, 62, 63, 63, 64, 64, 64, 64, 64 },
  { 31, 29, 28, 26, 24, 23, 21, 20, 19, 17, 16, 14, 13, 12, 11, 9, 8, 7, 6, 5,
    4, 4, 3, 2, 2, 1, 1, 0, 0, 0, 0, 0 }
};

void setup_obmc_mask(int length, const uint8_t *mask[2]) {
  switch (length) {
    case 1:
      mask[0] = obmc_mask_1[0];
      mask[1] = obmc_mask_1[1];
      break;
    case 2:
      mask[0] = obmc_mask_2[0];
      mask[1] = obmc_mask_2[1];
      break;
    case 4:
      mask[0] = obmc_mask_4[0];
      mask[1] = obmc_mask_4[1];
      break;
    case 8:
      mask[0] = obmc_mask_8[0];
      mask[1] = obmc_mask_8[1];
      break;
    case 16:
      mask[0] = obmc_mask_16[0];
      mask[1] = obmc_mask_16[1];
      break;
    case 32:
      mask[0] = obmc_mask_32[0];
      mask[1] = obmc_mask_32[1];
      break;
    default:
      mask[0] = NULL;
      mask[1] = NULL;
      assert(0);
      break;
  }
}

// This function combines motion compensated predictions that is generated by
// top/left neighboring blocks' inter predictors with the regular inter
// prediction. We assume the original prediction (bmc) is stored in
// xd->plane[].dst.buf
void av1_build_obmc_inter_prediction(
    AV1_COMMON *cm, MACROBLOCKD *xd, int mi_row, int mi_col,
    int use_tmp_dst_buf, uint8_t *final_buf[MAX_MB_PLANE],
    int final_stride[MAX_MB_PLANE], uint8_t *above_pred_buf[MAX_MB_PLANE],
    int above_pred_stride[MAX_MB_PLANE], uint8_t *left_pred_buf[MAX_MB_PLANE],
    int left_pred_stride[MAX_MB_PLANE]) {
  const TileInfo *const tile = &xd->tile;
  BLOCK_SIZE bsize = xd->mi[0]->mbmi.sb_type;
  int plane, i, mi_step;
  const int above_available = mi_row > tile->mi_row_start;
#if CONFIG_AOM_HIGHBITDEPTH
  int is_hbd = (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) ? 1 : 0;
#endif  // CONFIG_AOM_HIGHBITDEPTH

  if (use_tmp_dst_buf) {
    for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
      const struct macroblockd_plane *pd = &xd->plane[plane];
      int bw = (xd->n8_w << 3) >> pd->subsampling_x;
      int bh = (xd->n8_h << 3) >> pd->subsampling_y;
      int row;
#if CONFIG_AOM_HIGHBITDEPTH
      if (is_hbd) {
        uint16_t *final_buf16 = CONVERT_TO_SHORTPTR(final_buf[plane]);
        uint16_t *bmc_buf16 = CONVERT_TO_SHORTPTR(pd->dst.buf);
        for (row = 0; row < bh; ++row)
          memcpy(final_buf16 + row * final_stride[plane],
                 bmc_buf16 + row * pd->dst.stride, bw * sizeof(uint16_t));
      } else {
#endif
        for (row = 0; row < bh; ++row)
          memcpy(final_buf[plane] + row * final_stride[plane],
                 pd->dst.buf + row * pd->dst.stride, bw);
#if CONFIG_AOM_HIGHBITDEPTH
      }
#endif  // CONFIG_AOM_HIGHBITDEPTH
    }
  }

  // handle above row
  for (i = 0; above_available && i < AOMMIN(xd->n8_w, cm->mi_cols - mi_col);
       i += mi_step) {
    int mi_row_offset = -1;
    int mi_col_offset = i;
    MODE_INFO *mi = xd->mi[mi_col_offset + mi_row_offset * xd->mi_stride];
    MB_MODE_INFO *mbmi = &mi->mbmi;
    int overlap;

    mi_step = AOMMIN(xd->n8_w, num_8x8_blocks_wide_lookup[mbmi->sb_type]);

    if (!is_neighbor_overlappable(mbmi)) continue;

    overlap = num_4x4_blocks_high_lookup[bsize] << 1;

    for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
      const struct macroblockd_plane *pd = &xd->plane[plane];
      int bw = (mi_step << 3) >> pd->subsampling_x;
      int bh = overlap >> pd->subsampling_y;
      int row, col;
      int dst_stride = use_tmp_dst_buf ? final_stride[plane] : pd->dst.stride;
      uint8_t *dst = use_tmp_dst_buf
                         ? &final_buf[plane][(i << 3) >> pd->subsampling_x]
                         : &pd->dst.buf[(i << 3) >> pd->subsampling_x];
      int tmp_stride = above_pred_stride[plane];
      uint8_t *tmp = &above_pred_buf[plane][(i << 3) >> pd->subsampling_x];
      const uint8_t *mask[2];

      setup_obmc_mask(bh, mask);

#if CONFIG_AOM_HIGHBITDEPTH
      if (is_hbd) {
        uint16_t *dst16 = CONVERT_TO_SHORTPTR(dst);
        uint16_t *tmp16 = CONVERT_TO_SHORTPTR(tmp);

        for (row = 0; row < bh; ++row) {
          for (col = 0; col < bw; ++col)
            dst16[col] = ROUND_POWER_OF_TWO(
                mask[0][row] * dst16[col] + mask[1][row] * tmp16[col],
                OBMC_MASK_PREC_BITS);

          dst16 += dst_stride;
          tmp16 += tmp_stride;
        }
      } else {
#endif  // CONFIG_AOM_HIGHBITDEPTH
        for (row = 0; row < bh; ++row) {
          for (col = 0; col < bw; ++col)
            dst[col] = ROUND_POWER_OF_TWO(
                mask[0][row] * dst[col] + mask[1][row] * tmp[col],
                OBMC_MASK_PREC_BITS);
          dst += dst_stride;
          tmp += tmp_stride;
        }
#if CONFIG_AOM_HIGHBITDEPTH
      }
#endif  // CONFIG_AOM_HIGHBITDEPTH
    }
  }  // each mi in the above row

  // handle left column
  if (mi_col - 1 < tile->mi_col_start) return;

  for (i = 0; i < AOMMIN(xd->n8_h, cm->mi_rows - mi_row); i += mi_step) {
    int mi_row_offset = i;
    int mi_col_offset = -1;
    int overlap;
    MODE_INFO *mi = xd->mi[mi_col_offset + mi_row_offset * xd->mi_stride];
    MB_MODE_INFO *mbmi = &mi->mbmi;

    mi_step = AOMMIN(xd->n8_h, num_8x8_blocks_high_lookup[mbmi->sb_type]);

    if (!is_neighbor_overlappable(mbmi)) continue;

    overlap = num_4x4_blocks_wide_lookup[bsize] << 1;

    for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
      const struct macroblockd_plane *pd = &xd->plane[plane];
      int bw = overlap >> pd->subsampling_x;
      int bh = (mi_step << 3) >> pd->subsampling_y;
      int row, col;
      int dst_stride = use_tmp_dst_buf ? final_stride[plane] : pd->dst.stride;
      uint8_t *dst =
          use_tmp_dst_buf
              ? &final_buf[plane][((i << 3) >> pd->subsampling_y) * dst_stride]
              : &pd->dst.buf[((i << 3) >> pd->subsampling_y) * dst_stride];
      int tmp_stride = left_pred_stride[plane];
      uint8_t *tmp =
          &left_pred_buf[plane][((i << 3) >> pd->subsampling_y) * tmp_stride];
      const uint8_t *mask[2];

      setup_obmc_mask(bw, mask);

#if CONFIG_AOM_HIGHBITDEPTH
      if (is_hbd) {
        uint16_t *dst16 = CONVERT_TO_SHORTPTR(dst);
        uint16_t *tmp16 = CONVERT_TO_SHORTPTR(tmp);

        for (row = 0; row < bh; ++row) {
          for (col = 0; col < bw; ++col)
            dst16[col] = ROUND_POWER_OF_TWO(
                mask[0][col] * dst16[col] + mask[1][col] * tmp16[col],
                OBMC_MASK_PREC_BITS);
          dst16 += dst_stride;
          tmp16 += tmp_stride;
        }
      } else {
#endif  // CONFIG_AOM_HIGHBITDEPTH
        for (row = 0; row < bh; ++row) {
          for (col = 0; col < bw; ++col)
            dst[col] = ROUND_POWER_OF_TWO(
                mask[0][col] * dst[col] + mask[1][col] * tmp[col],
                OBMC_MASK_PREC_BITS);
          dst += dst_stride;
          tmp += tmp_stride;
        }
#if CONFIG_AOM_HIGHBITDEPTH
      }
#endif  // CONFIG_AOM_HIGHBITDEPTH
    }
  }  // each mi in the left column
}

void av1_build_prediction_by_above_preds(AV1_COMMON *cm, MACROBLOCKD *xd,
                                         int mi_row, int mi_col,
                                         uint8_t *tmp_buf[MAX_MB_PLANE],
                                         int tmp_stride[MAX_MB_PLANE]) {
  const TileInfo *const tile = &xd->tile;
  BLOCK_SIZE bsize = xd->mi[0]->mbmi.sb_type;
  int i, j, mi_step, ref;

  if (mi_row <= tile->mi_row_start) return;

  for (i = 0; i < AOMMIN(xd->n8_w, cm->mi_cols - mi_col); i += mi_step) {
    int mi_row_offset = -1;
    int mi_col_offset = i;
    int mi_x, mi_y, bw, bh;
    MODE_INFO *mi = xd->mi[mi_col_offset + mi_row_offset * xd->mi_stride];
    MB_MODE_INFO *mbmi = &mi->mbmi;

    mi_step = AOMMIN(xd->n8_w, num_8x8_blocks_wide_lookup[mbmi->sb_type]);

    if (!is_neighbor_overlappable(mbmi)) continue;

    for (j = 0; j < MAX_MB_PLANE; ++j) {
      struct macroblockd_plane *const pd = &xd->plane[j];
      setup_pred_plane(&pd->dst, tmp_buf[j], tmp_stride[j], 0, i, NULL,
                       pd->subsampling_x, pd->subsampling_y);
    }
    for (ref = 0; ref < 1 + has_second_ref(mbmi); ++ref) {
      MV_REFERENCE_FRAME frame = mbmi->ref_frame[ref];
      RefBuffer *ref_buf = &cm->frame_refs[frame - LAST_FRAME];

      xd->block_refs[ref] = ref_buf;
      if ((!av1_is_valid_scale(&ref_buf->sf)))
        aom_internal_error(xd->error_info, AOM_CODEC_UNSUP_BITSTREAM,
                           "Reference frame has invalid dimensions");
      av1_setup_pre_planes(xd, ref, ref_buf->buf, mi_row, mi_col + i,
                           &ref_buf->sf);
    }

    xd->mb_to_left_edge = -(((mi_col + i) * MI_SIZE) * 8);
    mi_x = (mi_col + i) << MI_SIZE_LOG2;
    mi_y = mi_row << MI_SIZE_LOG2;

    for (j = 0; j < MAX_MB_PLANE; ++j) {
      const struct macroblockd_plane *pd = &xd->plane[j];
      bw = (mi_step << MI_SIZE_LOG2) >> pd->subsampling_x;
      bh = AOMMAX((num_4x4_blocks_high_lookup[bsize] << 1) >> pd->subsampling_y,
                  4);

      if (mbmi->sb_type < BLOCK_8X8) {
        const PARTITION_TYPE bp = BLOCK_8X8 - mbmi->sb_type;
        const int have_vsplit = bp != PARTITION_HORZ;
        const int have_hsplit = bp != PARTITION_VERT;
        const int num_4x4_w = 2 >> ((!have_vsplit) | pd->subsampling_x);
        const int num_4x4_h = 2 >> ((!have_hsplit) | pd->subsampling_y);
        const int pw = 8 >> (have_vsplit | pd->subsampling_x);
        int x, y;

        for (y = 0; y < num_4x4_h; ++y)
          for (x = 0; x < num_4x4_w; ++x) {
            if ((bp == PARTITION_HORZ || bp == PARTITION_SPLIT) && y == 0 &&
                !pd->subsampling_y)
              continue;

            build_inter_predictors(xd, j, mi_col_offset, mi_row_offset,
                                   y * 2 + x, bw, bh, 4 * x, 0, pw, bh, mi_x,
                                   mi_y);
          }
      } else {
        build_inter_predictors(xd, j, mi_col_offset, mi_row_offset, 0, bw, bh,
                               0, 0, bw, bh, mi_x, mi_y);
      }
    }
  }
  xd->mb_to_left_edge = -((mi_col * MI_SIZE) * 8);
}

void av1_build_prediction_by_left_preds(AV1_COMMON *cm, MACROBLOCKD *xd,
                                        int mi_row, int mi_col,
                                        uint8_t *tmp_buf[MAX_MB_PLANE],
                                        int tmp_stride[MAX_MB_PLANE]) {
  const TileInfo *const tile = &xd->tile;
  BLOCK_SIZE bsize = xd->mi[0]->mbmi.sb_type;
  int i, j, mi_step, ref;

  if (mi_col - 1 < tile->mi_col_start) return;

  for (i = 0; i < AOMMIN(xd->n8_h, cm->mi_rows - mi_row); i += mi_step) {
    int mi_row_offset = i;
    int mi_col_offset = -1;
    int mi_x, mi_y, bw, bh;
    MODE_INFO *mi = xd->mi[mi_col_offset + mi_row_offset * xd->mi_stride];
    MB_MODE_INFO *mbmi = &mi->mbmi;

    mi_step = AOMMIN(xd->n8_h, num_8x8_blocks_high_lookup[mbmi->sb_type]);

    if (!is_neighbor_overlappable(mbmi)) continue;

    for (j = 0; j < MAX_MB_PLANE; ++j) {
      struct macroblockd_plane *const pd = &xd->plane[j];
      setup_pred_plane(&pd->dst, tmp_buf[j], tmp_stride[j], i, 0, NULL,
                       pd->subsampling_x, pd->subsampling_y);
    }
    for (ref = 0; ref < 1 + has_second_ref(mbmi); ++ref) {
      MV_REFERENCE_FRAME frame = mbmi->ref_frame[ref];
      RefBuffer *ref_buf = &cm->frame_refs[frame - LAST_FRAME];

      xd->block_refs[ref] = ref_buf;
      if ((!av1_is_valid_scale(&ref_buf->sf)))
        aom_internal_error(xd->error_info, AOM_CODEC_UNSUP_BITSTREAM,
                           "Reference frame has invalid dimensions");
      av1_setup_pre_planes(xd, ref, ref_buf->buf, mi_row + i, mi_col,
                           &ref_buf->sf);
    }

    xd->mb_to_top_edge = -(((mi_row + i) * MI_SIZE) * 8);
    mi_x = mi_col << MI_SIZE_LOG2;
    mi_y = (mi_row + i) << MI_SIZE_LOG2;

    for (j = 0; j < MAX_MB_PLANE; ++j) {
      const struct macroblockd_plane *pd = &xd->plane[j];
      bw = AOMMAX((num_4x4_blocks_wide_lookup[bsize] << 1) >> pd->subsampling_x,
                  4);
      bh = (mi_step << MI_SIZE_LOG2) >> pd->subsampling_y;

      if (mbmi->sb_type < BLOCK_8X8) {
        const PARTITION_TYPE bp = BLOCK_8X8 - mbmi->sb_type;
        const int have_vsplit = bp != PARTITION_HORZ;
        const int have_hsplit = bp != PARTITION_VERT;
        const int num_4x4_w = 2 >> ((!have_vsplit) | pd->subsampling_x);
        const int num_4x4_h = 2 >> ((!have_hsplit) | pd->subsampling_y);
        const int ph = 8 >> (have_hsplit | pd->subsampling_y);
        int x, y;

        for (y = 0; y < num_4x4_h; ++y)
          for (x = 0; x < num_4x4_w; ++x) {
            if ((bp == PARTITION_VERT || bp == PARTITION_SPLIT) && x == 0 &&
                !pd->subsampling_x)
              continue;

            build_inter_predictors(xd, j, mi_col_offset, mi_row_offset,
                                   y * 2 + x, bw, bh, 0, 4 * y, bw, ph, mi_x,
                                   mi_y);
          }
      } else {
        build_inter_predictors(xd, j, mi_col_offset, mi_row_offset, 0, bw, bh,
                               0, 0, bw, bh, mi_x, mi_y);
      }
    }
  }
  xd->mb_to_top_edge = -((mi_row * MI_SIZE) * 8);
}
#endif  // CONFIG_MOTION_VAR
