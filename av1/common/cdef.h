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

// ceil(log2(DERING_STRENGTHS^DERING_REFINEMENT_LEVELS *
//           CLPF_STRENGTHS^CLPF_REFINEMENT_LEVELS))
#define DERING_LEVEL_BITS (22)
#define MAX_DERING_LEVEL (1LL << DERING_LEVEL_BITS)

#define DERING_REFINEMENT_BITS 2
#define DERING_REFINEMENT_LEVELS 4
#define CLPF_REFINEMENT_BITS 1
#define CLPF_REFINEMENT_LEVELS 2

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

uint32_t levels_to_id(const int lev[DERING_REFINEMENT_LEVELS],
                      const int str[CLPF_REFINEMENT_LEVELS]);
void id_to_levels(int lev[DERING_REFINEMENT_LEVELS],
                  int str[CLPF_REFINEMENT_LEVELS], uint32_t id);
void cdef_get_bits(const int *lev, const int *str, int *dering_bits,
                   int *clpf_bits);

int sb_all_skip(const AV1_COMMON *const cm, int mi_row, int mi_col);
int sb_compute_dering_list(const AV1_COMMON *const cm, int mi_row, int mi_col,
                           dering_list *dlist);
void av1_cdef_frame(YV12_BUFFER_CONFIG *frame, AV1_COMMON *cm, MACROBLOCKD *xd,
                    uint32_t global_level, int clpf_strength_u,
                    int clpf_strength_v);

void av1_cdef_search(YV12_BUFFER_CONFIG *frame, const YV12_BUFFER_CONFIG *ref,
                     AV1_COMMON *cm, MACROBLOCKD *xd);

#ifdef __cplusplus
}  // extern "C"
#endif
#endif  // AV1_COMMON_DERING_H_
