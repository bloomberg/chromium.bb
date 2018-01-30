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

#include "./aom_dsp_rtcd.h"
#include "av1/common/av1_txfm.h"

void av1_round_shift_array_c(int32_t *arr, int size, int bit) {
  int i;
  if (bit == 0) {
    return;
  } else {
    if (bit > 0) {
      for (i = 0; i < size; i++) {
        arr[i] = round_shift(arr[i], bit);
      }
    } else {
      for (i = 0; i < size; i++) {
        arr[i] = arr[i] * (1 << (-bit));
      }
    }
  }
}

const TXFM_TYPE av1_txfm_type_ls[5][TX_TYPES_1D] = {
  { TXFM_TYPE_DCT4, TXFM_TYPE_ADST4, TXFM_TYPE_ADST4, TXFM_TYPE_IDENTITY4 },
  { TXFM_TYPE_DCT8, TXFM_TYPE_ADST8, TXFM_TYPE_ADST8, TXFM_TYPE_IDENTITY8 },
  { TXFM_TYPE_DCT16, TXFM_TYPE_ADST16, TXFM_TYPE_ADST16, TXFM_TYPE_IDENTITY16 },
  { TXFM_TYPE_DCT32, TXFM_TYPE_ADST32, TXFM_TYPE_ADST32, TXFM_TYPE_IDENTITY32 },
  { TXFM_TYPE_DCT64, TXFM_TYPE_INVALID, TXFM_TYPE_INVALID,
    TXFM_TYPE_IDENTITY64 }
};

const int8_t av1_txfm_stage_num_list[TXFM_TYPES] = {
  4,   // TXFM_TYPE_DCT4
  6,   // TXFM_TYPE_DCT8
  8,   // TXFM_TYPE_DCT16
  10,  // TXFM_TYPE_DCT32
  12,  // TXFM_TYPE_DCT64
  7,   // TXFM_TYPE_ADST4
  8,   // TXFM_TYPE_ADST8
  10,  // TXFM_TYPE_ADST16
  12,  // TXFM_TYPE_ADST32
  1,   // TXFM_TYPE_IDENTITY4
  1,   // TXFM_TYPE_IDENTITY8
  1,   // TXFM_TYPE_IDENTITY16
  1,   // TXFM_TYPE_IDENTITY32
  1,   // TXFM_TYPE_IDENTITY64
};
