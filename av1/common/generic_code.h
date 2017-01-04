/*
 * Copyright (c) 2001-2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

/* clang-format off */

#if !defined(_generic_code_H)
# define _generic_code_H

# include "aom_dsp/bitreader.h"
# include "aom_dsp/bitwriter.h"

# define GENERIC_TABLES 12

#define generic_decode(r, model, max, ex_q16, integration, ACCT_STR_NAME) \
  generic_decode_(r, model, max, ex_q16, integration ACCT_STR_ARG(ACCT_STR_NAME))
#define aom_decode_cdf_adapt_q15(r, cdf, n, count, rate, ACCT_STR_NAME) \
  aom_decode_cdf_adapt_q15_(r, cdf, n, count, rate ACCT_STR_ARG(ACCT_STR_NAME))
#define aom_decode_cdf_adapt(r, cdf, n, increment, ACCT_STR_NAME) \
  aom_decode_cdf_adapt_(r, cdf, n, increment ACCT_STR_ARG(ACCT_STR_NAME))

typedef struct {
  /** cdf for multiple expectations of x */
  uint16_t cdf[GENERIC_TABLES][16];
  /** Frequency increment for learning the cdfs */
  int increment;
} generic_encoder;

#define OD_IIR_DIADIC(y, x, shift) ((y) += ((x) - (y)) >> (shift))

void generic_model_init(generic_encoder *model);

#define OD_CDFS_INIT(cdf, val) aom_cdf_init(&cdf[0][0], \
 sizeof(cdf)/sizeof(cdf[0]), sizeof(cdf[0])/sizeof(cdf[0][0]), val, val)

#define OD_CDFS_INIT_FIRST(cdf, val, first) aom_cdf_init(&cdf[0][0], \
 sizeof(cdf)/sizeof(cdf[0]), sizeof(cdf[0])/sizeof(cdf[0][0]), val, first)

#define OD_SINGLE_CDF_INIT(cdf, val) aom_cdf_init(cdf, \
 1, sizeof(cdf)/sizeof(cdf[0]), val, val)

#define OD_SINGLE_CDF_INIT_FIRST(cdf, val, first) aom_cdf_init(cdf, \
 1, sizeof(cdf)/sizeof(cdf[0]), val, first)

void aom_cdf_init(uint16_t *cdf, int ncdfs, int nsyms, int val, int first);

void aom_cdf_adapt_q15(int val, uint16_t *cdf, int n, int *count, int rate);

void aom_encode_cdf_adapt_q15(aom_writer *w, int val, uint16_t *cdf, int n,
 int *count, int rate);

void aom_encode_cdf_adapt(aom_writer *w, int val, uint16_t *cdf, int n,
 int increment);

int aom_decode_cdf_adapt_(aom_reader *r, uint16_t *cdf, int n,
 int increment ACCT_STR_PARAM);

void generic_encode(aom_writer *w, generic_encoder *model, int x, int max,
 int *ex_q16, int integration);
double generic_encode_cost(generic_encoder *model, int x, int max,
 int *ex_q16);

double od_encode_cdf_cost(int val, uint16_t *cdf, int n);

int aom_decode_cdf_adapt_q15_(aom_reader *r, uint16_t *cdf, int n,
 int *count, int rate ACCT_STR_PARAM);

int generic_decode_(aom_reader *r, generic_encoder *model, int max,
 int *ex_q16, int integration ACCT_STR_PARAM);

int log_ex(int ex_q16);

void generic_model_update(generic_encoder *model, int *ex_q16, int x, int xs,
 int id, int integration);

#endif
