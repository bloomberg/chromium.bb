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

#ifndef AOM_DSP_INV_TXFM_H_
#define AOM_DSP_INV_TXFM_H_

#include <assert.h>

#include "./aom_config.h"
#include "aom_dsp/txfm_common.h"
#include "aom_ports/mem.h"

#ifdef __cplusplus
extern "C" {
#endif

static INLINE tran_high_t dct_const_round_shift(tran_high_t input) {
  return ROUND_POWER_OF_TWO(input, DCT_CONST_BITS);
}

static INLINE tran_high_t check_range(tran_high_t input, int bd) {
#if CONFIG_DAALA_TX
  // Daala TX coeffs cover a different range from AV1 TX
  // Temporary: this will be changing shortly upon moving to a fixed
  // coeff depth!
  // - 8 bit: signed 18 bit integer
  // - 10 bit: signed 20 bit integer
  // - 12 bit: signed 22 bit integer
  const int32_t int_max = (1 << (9 + bd)) - 1;
  const int32_t int_min = -int_max - 1;
#else
  // AV1 TX case
  // - 8 bit: signed 16 bit integer
  // - 10 bit: signed 18 bit integer
  // - 12 bit: signed 20 bit integer
  const int32_t int_max = (1 << (7 + bd)) - 1;
  const int32_t int_min = -int_max - 1;
#endif
#if CONFIG_COEFFICIENT_RANGE_CHECKING
  assert(int_min <= input);
  assert(input <= int_max);
#endif  // CONFIG_COEFFICIENT_RANGE_CHECKING
  return clamp64(input, int_min, int_max);
}

#define WRAPLOW(x) ((int32_t)check_range(x, 8))
#define HIGHBD_WRAPLOW(x, bd) ((int32_t)check_range((x), bd))

#if CONFIG_MRC_TX
// These each perform dct but add coefficients based on a mask
void aom_imrc32x32_1_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                           uint8_t *mask);

void aom_imrc32x32_1024_add_c(const tran_low_t *input, uint8_t *dest,
                              int stride, uint8_t *mask);

void aom_imrc32x32_135_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                             uint8_t *mask);

void aom_imrc32x32_34_add_c(const tran_low_t *input, uint8_t *dest, int stride,
                            uint8_t *mask);
#endif  // CONFIG_MRC_TX

void aom_idct4_c(const tran_low_t *input, tran_low_t *output);
void aom_idct8_c(const tran_low_t *input, tran_low_t *output);
void aom_idct16_c(const tran_low_t *input, tran_low_t *output);
void aom_idct32_c(const tran_low_t *input, tran_low_t *output);
#if CONFIG_TX64X64 && CONFIG_DAALA_TX64
void aom_idct64_c(const tran_low_t *input, tran_low_t *output);
#endif
void aom_iadst4_c(const tran_low_t *input, tran_low_t *output);
void aom_iadst8_c(const tran_low_t *input, tran_low_t *output);
void aom_iadst16_c(const tran_low_t *input, tran_low_t *output);

void aom_highbd_idct4_c(const tran_low_t *input, tran_low_t *output, int bd);
void aom_highbd_idct8_c(const tran_low_t *input, tran_low_t *output, int bd);
void aom_highbd_idct16_c(const tran_low_t *input, tran_low_t *output, int bd);
void aom_highbd_idct32_c(const tran_low_t *input, tran_low_t *output, int bd);

void aom_highbd_iadst4_c(const tran_low_t *input, tran_low_t *output, int bd);
void aom_highbd_iadst8_c(const tran_low_t *input, tran_low_t *output, int bd);
void aom_highbd_iadst16_c(const tran_low_t *input, tran_low_t *output, int bd);

static INLINE uint16_t highbd_clip_pixel_add(uint16_t dest, tran_high_t trans,
                                             int bd) {
  trans = HIGHBD_WRAPLOW(trans, bd);
  return clip_pixel_highbd(dest + (int)trans, bd);
}

static INLINE uint8_t clip_pixel_add(uint8_t dest, tran_high_t trans) {
  trans = WRAPLOW(trans);
  return clip_pixel(dest + (int)trans);
}
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_DSP_INV_TXFM_H_
