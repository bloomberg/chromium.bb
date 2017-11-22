#include <smmintrin.h>

#include "./aom_config.h"
#include "./av1_rtcd.h"
#include "av1/common/restoration.h"
#include "aom_dsp/x86/synonyms.h"

// Load 4 bytes from the possibly-misaligned pointer p, extend each byte to
// 32-bit precision and return them in an SSE register.
static __m128i xx_load_extend_8_32(const void *p) {
  return _mm_cvtepu8_epi32(xx_loadl_32(p));
}

// Load 4 halfwords from the possibly-misaligned pointer p, extend each
// halfword to 32-bit precision and return them in an SSE register.
static __m128i xx_load_extend_16_32(const void *p) {
  return _mm_cvtepu16_epi32(xx_loadl_64(p));
}

// Compute the scan of an SSE register holding 4 32-bit integers. If the
// register holds x0..x3 then the scan will hold x0, x0+x1, x0+x1+x2,
// x0+x1+x2+x3
static __m128i scan_32(__m128i x) {
  const __m128i x01 = _mm_add_epi32(x, _mm_slli_si128(x, 4));
  return _mm_add_epi32(x01, _mm_slli_si128(x01, 8));
}

// Compute two integral images from src. B sums elements; A sums their
// squares. The images are offset by one pixel, so will have width and height
// equal to width + 1, height + 1 and the first row and column will be zero.
//
// A+1 and B+1 should be aligned to 16 bytes. buf_stride should be a multiple
// of 4.
static void integral_images(const uint8_t *src, int src_stride, int width,
                            int height, int32_t *A, int32_t *B,
                            int buf_stride) {
  // Write out the zero top row
  memset(A, 0, sizeof(*A) * (width + 1));
  memset(B, 0, sizeof(*B) * (width + 1));

  const __m128i zero = _mm_setzero_si128();
  for (int i = 0; i < height; ++i) {
    // Zero the left column.
    A[(i + 1) * buf_stride] = B[(i + 1) * buf_stride] = 0;

    // ldiff is the difference H - D where H is the output sample immediately
    // to the left and D is the output sample above it. These are scalars,
    // replicated across the four lanes.
    __m128i ldiff1 = zero, ldiff2 = zero;
    for (int j = 0; j < width; j += 4) {
      const int ABj = 1 + j;

      const __m128i above1 = xx_load_128(B + ABj + i * buf_stride);
      const __m128i above2 = xx_load_128(A + ABj + i * buf_stride);

      const __m128i x1 = xx_load_extend_8_32(src + j + i * src_stride);
      const __m128i x2 = _mm_madd_epi16(x1, x1);

      const __m128i sc1 = scan_32(x1);
      const __m128i sc2 = scan_32(x2);

      const __m128i row1 = _mm_add_epi32(_mm_add_epi32(sc1, above1), ldiff1);
      const __m128i row2 = _mm_add_epi32(_mm_add_epi32(sc2, above2), ldiff2);

      xx_store_128(B + ABj + (i + 1) * buf_stride, row1);
      xx_store_128(A + ABj + (i + 1) * buf_stride, row2);

      // Calculate the new H - D.
      ldiff1 = _mm_shuffle_epi32(_mm_sub_epi32(row1, above1), 0xff);
      ldiff2 = _mm_shuffle_epi32(_mm_sub_epi32(row2, above2), 0xff);
    }
  }
}

