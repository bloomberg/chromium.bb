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

#ifndef AOM_DSP_ANSREADER_H_
#define AOM_DSP_ANSREADER_H_
// A uABS and rANS decoder implementation of Asymmetric Numeral Systems
// http://arxiv.org/abs/1311.2540v2

#include <assert.h>
#include "./aom_config.h"
#include "aom/aom_integer.h"
#include "aom_dsp/prob.h"
#include "aom_dsp/ans.h"
#include "aom_ports/mem_ops.h"
#if CONFIG_ACCOUNTING
#include "av1/common/accounting.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct AnsDecoder {
  const uint8_t *buf;
  int buf_offset;
  uint32_t state;
#if ANS_MAX_SYMBOLS
  int symbols_left;
#endif
#if CONFIG_ACCOUNTING
  Accounting *accounting;
#endif
};

static INLINE int ans_read_reinit(struct AnsDecoder *const ans);

static INLINE unsigned refill_state(struct AnsDecoder *const ans,
                                    unsigned state) {
#if ANS_REVERSE
  while (state < L_BASE && ans->buf_offset < 0) {
    state = state * IO_BASE + ans->buf[ans->buf_offset++];
  }
#else
  while (state < L_BASE && ans->buf_offset > 0) {
    state = state * IO_BASE + ans->buf[--ans->buf_offset];
  }
#endif
  return state;
}

static INLINE int uabs_read(struct AnsDecoder *ans, AnsP8 p0) {
  AnsP8 p = ANS_P8_PRECISION - p0;
  int s;
  unsigned xp, sp;
  unsigned state;
#if ANS_MAX_SYMBOLS
  if (ans->symbols_left-- == 0) {
    ans_read_reinit(ans);
    ans->symbols_left--;
  }
#endif
  state = ans->state;
  sp = state * p;
  xp = sp / ANS_P8_PRECISION;
  s = (sp & 0xFF) >= p0;
  if (s)
    state = xp;
  else
    state -= xp;
  ans->state = refill_state(ans, state);
  return s;
}

static INLINE int uabs_read_bit(struct AnsDecoder *ans) {
  int s;
  unsigned state;
#if ANS_MAX_SYMBOLS
  if (ans->symbols_left-- == 0) {
    ans_read_reinit(ans);
    ans->symbols_left--;
  }
#endif
  state = ans->state;
  s = (int)(state & 1);
  state >>= 1;
  ans->state = refill_state(ans, state);
  return s;
}

struct rans_dec_sym {
  uint8_t val;
  aom_cdf_prob prob;
  aom_cdf_prob cum_prob;  // not-inclusive
};

static INLINE void fetch_sym(struct rans_dec_sym *out, const aom_cdf_prob *cdf,
                             aom_cdf_prob rem) {
  int i;
  aom_cdf_prob cum_prob = 0, top_prob;
  // TODO(skal): if critical, could be a binary search.
  // Or, better, an O(1) alias-table.
  for (i = 0; rem >= (top_prob = cdf[i]); ++i) {
    cum_prob = top_prob;
  }
  out->val = i;
  out->prob = top_prob - cum_prob;
  out->cum_prob = cum_prob;
}

static INLINE int rans_read(struct AnsDecoder *ans, const aom_cdf_prob *tab) {
  unsigned rem;
  unsigned quo;
  struct rans_dec_sym sym;
#if ANS_MAX_SYMBOLS
  if (ans->symbols_left-- == 0) {
    ans_read_reinit(ans);
    ans->symbols_left--;
  }
#endif
  quo = ans->state / RANS_PRECISION;
  rem = ans->state % RANS_PRECISION;
  fetch_sym(&sym, tab, rem);
  ans->state = quo * sym.prob + rem - sym.cum_prob;
  ans->state = refill_state(ans, ans->state);
  return sym.val;
}

static INLINE int ans_read_init(struct AnsDecoder *const ans,
                                const uint8_t *const buf, int offset) {
  unsigned x;
  if (offset < 1) return 1;
#if ANS_REVERSE
  ans->buf = buf + offset;
  ans->buf_offset = -offset;
  x = buf[0];
  if ((x & 0x80) == 0) {
    if (offset < 2) return 1;
    ans->buf_offset += 2;
    ans->state = mem_get_be16(buf) & 0x7FFF;
  } else {
    if (offset < 3) return 1;
    ans->buf_offset += 3;
    ans->state = mem_get_be24(buf) & 0x7FFFFF;
  }
#else
  ans->buf = buf;
  x = buf[offset - 1];
  if ((x & 0x80) == 0) {
    if (offset < 2) return 1;
    ans->buf_offset = offset - 2;
    ans->state = mem_get_le16(buf + offset - 2) & 0x7FFF;
  } else if ((x & 0xC0) == 0x80) {
    if (offset < 3) return 1;
    ans->buf_offset = offset - 3;
    ans->state = mem_get_le24(buf + offset - 3) & 0x3FFFFF;
  } else if ((x & 0xE0) == 0xE0) {
    if (offset < 4) return 1;
    ans->buf_offset = offset - 4;
    ans->state = mem_get_le32(buf + offset - 4) & 0x1FFFFFFF;
  } else {
    // 110xxxxx implies this byte is a superframe marker
    return 1;
  }
#endif  // ANS_REVERSE
#if CONFIG_ACCOUNTING
  ans->accounting = NULL;
#endif
  ans->state += L_BASE;
  if (ans->state >= L_BASE * IO_BASE) return 1;
#if ANS_MAX_SYMBOLS
  ans->symbols_left = ANS_MAX_SYMBOLS;
#endif
  return 0;
}

#if ANS_REVERSE
static INLINE int ans_read_reinit(struct AnsDecoder *const ans) {
  return ans_read_init(ans, ans->buf + ans->buf_offset, -ans->buf_offset);
}
#endif

static INLINE int ans_read_end(struct AnsDecoder *const ans) {
  return ans->state == L_BASE;
}

static INLINE int ans_reader_has_error(const struct AnsDecoder *const ans) {
  return ans->state < L_BASE && ans->buf_offset == 0;
}
#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // AOM_DSP_ANSREADER_H_
