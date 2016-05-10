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

#include <math.h>

#include "./av1_rtcd.h"
#include "./aom_dsp_rtcd.h"
#include "av1/common/blockd.h"
#include "av1/common/idct.h"
#include "aom_dsp/inv_txfm.h"
#include "aom_ports/mem.h"

void av1_iht4x4_16_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                         int tx_type) {
  const transform_2d IHT_4[] = {
    { idct4_c, idct4_c },   // DCT_DCT  = 0
    { iadst4_c, idct4_c },  // ADST_DCT = 1
    { idct4_c, iadst4_c },  // DCT_ADST = 2
    { iadst4_c, iadst4_c }  // ADST_ADST = 3
  };

  int i, j;
  tran_low_t out[4 * 4];
  tran_low_t *outptr = out;
  tran_low_t temp_in[4], temp_out[4];

  // inverse transform row vectors
  for (i = 0; i < 4; ++i) {
    IHT_4[tx_type].rows(input, outptr);
    input += 4;
    outptr += 4;
  }

  // inverse transform column vectors
  for (i = 0; i < 4; ++i) {
    for (j = 0; j < 4; ++j) temp_in[j] = out[j * 4 + i];
    IHT_4[tx_type].cols(temp_in, temp_out);
    for (j = 0; j < 4; ++j) {
      dest[j * stride + i] = clip_pixel_add(dest[j * stride + i],
                                            ROUND_POWER_OF_TWO(temp_out[j], 4));
    }
  }
}

static const transform_2d IHT_8[] = {
  { idct8_c, idct8_c },   // DCT_DCT  = 0
  { iadst8_c, idct8_c },  // ADST_DCT = 1
  { idct8_c, iadst8_c },  // DCT_ADST = 2
  { iadst8_c, iadst8_c }  // ADST_ADST = 3
};

void av1_iht8x8_64_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                         int tx_type) {
  int i, j;
  tran_low_t out[8 * 8];
  tran_low_t *outptr = out;
  tran_low_t temp_in[8], temp_out[8];
  const transform_2d ht = IHT_8[tx_type];

  // inverse transform row vectors
  for (i = 0; i < 8; ++i) {
    ht.rows(input, outptr);
    input += 8;
    outptr += 8;
  }

  // inverse transform column vectors
  for (i = 0; i < 8; ++i) {
    for (j = 0; j < 8; ++j) temp_in[j] = out[j * 8 + i];
    ht.cols(temp_in, temp_out);
    for (j = 0; j < 8; ++j) {
      dest[j * stride + i] = clip_pixel_add(dest[j * stride + i],
                                            ROUND_POWER_OF_TWO(temp_out[j], 5));
    }
  }
}

static const transform_2d IHT_16[] = {
  { idct16_c, idct16_c },   // DCT_DCT  = 0
  { iadst16_c, idct16_c },  // ADST_DCT = 1
  { idct16_c, iadst16_c },  // DCT_ADST = 2
  { iadst16_c, iadst16_c }  // ADST_ADST = 3
};

void av1_iht16x16_256_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                            int tx_type) {
  int i, j;
  tran_low_t out[16 * 16];
  tran_low_t *outptr = out;
  tran_low_t temp_in[16], temp_out[16];
  const transform_2d ht = IHT_16[tx_type];

  // Rows
  for (i = 0; i < 16; ++i) {
    ht.rows(input, outptr);
    input += 16;
    outptr += 16;
  }

  // Columns
  for (i = 0; i < 16; ++i) {
    for (j = 0; j < 16; ++j) temp_in[j] = out[j * 16 + i];
    ht.cols(temp_in, temp_out);
    for (j = 0; j < 16; ++j) {
      dest[j * stride + i] = clip_pixel_add(dest[j * stride + i],
                                            ROUND_POWER_OF_TWO(temp_out[j], 6));
    }
  }
}

// idct
void av1_idct4x4_add(const tran_low_t *input, uint8_t *dest, int stride,
                     int eob) {
  if (eob > 1)
    aom_idct4x4_16_add(input, dest, stride);
  else
    aom_idct4x4_1_add(input, dest, stride);
}

void av1_iwht4x4_add(const tran_low_t *input, uint8_t *dest, int stride,
                     int eob) {
  if (eob > 1)
    aom_iwht4x4_16_add(input, dest, stride);
  else
    aom_iwht4x4_1_add(input, dest, stride);
}