#if CONFIG_HIGHBITDEPTH
// Compute two integral images from src. B sums elements; A sums their squares
//
// A and B should be aligned to 16 bytes. buf_stride should be a multiple of 4.
static void integral_images_highbd(const uint16_t *src, int src_stride,
                                   int width, int height, int32_t *A,
                                   int32_t *B, int buf_stride) {
  // Write out the zero top row
  memset(A, 0, sizeof(*A) * (width + 1));
  memset(B, 0, sizeof(*B) * (width + 1));

  const __m128i zero = _mm_setzero_si128();
  for (int i = 0; i < height; ++i) {
    // Zero the left column.
    A[(i + 1) * buf_stride] = B[(i + 1) * buf_stride] = 0;

    // ldiff is the difference H - D where H is the output sample immediately
    // to the left and D is the output sample above it. These are scalars,
    // replicated across the four lanes.
    __m128i ldiff1 = zero, ldiff2 = zero;
    for (int j = 0; j < width; j += 4) {
      const int ABj = 1 + j;

      const __m128i above1 = xx_load_128(B + ABj + i * buf_stride);
      const __m128i above2 = xx_load_128(A + ABj + i * buf_stride);

      const __m128i x1 = xx_load_extend_16_32(src + j + i * src_stride);
      const __m128i x2 = _mm_madd_epi16(x1, x1);

      const __m128i sc1 = scan_32(x1);
      const __m128i sc2 = scan_32(x2);

      const __m128i row1 = _mm_add_epi32(_mm_add_epi32(sc1, above1), ldiff1);
      const __m128i row2 = _mm_add_epi32(_mm_add_epi32(sc2, above2), ldiff2);

      xx_store_128(B + ABj + (i + 1) * buf_stride, row1);
      xx_store_128(A + ABj + (i + 1) * buf_stride, row2);

      // Calculate the new H - D.
      ldiff1 = _mm_shuffle_epi32(_mm_sub_epi32(row1, above1), 0xff);
      ldiff2 = _mm_shuffle_epi32(_mm_sub_epi32(row2, above2), 0xff);
    }
  }
}
#endif

// Compute four values of boxsum from the given integral image. ii should point
// at the middle of the box (for the first value). r is the box radius
static __m128i boxsum_from_ii(const int32_t *ii, int stride, int r) {
  const __m128i tl = xx_loadu_128(ii - (r + 1) - (r + 1) * stride);
  const __m128i tr = xx_loadu_128(ii + (r + 0) - (r + 1) * stride);
  const __m128i bl = xx_loadu_128(ii - (r + 1) + r * stride);
  const __m128i br = xx_loadu_128(ii + (r + 0) + r * stride);
  const __m128i u = _mm_sub_epi32(tr, tl);
  const __m128i v = _mm_sub_epi32(br, bl);
  return _mm_sub_epi32(v, u);
}

static __m128i round_for_shift(unsigned shift) {
  return _mm_set1_epi32((1 << shift) >> 1);
}

static __m128i compute_p(__m128i sum1, __m128i sum2, int bit_depth, int n) {
  __m128i an, bb;
  if (bit_depth > 8) {
    const __m128i rounding_a = round_for_shift(2 * (bit_depth - 8));
    const __m128i rounding_b = round_for_shift(bit_depth - 8);
    const __m128i shift_a = _mm_cvtsi32_si128(2 * (bit_depth - 8));
    const __m128i shift_b = _mm_cvtsi32_si128(bit_depth - 8);
    const __m128i a = _mm_srl_epi32(_mm_add_epi32(sum2, rounding_a), shift_a);
    const __m128i b = _mm_srl_epi32(_mm_add_epi32(sum1, rounding_b), shift_b);
    // b < 2^14, so we can use a 16-bit madd rather than a 32-bit
    // mullo to square it
    bb = _mm_madd_epi16(b, b);
    an = _mm_max_epi32(_mm_mullo_epi32(a, _mm_set1_epi32(n)), bb);
  } else {
    bb = _mm_madd_epi16(sum1, sum1);
    an = _mm_mullo_epi32(sum2, _mm_set1_epi32(n));
  }
  return _mm_sub_epi32(an, bb);
}

