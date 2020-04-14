#include <immintrin.h>

#include "config/aom_dsp_rtcd.h"

static INLINE void copy_128(const uint8_t *src, uint8_t *dst) {
  __m256i s[4];
  s[0] = _mm256_loadu_si256((__m256i *)(src + 0 * 32));
  s[1] = _mm256_loadu_si256((__m256i *)(src + 1 * 32));
  s[2] = _mm256_loadu_si256((__m256i *)(src + 2 * 32));
  s[3] = _mm256_loadu_si256((__m256i *)(src + 3 * 32));
  _mm256_storeu_si256((__m256i *)(dst + 0 * 32), s[0]);
  _mm256_storeu_si256((__m256i *)(dst + 1 * 32), s[1]);
  _mm256_storeu_si256((__m256i *)(dst + 2 * 32), s[2]);
  _mm256_storeu_si256((__m256i *)(dst + 3 * 32), s[3]);
}

void aom_convolve_copy_avx2(const uint8_t *src, ptrdiff_t src_stride,
                            uint8_t *dst, ptrdiff_t dst_stride, int w, int h) {
  if (w >= 16) {
    assert(!((intptr_t)dst % 16));
    assert(!(dst_stride % 16));
  }

  if (w == 2) {
    do {
      memmove(dst, src, 2 * sizeof(*src));
      src += src_stride;
      dst += dst_stride;
      memmove(dst, src, 2 * sizeof(*src));
      src += src_stride;
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 4) {
    do {
      memmove(dst, src, 4 * sizeof(*src));
      src += src_stride;
      dst += dst_stride;
      memmove(dst, src, 4 * sizeof(*src));
      src += src_stride;
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 8) {
    do {
      __m128i s[2];
      s[0] = _mm_loadl_epi64((__m128i *)src);
      src += src_stride;
      s[1] = _mm_loadl_epi64((__m128i *)src);
      src += src_stride;
      _mm_storel_epi64((__m128i *)dst, s[0]);
      dst += dst_stride;
      _mm_storel_epi64((__m128i *)dst, s[1]);
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 16) {
    do {
      __m128i s[2];
      s[0] = _mm_loadu_si128((__m128i *)src);
      src += src_stride;
      s[1] = _mm_loadu_si128((__m128i *)src);
      src += src_stride;
      _mm_store_si128((__m128i *)dst, s[0]);
      dst += dst_stride;
      _mm_store_si128((__m128i *)dst, s[1]);
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 32) {
    do {
      __m256i s[2];
      s[0] = _mm256_loadu_si256((__m256i *)src);
      src += src_stride;
      s[1] = _mm256_loadu_si256((__m256i *)src);
      src += src_stride;
      _mm256_storeu_si256((__m256i *)dst, s[0]);
      dst += dst_stride;
      _mm256_storeu_si256((__m256i *)dst, s[1]);
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 64) {
    do {
      __m256i s[4];
      s[0] = _mm256_loadu_si256((__m256i *)(src + 0 * 32));
      s[1] = _mm256_loadu_si256((__m256i *)(src + 1 * 32));
      src += src_stride;
      s[2] = _mm256_loadu_si256((__m256i *)(src + 0 * 32));
      s[3] = _mm256_loadu_si256((__m256i *)(src + 1 * 32));
      src += src_stride;
      _mm256_storeu_si256((__m256i *)(dst + 0 * 32), s[0]);
      _mm256_storeu_si256((__m256i *)(dst + 1 * 32), s[1]);
      dst += dst_stride;
      _mm256_storeu_si256((__m256i *)(dst + 0 * 32), s[2]);
      _mm256_storeu_si256((__m256i *)(dst + 1 * 32), s[3]);
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else {
    do {
      copy_128(src, dst);
      src += src_stride;
      dst += dst_stride;
      copy_128(src, dst);
      src += src_stride;
      dst += dst_stride;
      h -= 2;
    } while (h);
  }
}