void av1_idct8x8_add(const tran_low_t *input, uint8_t *dest, int stride,
                     int eob) {
  // If dc is 1, then input[0] is the reconstructed value, do not need
  // dequantization. Also, when dc is 1, dc is counted in eobs, namely eobs >=1.

  // The calculation can be simplified if there are not many non-zero dct
  // coefficients. Use eobs to decide what to do.
  // TODO(yunqingwang): "eobs = 1" case is also handled in av1_short_idct8x8_c.
  // Combine that with code here.
  if (eob == 1)
    // DC only DCT coefficient
    aom_idct8x8_1_add(input, dest, stride);
  else if (eob <= 12)
    aom_idct8x8_12_add(input, dest, stride);
  else
    aom_idct8x8_64_add(input, dest, stride);
}

void av1_idct16x16_add(const tran_low_t *input, uint8_t *dest, int stride,
                       int eob) {
  /* The calculation can be simplified if there are not many non-zero dct
   * coefficients. Use eobs to separate different cases. */
  if (eob == 1) /* DC only DCT coefficient. */
    aom_idct16x16_1_add(input, dest, stride);
  else if (eob <= 10)
    aom_idct16x16_10_add(input, dest, stride);
  else
    aom_idct16x16_256_add(input, dest, stride);
}

void av1_idct32x32_add(const tran_low_t *input, uint8_t *dest, int stride,
                       int eob) {
  if (eob == 1)
    aom_idct32x32_1_add(input, dest, stride);
  else if (eob <= 34)
    // non-zero coeff only in upper-left 8x8
    aom_idct32x32_34_add(input, dest, stride);
  else
    aom_idct32x32_1024_add(input, dest, stride);
}

void av1_inv_txfm_add_4x4(const tran_low_t *input, uint8_t *dest, int stride,
                          int eob, TX_TYPE tx_type, int lossless) {
  if (lossless) {
    assert(tx_type == DCT_DCT);
    av1_iwht4x4_add(input, dest, stride, eob);
  } else {
    switch (tx_type) {
      case DCT_DCT: av1_idct4x4_add(input, dest, stride, eob); break;
      case ADST_DCT:
      case DCT_ADST:
      case ADST_ADST: av1_iht4x4_16_add(input, dest, stride, tx_type); break;
      default: assert(0); break;
    }
  }
}

void av1_inv_txfm_add_8x8(const tran_low_t *input, uint8_t *dest, int stride,
                          int eob, TX_TYPE tx_type) {
  switch (tx_type) {
    case DCT_DCT: av1_idct8x8_add(input, dest, stride, eob); break;
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST: av1_iht8x8_64_add(input, dest, stride, tx_type); break;
    default: assert(0); break;
  }
}

void av1_inv_txfm_add_16x16(const tran_low_t *input, uint8_t *dest, int stride,
                            int eob, TX_TYPE tx_type) {
  switch (tx_type) {
    case DCT_DCT: av1_idct16x16_add(input, dest, stride, eob); break;
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST: av1_iht16x16_256_add(input, dest, stride, tx_type); break;
    default: assert(0); break;
  }
}

void av1_inv_txfm_add_32x32(const tran_low_t *input, uint8_t *dest, int stride,
                            int eob, TX_TYPE tx_type) {
  switch (tx_type) {
    case DCT_DCT: av1_idct32x32_add(input, dest, stride, eob); break;
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST: assert(0); break;
    default: assert(0); break;
  }
}

#if CONFIG_AOM_HIGHBITDEPTH
void av1_highbd_iht4x4_16_add_c(const tran_low_t *input, uint8_t *dest8,
                                int stride, int tx_type, int bd) {
  const highbd_transform_2d IHT_4[] = {
    { aom_highbd_idct4_c, aom_highbd_idct4_c },   // DCT_DCT  = 0
    { aom_highbd_iadst4_c, aom_highbd_idct4_c },  // ADST_DCT = 1
    { aom_highbd_idct4_c, aom_highbd_iadst4_c },  // DCT_ADST = 2
    { aom_highbd_iadst4_c, aom_highbd_iadst4_c }  // ADST_ADST = 3
  };
  uint16_t *dest = CONVERT_TO_SHORTPTR(dest8);

  int i, j;
  tran_low_t out[4 * 4];
  tran_low_t *outptr = out;
  tran_low_t temp_in[4], temp_out[4];

  // Inverse transform row vectors.
  for (i = 0; i < 4; ++i) {
    IHT_4[tx_type].rows(input, outptr, bd);
    input += 4;
    outptr += 4;
  }

  // Inverse transform column vectors.
  for (i = 0; i < 4; ++i) {
    for (j = 0; j < 4; ++j) temp_in[j] = out[j * 4 + i];
    IHT_4[tx_type].cols(temp_in, temp_out, bd);
    for (j = 0; j < 4; ++j) {
      dest[j * stride + i] = highbd_clip_pixel_add(
          dest[j * stride + i], ROUND_POWER_OF_TWO(temp_out[j], 4), bd);
    }
  }
}