// Assumes that C, D are integral images for the original buffer which has been
// extended to have a padding of SGRPROJ_BORDER_VERT/SGRPROJ_BORDER_HORZ pixels
// on the sides. A, B, C, D point at logical position (0, 0).
static void calc_ab(int32_t *A, int32_t *B, const int32_t *C, const int32_t *D,
                    int width, int height, int buf_stride, int eps,
                    int bit_depth, int r) {
  const int n = (2 * r + 1) * (2 * r + 1);
  const __m128i s = _mm_set1_epi32(sgrproj_mtable[eps - 1][n - 1]);
  // one_over_n[n-1] is 2^12/n, so easily fits in an int16
  const __m128i one_over_n = _mm_set1_epi32(one_by_x[n - 1]);

  const __m128i rnd_z = round_for_shift(SGRPROJ_MTABLE_BITS);
  const __m128i rnd_res = round_for_shift(SGRPROJ_RECIP_BITS);

  for (int i = -1; i < height + 1; ++i) {
    for (int j0 = -1; j0 < width + 1; j0 += 4) {
      const int32_t *Cij = C + i * buf_stride + j0;
      const int32_t *Dij = D + i * buf_stride + j0;

      const __m128i pre_sum1 = boxsum_from_ii(Dij, buf_stride, r);
      const __m128i pre_sum2 = boxsum_from_ii(Cij, buf_stride, r);

#if CONFIG_DEBUG
      // When width + 2 isn't a multiple of four, z will contain some
      // uninitialised data in its upper words. This isn't really a problem
      // (they will be clamped to safe indices by the min() below, and will be
      // written to memory locations that we don't read again), but Valgrind
      // complains because we're using an uninitialised value as the address
      // for a load operation
      //
      // This mask is reasonably cheap to compute and quiets the warnings. Note
      // that we can't mask p instead of sum1 and sum2 (which would be cheaper)
      // because Valgrind gets the taint propagation in compute_p wrong.
      const __m128i ones32 = _mm_set_epi64x(0, 0xffffffffULL);
      const __m128i shift =
          _mm_set_epi64x(0, AOMMAX(0, 32 - 8 * (width + 1 - j0)));
      const __m128i mask = _mm_cvtepi8_epi32(_mm_srl_epi32(ones32, shift));
      const __m128i sum1 = _mm_and_si128(mask, pre_sum1);
      const __m128i sum2 = _mm_and_si128(mask, pre_sum2);
#else
      const __m128i sum1 = pre_sum1;
      const __m128i sum2 = pre_sum2;
#endif  // CONFIG_DEBUG

      const __m128i p = compute_p(sum1, sum2, bit_depth, n);

      const __m128i z = _mm_min_epi32(
          _mm_srli_epi32(_mm_add_epi32(_mm_mullo_epi32(p, s), rnd_z),
                         SGRPROJ_MTABLE_BITS),
          _mm_set1_epi32(255));

      // 'Gather' type instructions are not available pre-AVX2, so synthesize a
      // gather using scalar loads.
      const __m128i a_res = _mm_set_epi32(x_by_xplus1[_mm_extract_epi32(z, 3)],
                                          x_by_xplus1[_mm_extract_epi32(z, 2)],
                                          x_by_xplus1[_mm_extract_epi32(z, 1)],
                                          x_by_xplus1[_mm_extract_epi32(z, 0)]);

      xx_storeu_128(A + i * buf_stride + j0, a_res);

      const __m128i a_complement =
          _mm_sub_epi32(_mm_set1_epi32(SGRPROJ_SGR), a_res);

      // sum1 might have lanes greater than 2^15, so we can't use madd to do
      // multiplication involving sum1. However, a_complement and one_over_n
      // are both less than 256, so we can multiply them first.
      const __m128i a_comp_over_n = _mm_madd_epi16(a_complement, one_over_n);
      const __m128i b_int = _mm_mullo_epi32(a_comp_over_n, sum1);
      const __m128i b_res =
          _mm_srli_epi32(_mm_add_epi32(b_int, rnd_res), SGRPROJ_RECIP_BITS);

      xx_storeu_128(B + i * buf_stride + j0, b_res);
    }
  }
}

// Calculate 4 values of the "cross sum" starting at buf. This is a 3x3 filter
// where the outer four corners have weight 3 and all other pixels have weight
// 4.
static __m128i cross_sum(const int32_t *buf, int stride) {
  const __m128i a0 = xx_loadu_128(buf - 1 - stride);
  const __m128i a1 = xx_loadu_128(buf + 3 - stride);
  const __m128i b0 = xx_loadu_128(buf - 1);
  const __m128i b1 = xx_loadu_128(buf + 3);
  const __m128i c0 = xx_loadu_128(buf - 1 + stride);
  const __m128i c1 = xx_loadu_128(buf + 3 + stride);

  const __m128i fours =
      _mm_add_epi32(_mm_add_epi32(_mm_add_epi32(_mm_alignr_epi8(b1, b0, 4), b0),
                                  _mm_add_epi32(_mm_alignr_epi8(b1, b0, 8),
                                                _mm_alignr_epi8(c1, c0, 4))),
                    _mm_alignr_epi8(a1, a0, 4));
  const __m128i threes = _mm_add_epi32(
      _mm_add_epi32(a0, c0),
      _mm_add_epi32(_mm_alignr_epi8(a1, a0, 8), _mm_alignr_epi8(c1, c0, 8)));

  return _mm_sub_epi32(_mm_slli_epi32(_mm_add_epi32(fours, threes), 2), threes);
}

