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

#ifndef AV1_COMMON_MV_H_
#define AV1_COMMON_MV_H_

#include "aom/aom_integer.h"

#include "av1/common/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mv {
  int16_t row;
  int16_t col;
} MV;

typedef union int_mv {
  uint32_t as_int;
  MV as_mv;
} int_mv; /* facilitates faster equality tests and copies */

typedef struct mv32 {
  int32_t row;
  int32_t col;
} MV32;

#if CONFIG_REF_MV
typedef struct candidate_mv {
  int_mv this_mv;
  int weight;
} CANDIDATE_MV;
#endif

static INLINE int is_zero_mv(const MV *mv) {
  return *((const uint32_t *)mv) == 0;
}

static INLINE int is_equal_mv(const MV *a, const MV *b) {
  return *((const uint32_t *)a) == *((const uint32_t *)b);
}

static INLINE void clamp_mv(MV *mv, int min_col, int max_col, int min_row,
                            int max_row) {
  mv->col = clamp(mv->col, min_col, max_col);
  mv->row = clamp(mv->row, min_row, max_row);
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_MV_H_