static const highbd_transform_2d HIGH_IHT_8[] = {
  { aom_highbd_idct8_c, aom_highbd_idct8_c },   // DCT_DCT  = 0
  { aom_highbd_iadst8_c, aom_highbd_idct8_c },  // ADST_DCT = 1
  { aom_highbd_idct8_c, aom_highbd_iadst8_c },  // DCT_ADST = 2
  { aom_highbd_iadst8_c, aom_highbd_iadst8_c }  // ADST_ADST = 3
};

void av1_highbd_iht8x8_64_add_c(const tran_low_t *input, uint8_t *dest8,
                                int stride, int tx_type, int bd) {
  int i, j;
  tran_low_t out[8 * 8];
  tran_low_t *outptr = out;
  tran_low_t temp_in[8], temp_out[8];
  const highbd_transform_2d ht = HIGH_IHT_8[tx_type];
  uint16_t *dest = CONVERT_TO_SHORTPTR(dest8);

  // Inverse transform row vectors.
  for (i = 0; i < 8; ++i) {
    ht.rows(input, outptr, bd);
    input += 8;
    outptr += 8;
  }

  // Inverse transform column vectors.
  for (i = 0; i < 8; ++i) {
    for (j = 0; j < 8; ++j) temp_in[j] = out[j * 8 + i];
    ht.cols(temp_in, temp_out, bd);
    for (j = 0; j < 8; ++j) {
      dest[j * stride + i] = highbd_clip_pixel_add(
          dest[j * stride + i], ROUND_POWER_OF_TWO(temp_out[j], 5), bd);
    }
  }
}

static const highbd_transform_2d HIGH_IHT_16[] = {
  { aom_highbd_idct16_c, aom_highbd_idct16_c },   // DCT_DCT  = 0
  { aom_highbd_iadst16_c, aom_highbd_idct16_c },  // ADST_DCT = 1
  { aom_highbd_idct16_c, aom_highbd_iadst16_c },  // DCT_ADST = 2
  { aom_highbd_iadst16_c, aom_highbd_iadst16_c }  // ADST_ADST = 3
};

void av1_highbd_iht16x16_256_add_c(const tran_low_t *input, uint8_t *dest8,
                                   int stride, int tx_type, int bd) {
  int i, j;
  tran_low_t out[16 * 16];
  tran_low_t *outptr = out;
  tran_low_t temp_in[16], temp_out[16];
  const highbd_transform_2d ht = HIGH_IHT_16[tx_type];
  uint16_t *dest = CONVERT_TO_SHORTPTR(dest8);

  // Rows
  for (i = 0; i < 16; ++i) {
    ht.rows(input, outptr, bd);
    input += 16;
    outptr += 16;
  }

  // Columns
  for (i = 0; i < 16; ++i) {
    for (j = 0; j < 16; ++j) temp_in[j] = out[j * 16 + i];
    ht.cols(temp_in, temp_out, bd);
    for (j = 0; j < 16; ++j) {
      dest[j * stride + i] = highbd_clip_pixel_add(
          dest[j * stride + i], ROUND_POWER_OF_TWO(temp_out[j], 6), bd);
    }
  }
}

// idct
void av1_highbd_idct4x4_add(const tran_low_t *input, uint8_t *dest, int stride,
                            int eob, int bd) {
  if (eob > 1)
    aom_highbd_idct4x4_16_add(input, dest, stride, bd);
  else
    aom_highbd_idct4x4_1_add(input, dest, stride, bd);
}