// The final filter for selfguided restoration. Computes a weighted average
// across A, B with "cross sums" (see cross_sum implementation above)
static void final_filter(int32_t *dst, int dst_stride, const int32_t *A,
                         const int32_t *B, int buf_stride, const void *dgd8,
                         int dgd_stride, int width, int height, int highbd) {
  const int nb = 5;
  const __m128i rounding =
      round_for_shift(SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
  const uint8_t *dgd_real =
      highbd ? (const uint8_t *)CONVERT_TO_SHORTPTR(dgd8) : dgd8;

  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; j += 4) {
      const __m128i a = cross_sum(A + i * buf_stride + j, buf_stride);
      const __m128i b = cross_sum(B + i * buf_stride + j, buf_stride);
      const __m128i raw =
          xx_loadl_64(dgd_real + ((i * dgd_stride + j) << highbd));
      const __m128i src =
          highbd ? _mm_cvtepu16_epi32(raw) : _mm_cvtepu8_epi32(raw);

      __m128i v = _mm_add_epi32(_mm_madd_epi16(a, src), b);
      __m128i w = _mm_srai_epi32(_mm_add_epi32(v, rounding),
                                 SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);

      xx_storeu_128(dst + i * dst_stride + j, w);
    }
  }
}

void av1_selfguided_restoration_sse4_1(const uint8_t *dgd8, int width,
                                       int height, int dgd_stride,
                                       int32_t *flt1, int32_t *flt2,
                                       int flt_stride,
                                       const sgr_params_type *params,
                                       int bit_depth, int highbd) {
  DECLARE_ALIGNED(16, int32_t, buf[4 * RESTORATION_PROC_UNIT_PELS]);

  const int width_ext = width + 2 * SGRPROJ_BORDER_HORZ;
  const int height_ext = height + 2 * SGRPROJ_BORDER_VERT;

  // Adjusting the stride of A and B here appears to avoid bad cache effects,
  // leading to a significant speed improvement.
  // We also align the stride to a multiple of 16 bytes for efficiency.
  int buf_stride = ((width_ext + 3) & ~3) + 16;

  // The "tl" pointers point at the top-left of the initialised data for the
  // array. Adding 3 here ensures that column 1 is 16-byte aligned.
  int32_t *Atl = buf + 0 * RESTORATION_PROC_UNIT_PELS + 3;
  int32_t *Btl = buf + 1 * RESTORATION_PROC_UNIT_PELS + 3;
  int32_t *Ctl = buf + 2 * RESTORATION_PROC_UNIT_PELS + 3;
  int32_t *Dtl = buf + 3 * RESTORATION_PROC_UNIT_PELS + 3;

  // The "0" pointers are (- SGRPROJ_BORDER_VERT, -SGRPROJ_BORDER_HORZ). Note
  // there's a zero row and column in A, B (integral images), so we move down
  // and right one for them.
  const int buf_diag_border =
      SGRPROJ_BORDER_HORZ + buf_stride * SGRPROJ_BORDER_VERT;

  int32_t *A0 = Atl + 1 + buf_stride;
  int32_t *B0 = Btl + 1 + buf_stride;
  int32_t *C0 = Ctl + 1 + buf_stride;
  int32_t *D0 = Dtl + 1 + buf_stride;

  // Finally, A, B, C, D point at position (0, 0).
  int32_t *A = A0 + buf_diag_border;
  int32_t *B = B0 + buf_diag_border;
  int32_t *C = C0 + buf_diag_border;
  int32_t *D = D0 + buf_diag_border;

  const int dgd_diag_border =
      SGRPROJ_BORDER_HORZ + dgd_stride * SGRPROJ_BORDER_VERT;
  const uint8_t *dgd0 = dgd8 - dgd_diag_border;

// Generate integral images from the input. C will contain sums of squares; D
// will contain just sums
#if CONFIG_HIGHBITDEPTH
  if (highbd)
    integral_images_highbd(CONVERT_TO_SHORTPTR(dgd0), dgd_stride, width_ext,
                           height_ext, Ctl, Dtl, buf_stride);
  else
#endif  // CONFIG_HIGHBITDEPTH
    integral_images(dgd0, dgd_stride, width_ext, height_ext, Ctl, Dtl,
                    buf_stride);

  // Write to flt1 and flt2
  for (int i = 0; i < 2; ++i) {
    int r = i ? params->r2 : params->r1;
    int e = i ? params->e2 : params->e1;
    int32_t *flt = i ? flt2 : flt1;

    assert(r + 1 <= AOMMIN(SGRPROJ_BORDER_VERT, SGRPROJ_BORDER_HORZ));
    calc_ab(A, B, C, D, width, height, buf_stride, e, bit_depth, r);
    final_filter(flt, flt_stride, A, B, buf_stride, dgd8, dgd_stride, width,
                 height, highbd);
  }
}

