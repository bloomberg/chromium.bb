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

#ifndef VP10_COMMON_VP10_FWD_TXFM_H_
#define VP10_COMMON_VP10_FWD_TXFM_H_

#include "aom_dsp/txfm_common.h"
#include "aom_dsp/fwd_txfm.h"

void vp10_fdct32(const tran_high_t *input, tran_high_t *output, int round);
#endif  // VP10_COMMON_VP10_FWD_TXFM_H_
