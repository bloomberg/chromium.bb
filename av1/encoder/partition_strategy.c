/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "aom_ports/system_state.h"

#include "av1/common/enums.h"
#include "av1/common/reconinter.h"

#include "av1/encoder/encoder.h"
#include "av1/encoder/partition_model_weights.h"
#include "av1/encoder/partition_strategy.h"

// Performs a simple_motion_search with a single reference frame and extract
// the variance of residues. Here features is assumed to be a length 6 array.
// After this function is called, we will store the following in to features:
// features[0] = log(1 + dc_q**2/256)
// features[1] = log(1 + variance_of_residue)
// for i in [2, 3, 4, 5]:
//  features[i] = log(1 + variance_of_residue_in_block[i]/variance_of_residue)
static void get_res_var_features(AV1_COMP *const cpi, MACROBLOCK *x, int mi_row,
                                 int mi_col, BLOCK_SIZE bsize,
                                 float *features) {
  // TODO(chiyotsai@google.com): The data this model trained on did not also use
  // SIMPLE_TRANSLATION to build the inter_predictor. Retraining and tuning the
  // model with the correct data should give better performance.
  assert(mi_size_wide[bsize] == mi_size_high[bsize]);

  MACROBLOCKD *xd = &x->e_mbd;

  // Perform a single motion search in Y_PLANE to make a prediction
  const int use_subpixel = 0;

  // Start getting the features
  int f_idx = 0;

  // Q_INDEX
  const int dc_q = av1_dc_quant_QTX(x->qindex, 0, xd->bd) >> (xd->bd - 8);
  aom_clear_system_state();
  features[f_idx++] = logf(1.0f + (float)(dc_q * dc_q) / 256.0f);

  // VARIANCE
  unsigned int sse = 0;
  unsigned int var = 0;
  const MV ref_mv_full = { .row = 0, .col = 0 };
  av1_simple_motion_sse_var(cpi, x, mi_row, mi_col, bsize, ref_mv_full,
                            use_subpixel, &sse, &var);
  aom_clear_system_state();
  features[f_idx++] = logf(1.0f + (float)var);

  // Regional
  const uint8_t *src = x->plane[0].src.buf;
  const int src_stride = x->plane[0].src.stride;
  const uint8_t *dst = xd->plane[0].dst.buf;
  const int dst_stride = xd->plane[0].dst.stride;
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  const BLOCK_SIZE subsize = get_partition_subsize(bsize, PARTITION_SPLIT);
  int r_idx = 0;
  for (r_idx = 0; r_idx < 4; r_idx++) {
    const int x_idx = (r_idx & 1) * bw / 2;
    const int y_idx = (r_idx >> 1) * bh / 2;
    const int src_offset = y_idx * src_stride + x_idx;
    const int dst_offset = y_idx * dst_stride + x_idx;
    const unsigned int sub_var = cpi->fn_ptr[subsize].vf(
        src + src_offset, src_stride, dst + dst_offset, dst_stride, &sse);
    aom_clear_system_state();
    const float var_ratio = (1.0f + (float)sub_var) / (4.0f + (float)var);
    features[f_idx++] = var_ratio;
  }
}

void av1_simple_motion_search_based_split(
    AV1_COMP *const cpi, MACROBLOCK *x, int mi_row, int mi_col,
    BLOCK_SIZE bsize, int *partition_none_allowed, int *partition_horz_allowed,
    int *partition_vert_allowed, int *do_rectangular_split) {
  const NN_CONFIG *nn_config = NULL;
  float split_only_thresh = 0.0f;
  if (bsize == BLOCK_128X128) {
    nn_config = &av1_simple_motion_search_based_split_nn_config_128;
    split_only_thresh = av1_simple_motion_search_based_split_thresh_128;
  } else if (bsize == BLOCK_64X64) {
    nn_config = &av1_simple_motion_search_based_split_nn_config_64;
    split_only_thresh = av1_simple_motion_search_based_split_thresh_64;
  } else if (bsize == BLOCK_32X32) {
    nn_config = &av1_simple_motion_search_based_split_nn_config_32;
    split_only_thresh = av1_simple_motion_search_based_split_thresh_32;
  } else if (bsize == BLOCK_16X16) {
    nn_config = &av1_simple_motion_search_based_split_nn_config_16;
    split_only_thresh = av1_simple_motion_search_based_split_thresh_16;
  } else if (bsize == BLOCK_8X8) {
    // Disable BLOCK_8X8 for now
#if !CONFIG_DISABLE_FULL_PIXEL_SPLIT_8X8
    nn_config = &av1_simple_motion_search_based_split_nn_config_8;
    split_only_thresh = av1_simple_motion_search_based_split_thresh_8;
#endif
  } else {
    assert(0 && "Unexpected block size in simple_motion_based_split");
  }
  if (nn_config) {
    float features[6] = { 0 };
    float score = 0;
    get_res_var_features(cpi, x, mi_row, mi_col, bsize, features);
    av1_nn_predict(features, nn_config, &score);

    if (score > split_only_thresh) {
      *partition_none_allowed = 0;
      *partition_horz_allowed = 0;
      *partition_vert_allowed = 0;
      *do_rectangular_split = 0;
    }
  }
}
