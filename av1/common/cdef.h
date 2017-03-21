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
#ifndef AV1_COMMON_DERING_H_
#define AV1_COMMON_DERING_H_

#define CDEF_MAX_STRENGTHS 16
#define CDEF_STRENGTH_BITS 7

#define DERING_STRENGTHS 21
#define CLPF_STRENGTHS 4

#include "./aom_config.h"
#include "aom/aom_integer.h"
#include "aom_ports/mem.h"
#include "av1/common/od_dering.h"
#include "av1/common/onyxc_int.h"
#include "./od_dering.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int dering_level_table[DERING_STRENGTHS];

int sb_all_skip(const AV1_COMMON *const cm, int mi_row, int mi_col);
int sb_compute_dering_list(const AV1_COMMON *const cm, int mi_row, int mi_col,
                           dering_list *dlist);
void av1_cdef_frame(YV12_BUFFER_CONFIG *frame, AV1_COMMON *cm, MACROBLOCKD *xd,
                    int clpf_strength_u, int clpf_strength_v);

void av1_cdef_search(YV12_BUFFER_CONFIG *frame, const YV12_BUFFER_CONFIG *ref,
                     AV1_COMMON *cm, MACROBLOCKD *xd);

#ifdef __cplusplus
}  // extern "C"
#endif
#endif  // AV1_COMMON_DERING_H_
