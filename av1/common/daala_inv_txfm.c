/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "./av1_rtcd.h"
#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"
#include "av1/common/daala_tx.h"
#include "av1/common/daala_inv_txfm.h"
#include "av1/common/idct.h"

#if CONFIG_DAALA_TX

// Complete Daala TX map, sans lossless which is special cased
typedef void (*daala_itx)(od_coeff *, int, const od_coeff[]);

static daala_itx tx_map[TX_SIZES][TX_TYPES] = {
  //  4-point transforms
  { od_bin_idct4, od_bin_idst4, od_bin_idst4, od_bin_iidtx4 },

  //  8-point transforms
  { od_bin_idct8, od_bin_idst8, od_bin_idst8, od_bin_iidtx8 },

  //  16-point transforms
  { od_bin_idct16, od_bin_idst16, od_bin_idst16, od_bin_iidtx16 },

  //  32-point transforms
  { od_bin_idct32, od_bin_idst32, od_bin_idst32, od_bin_iidtx32 },

#if CONFIG_TX64X64
  //  64-point transforms
  { od_bin_idct64, NULL, NULL, od_bin_iidtx64 },
#endif
};

static int tx_flip(TX_TYPE_1D t) { return t == FLIPADST_1D; }

// Daala TX toplevel inverse entry point.  This same function is
// intended for both low and high bitdepth cases with a tran_low_t of
// 32 bits (matching od_coeff), and a passed-in pixel buffer of either
// bytes (hbd=0) or shorts (hbd=1).
void daala_inv_txfm_add(const tran_low_t *input_coeffs, void *output_pixels,
                        int output_stride, TxfmParam *txfm_param) {
  const TX_SIZE tx_size = txfm_param->tx_size;
  const TX_TYPE tx_type = txfm_param->tx_type;
  const int px_depth = txfm_param->bd;
  assert(tx_size <= TX_SIZES_ALL);
  assert(tx_type <= TX_TYPES);

  if (txfm_param->lossless) {
    // Transform function special-cased for lossless
    assert(tx_type == DCT_DCT);
    assert(tx_size == TX_4X4);
    if (txfm_param->is_hbd)
      av1_iwht4x4_add(input_coeffs, output_pixels, output_stride, txfm_param);
    else
      // Note that the output pointer in the prototype is uint8, but the
      // function converts to short internally
      av1_highbd_iwht4x4_add(input_coeffs, output_pixels, output_stride,
                             txfm_param->eob, px_depth);
  } else {
    // General TX case
    const int downshift = TX_COEFF_DEPTH - px_depth;
    assert(downshift >= 0);
    assert(sizeof(tran_low_t) == sizeof(od_coeff));
    assert(sizeof(tran_low_t) >= 4);

    // Hook into existing map translation infrastructure to select
    // appropriate TX functions
    const int cols = tx_size_wide[tx_size];
    const int rows = tx_size_high[tx_size];
    const TX_SIZE col_idx = txsize_vert_map[tx_size];
    const TX_SIZE row_idx = txsize_horz_map[tx_size];
    assert(col_idx <= TX_SIZES);
    assert(row_idx <= TX_SIZES);
    assert(vtx_tab[tx_type] <= (int)TX_TYPES_1D);
    assert(htx_tab[tx_type] <= (int)TX_TYPES_1D);
    daala_itx col_tx = tx_map[col_idx][vtx_tab[tx_type]];
    daala_itx row_tx = tx_map[row_idx][htx_tab[tx_type]];
    int col_flip = tx_flip(vtx_tab[tx_type]);
    int row_flip = tx_flip(htx_tab[tx_type]);
    od_coeff tmpsq[MAX_TX_SQUARE];
    int r;
    int c;

    assert(col_tx);
    assert(row_tx);

    // Inverse-transform rows
    for (r = 0; r < rows; ++r) {
      // The output addressing transposes
      if (row_flip)
        row_tx(tmpsq + r + (rows * cols) - rows, -rows,
               input_coeffs + r * cols);
      else
        row_tx(tmpsq + r, rows, input_coeffs + r * cols);
    }

    // Inverse-transform columns
    for (c = 0; c < cols; ++c) {
      // Above transposed, so our cols are now rows
      if (col_flip)
        col_tx(tmpsq + c * rows + rows - 1, -1, tmpsq + c * rows);
      else
        col_tx(tmpsq + c * rows, 1, tmpsq + c * rows);
    }

    // Sum with destination according to bit depth
    // The tmpsq array is currently transposed relative to output
    if (txfm_param->is_hbd) {
      // Destination array is shorts
      uint16_t *out16 = CONVERT_TO_SHORTPTR(output_pixels);
      for (r = 0; r < rows; ++r)
        for (c = 0; c < cols; ++c)
          out16[r * output_stride + c] = highbd_clip_pixel_add(
              out16[r * output_stride + c],
              (tmpsq[c * rows + r] + (1 << downshift >> 1)) >> downshift,
              px_depth);
    } else {
      // Destination array is bytes
      uint8_t *out8 = (uint8_t *)output_pixels;
      for (r = 0; r < rows; ++r)
        for (c = 0; c < cols; ++c)
          out8[r * output_stride + c] = clip_pixel_add(
              out8[r * output_stride + c],
              (tmpsq[c * rows + r] + (1 << downshift >> 1)) >> downshift);
    }
  }
}

#endif
