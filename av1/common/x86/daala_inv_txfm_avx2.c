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

typedef void (*daala_row_itx)(int16_t *out, int rows, const tran_low_t *in);
typedef void (*daala_col_itx_add)(unsigned char *output_pixels,
                                  int output_stride, int cols,
                                  const int16_t *in, int bd);

static const daala_row_itx TX_ROW_MAP[TX_SIZES][TX_TYPES] = {
  // 4-point transforms
  { NULL, NULL, NULL, NULL },
  // 8-point transforms
  { NULL, NULL, NULL, NULL },
  // 16-point transforms
  { NULL, NULL, NULL, NULL },
  // 32-point transforms
  { NULL, NULL, NULL, NULL },
#if CONFIG_TX64X64
  // 64-point transforms
  { NULL, NULL, NULL, NULL },
#endif
};

static const daala_col_itx_add TX_COL_MAP[2][TX_SIZES][TX_TYPES] = {
  // Low bit depth output
  {
      // 4-point transforms
      { NULL, NULL, NULL, NULL },
      // 8-point transforms
      { NULL, NULL, NULL, NULL },
      // 16-point transforms
      { NULL, NULL, NULL, NULL },
      // 32-point transforms
      { NULL, NULL, NULL, NULL },
#if CONFIG_TX64X64
      // 64-point transforms
      { NULL, NULL, NULL, NULL },
#endif
  },
  // High bit depth output
  {
      // 4-point transforms
      { NULL, NULL, NULL, NULL },
      // 8-point transforms
      { NULL, NULL, NULL, NULL },
      // 16-point transforms
      { NULL, NULL, NULL, NULL },
      // 32-point transforms
      { NULL, NULL, NULL, NULL },
#if CONFIG_TX64X64
      // 64-point transforms
      { NULL, NULL, NULL, NULL },
#endif
  }
};

/* Define this to verify the SIMD against the C versions of the transforms.
   This is intended to be replaced by real unit tests in the future. */
#undef DAALA_TX_VERIFY_SIMD

void daala_inv_txfm_add_avx2(const tran_low_t *input_coeffs,
                             void *output_pixels, int output_stride,
                             TxfmParam *txfm_param) {
  const TX_SIZE tx_size = txfm_param->tx_size;
  const TX_TYPE tx_type = txfm_param->tx_type;
  assert(tx_size <= TX_SIZES_ALL);
  assert(tx_type <= TX_TYPES);

  if (txfm_param->lossless) {
    daala_inv_txfm_add_c(input_coeffs, output_pixels, output_stride,
                         txfm_param);
  } else {
    // General TX case
    assert(sizeof(tran_low_t) == sizeof(od_coeff));
    assert(sizeof(tran_low_t) >= 4);

    // Hook into existing map translation infrastructure to select
    // appropriate TX functions
    const TX_SIZE col_idx = txsize_vert_map[tx_size];
    const TX_SIZE row_idx = txsize_horz_map[tx_size];
    assert(col_idx <= TX_SIZES);
    assert(row_idx <= TX_SIZES);
    assert(vtx_tab[tx_type] <= (int)TX_TYPES_1D);
    assert(htx_tab[tx_type] <= (int)TX_TYPES_1D);
    daala_row_itx row_tx = TX_ROW_MAP[row_idx][htx_tab[tx_type]];
    daala_col_itx_add col_tx =
        TX_COL_MAP[txfm_param->is_hbd][col_idx][vtx_tab[tx_type]];
    int16_t tmpsq[MAX_TX_SQUARE];

    if (row_tx == NULL || col_tx == NULL) {
      daala_inv_txfm_add_c(input_coeffs, output_pixels, output_stride,
                           txfm_param);
    } else {
      const int cols = tx_size_wide[tx_size];
      const int rows = tx_size_high[tx_size];
#if defined(DAALA_TX_VERIFY_SIMD)
      unsigned char out_check_buf8[MAX_TX_SQUARE];
      int16_t out_check_buf16[MAX_TX_SQUARE];
      unsigned char *out_check_buf;
      {
        if (txfm_param->is_hbd) {
          uint16_t *output_pixels16;
          int r;
          output_pixels16 = CONVERT_TO_SHORTPTR(output_pixels);
          for (r = 0; r < rows; r++) {
            memcpy(out_check_buf16 + r * cols,
                   output_pixels16 + r * output_stride,
                   cols * sizeof(*out_check_buf16));
          }
          out_check_buf = CONVERT_TO_BYTEPTR(out_check_buf16);
        } else {
          unsigned char *output_pixels8;
          int r;
          output_pixels8 = (unsigned char *)output_pixels;
          for (r = 0; r < rows; r++) {
            memcpy(out_check_buf8 + r * cols,
                   output_pixels8 + r * output_stride,
                   cols * sizeof(*out_check_buf8));
          }
          out_check_buf = out_check_buf8;
        }
      }
      daala_inv_txfm_add_c(input_coeffs, out_check_buf, cols, txfm_param);
#endif
      // Inverse-transform rows
      row_tx(tmpsq, rows, input_coeffs);
      // Inverse-transform columns and sum with destination
      col_tx(output_pixels, output_stride, cols, tmpsq, txfm_param->bd);
#if defined(DAALA_TX_VERIFY_SIMD)
      {
        if (txfm_param->is_hbd) {
          uint16_t *output_pixels16;
          int r;
          output_pixels16 = CONVERT_TO_SHORTPTR(output_pixels);
          for (r = 0; r < rows; r++) {
            if (memcmp(out_check_buf16 + r * cols,
                       output_pixels16 + r * output_stride,
                       cols * sizeof(*out_check_buf16))) {
              fprintf(stderr, "%s(%i): Inverse %ix%i %i_%i TX SIMD mismatch.\n",
                      __FILE__, __LINE__, rows, cols, vtx_tab[tx_type],
                      htx_tab[tx_type]);
              assert(0);
              exit(EXIT_FAILURE);
            }
          }
        } else {
          unsigned char *output_pixels8;
          int r;
          output_pixels8 = (unsigned char *)output_pixels;
          for (r = 0; r < rows; r++) {
            if (memcmp(out_check_buf8 + r * cols,
                       output_pixels8 + r * output_stride,
                       cols * sizeof(*out_check_buf8))) {
              fprintf(stderr, "%s(%i): Inverse %ix%i %i_%i TX SIMD mismatch.\n",
                      __FILE__, __LINE__, rows, cols, vtx_tab[tx_type],
                      htx_tab[tx_type]);
              assert(0);
              exit(EXIT_FAILURE);
            }
          }
        }
      }
#endif
    }
  }
}

#endif
