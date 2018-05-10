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
#include "av1/common/odintrin.h"
#include "aom_ports/mem.h"

#ifdef __cplusplus
extern "C" {
#endif

static INLINE tran_high_t dct_const_round_shift(tran_high_t input) {
  return ROUND_POWER_OF_TWO(input, DCT_CONST_BITS);
}

static INLINE uint16_t highbd_clip_pixel_add(uint16_t dest, tran_high_t trans,
                                             int bd) {
  return clip_pixel_highbd(dest + (int)trans, bd);
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_DSP_INV_TXFM_H_
