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

#ifndef VP10_DECODER_DETOKENIZE_H_
#define VP10_DECODER_DETOKENIZE_H_

#include "aom_dsp/bitreader.h"
#include "av1/decoder/decoder.h"
#include "av1/common/scan.h"

#ifdef __cplusplus
extern "C" {
#endif

int vp10_decode_block_tokens(MACROBLOCKD *xd, int plane, const scan_order *sc,
                             int x, int y, TX_SIZE tx_size, vpx_reader *r,
                             int seg_id);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_DECODER_DETOKENIZE_H_