void apply_selfguided_restoration_sse4_1(const uint8_t *dat8, int width,
                                         int height, int stride, int eps,
                                         const int *xqd, uint8_t *dst8,
                                         int dst_stride, int32_t *tmpbuf,
                                         int bit_depth, int highbd) {
  int32_t *flt1 = tmpbuf;
  int32_t *flt2 = flt1 + RESTORATION_TILEPELS_MAX;
  assert(width * height <= RESTORATION_TILEPELS_MAX);
  av1_selfguided_restoration_sse4_1(dat8, width, height, stride, flt1, flt2,
                                    width, &sgr_params[eps], bit_depth, highbd);

  int xq[2];
  decode_xq(xqd, xq);

  __m128i xq0 = _mm_set1_epi32(xq[0]);
  __m128i xq1 = _mm_set1_epi32(xq[1]);

  for (int i = 0; i < height; ++i) {
    // Calculate output in batches of 8 pixels
    for (int j = 0; j < width; j += 8) {
      const int k = i * width + j;
      const int m = i * dst_stride + j;

      const uint8_t *dat8ij = dat8 + i * stride + j;
      __m128i src;
      if (highbd) {
        src = xx_load_128(CONVERT_TO_SHORTPTR(dat8ij));
      } else {
        src = _mm_cvtepu8_epi16(xx_loadl_64(dat8ij));
      }

      const __m128i u = _mm_slli_epi16(src, SGRPROJ_RST_BITS);
      const __m128i u_0 = _mm_cvtepu16_epi32(u);
      const __m128i u_1 = _mm_cvtepu16_epi32(_mm_srli_si128(u, 8));

      const __m128i f1_0 = _mm_sub_epi32(xx_loadu_128(&flt1[k]), u_0);
      const __m128i f2_0 = _mm_sub_epi32(xx_loadu_128(&flt2[k]), u_0);
      const __m128i f1_1 = _mm_sub_epi32(xx_loadu_128(&flt1[k + 4]), u_1);
      const __m128i f2_1 = _mm_sub_epi32(xx_loadu_128(&flt2[k + 4]), u_1);

      const __m128i v_0 = _mm_add_epi32(
          _mm_add_epi32(_mm_mullo_epi32(xq0, f1_0), _mm_mullo_epi32(xq1, f2_0)),
          _mm_slli_epi32(u_0, SGRPROJ_PRJ_BITS));
      const __m128i v_1 = _mm_add_epi32(
          _mm_add_epi32(_mm_mullo_epi32(xq0, f1_1), _mm_mullo_epi32(xq1, f2_1)),
          _mm_slli_epi32(u_1, SGRPROJ_PRJ_BITS));

      const __m128i rounding =
          round_for_shift(SGRPROJ_PRJ_BITS + SGRPROJ_RST_BITS);
      const __m128i w_0 = _mm_srai_epi32(_mm_add_epi32(v_0, rounding),
                                         SGRPROJ_PRJ_BITS + SGRPROJ_RST_BITS);
      const __m128i w_1 = _mm_srai_epi32(_mm_add_epi32(v_1, rounding),
                                         SGRPROJ_PRJ_BITS + SGRPROJ_RST_BITS);

      if (highbd) {
        // Pack into 16 bits and clamp to [0, 2^bit_depth)
        const __m128i tmp = _mm_packus_epi32(w_0, w_1);
        const __m128i max = _mm_set1_epi16((1 << bit_depth) - 1);
        const __m128i res = _mm_min_epi16(tmp, max);
        xx_store_128(CONVERT_TO_SHORTPTR(dst8 + m), res);
      } else {
        // Pack into 8 bits and clamp to [0, 256)
        const __m128i tmp = _mm_packs_epi32(w_0, w_1);
        const __m128i res = _mm_packus_epi16(tmp, tmp /* "don't care" value */);
        xx_storel_64(dst8 + m, res);
      }
    }
  }
}
