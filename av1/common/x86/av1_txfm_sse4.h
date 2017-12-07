#ifndef AV1_TXFM_SSE4_H_
#define AV1_TXFM_SSE4_H_

#include <smmintrin.h>

#ifdef __cplusplus
extern "C" {
#endif

static INLINE __m128i av1_round_shift_32_sse4_1(__m128i vec, int bit) {
  __m128i tmp, round;
  round = _mm_set1_epi32(1 << (bit - 1));
  tmp = _mm_add_epi32(vec, round);
  return _mm_srai_epi32(tmp, bit);
}

static INLINE void av1_round_shift_array_32_sse4_1(__m128i *input,
                                                   __m128i *output,
                                                   const int size,
                                                   const int bit) {
  if (bit > 0) {
    int i;
    for (i = 0; i < size; i++) {
      output[i] = av1_round_shift_32_sse4_1(input[i], bit);
    }
  } else {
    int i;
    for (i = 0; i < size; i++) {
      output[i] = _mm_slli_epi32(input[i], -bit);
    }
  }
}

#ifdef __cplusplus
}
#endif

#endif  // AV1_TXFM_SSE4_H_