void av1_highbd_iwht4x4_add(const tran_low_t *input, uint8_t *dest, int stride,
                            int eob, int bd) {
  if (eob > 1)
    aom_highbd_iwht4x4_16_add(input, dest, stride, bd);
  else
    aom_highbd_iwht4x4_1_add(input, dest, stride, bd);
}

void av1_highbd_idct8x8_add(const tran_low_t *input, uint8_t *dest, int stride,
                            int eob, int bd) {
  // If dc is 1, then input[0] is the reconstructed value, do not need
  // dequantization. Also, when dc is 1, dc is counted in eobs, namely eobs >=1.

  // The calculation can be simplified if there are not many non-zero dct
  // coefficients. Use eobs to decide what to do.
  // TODO(yunqingwang): "eobs = 1" case is also handled in av1_short_idct8x8_c.
  // Combine that with code here.
  // DC only DCT coefficient
  if (eob == 1) {
    aom_highbd_idct8x8_1_add(input, dest, stride, bd);
  } else if (eob <= 10) {
    aom_highbd_idct8x8_10_add(input, dest, stride, bd);
  } else {
    aom_highbd_idct8x8_64_add(input, dest, stride, bd);
  }
}

void av1_highbd_idct16x16_add(const tran_low_t *input, uint8_t *dest,
                              int stride, int eob, int bd) {
  // The calculation can be simplified if there are not many non-zero dct
  // coefficients. Use eobs to separate different cases.
  // DC only DCT coefficient.
  if (eob == 1) {
    aom_highbd_idct16x16_1_add(input, dest, stride, bd);
  } else if (eob <= 10) {
    aom_highbd_idct16x16_10_add(input, dest, stride, bd);
  } else {
    aom_highbd_idct16x16_256_add(input, dest, stride, bd);
  }
}

void av1_highbd_idct32x32_add(const tran_low_t *input, uint8_t *dest,
                              int stride, int eob, int bd) {
  // Non-zero coeff only in upper-left 8x8
  if (eob == 1) {
    aom_highbd_idct32x32_1_add(input, dest, stride, bd);
  } else if (eob <= 34) {
    aom_highbd_idct32x32_34_add(input, dest, stride, bd);
  } else {
    aom_highbd_idct32x32_1024_add(input, dest, stride, bd);
  }
}

void av1_highbd_inv_txfm_add_4x4(const tran_low_t *input, uint8_t *dest,
                                 int stride, int eob, int bd, TX_TYPE tx_type,
                                 int lossless) {
  if (lossless) {
    assert(tx_type == DCT_DCT);
    av1_highbd_iwht4x4_add(input, dest, stride, eob, bd);
  } else {
    switch (tx_type) {
      case DCT_DCT: av1_highbd_idct4x4_add(input, dest, stride, eob, bd); break;
      case ADST_DCT:
      case DCT_ADST:
      case ADST_ADST:
        av1_highbd_iht4x4_16_add(input, dest, stride, tx_type, bd);
        break;
      default: assert(0); break;
    }
  }
}

void av1_highbd_inv_txfm_add_8x8(const tran_low_t *input, uint8_t *dest,
                                 int stride, int eob, int bd, TX_TYPE tx_type) {
  switch (tx_type) {
    case DCT_DCT: av1_highbd_idct8x8_add(input, dest, stride, eob, bd); break;
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
      av1_highbd_iht8x8_64_add(input, dest, stride, tx_type, bd);
      break;
    default: assert(0); break;
  }
}

void av1_highbd_inv_txfm_add_16x16(const tran_low_t *input, uint8_t *dest,
                                   int stride, int eob, int bd,
                                   TX_TYPE tx_type) {
  switch (tx_type) {
    case DCT_DCT: av1_highbd_idct16x16_add(input, dest, stride, eob, bd); break;
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
      av1_highbd_iht16x16_256_add(input, dest, stride, tx_type, bd);
      break;
    default: assert(0); break;
  }
}

void av1_highbd_inv_txfm_add_32x32(const tran_low_t *input, uint8_t *dest,
                                   int stride, int eob, int bd,
                                   TX_TYPE tx_type) {
  switch (tx_type) {
    case DCT_DCT: av1_highbd_idct32x32_add(input, dest, stride, eob, bd); break;
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST: assert(0); break;
    default: assert(0); break;
  }
}
#endif  // CONFIG_AOM_HIGHBITDEPTH
