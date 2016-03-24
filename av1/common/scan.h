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

#ifndef VP10_COMMON_SCAN_H_
#define VP10_COMMON_SCAN_H_

#include "aom/vpx_integer.h"
#include "aom_ports/mem.h"

#include "av1/common/enums.h"
#include "av1/common/blockd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_NEIGHBORS 2

typedef struct {
  const int16_t *scan;
  const int16_t *iscan;
  const int16_t *neighbors;
} scan_order;

extern const scan_order vp10_default_scan_orders[TX_SIZES];
extern const scan_order vp10_scan_orders[TX_SIZES][TX_TYPES];

static INLINE int get_coef_context(const int16_t *neighbors,
                                   const uint8_t *token_cache, int c) {
  return (1 + token_cache[neighbors[MAX_NEIGHBORS * c + 0]] +
          token_cache[neighbors[MAX_NEIGHBORS * c + 1]]) >>
         1;
}

static INLINE const scan_order *get_scan(TX_SIZE tx_size, TX_TYPE tx_type) {
  return &vp10_scan_orders[tx_size][tx_type];
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_COMMON_SCAN_H_
