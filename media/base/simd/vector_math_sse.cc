// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/vector_math_testing.h"

#include <xmmintrin.h>  // NOLINT

namespace media {
namespace vector_math {

void FMAC_SSE(const float src[], float scale, int len, float dest[]) {
  const int rem = len % 4;
  const int last_index = len - rem;
  __m128 m_scale = _mm_set_ps1(scale);
  for (int i = 0; i < last_index; i += 4) {
    _mm_store_ps(dest + i, _mm_add_ps(_mm_load_ps(dest + i),
                 _mm_mul_ps(_mm_load_ps(src + i), m_scale)));
  }

  // Handle any remaining values that wouldn't fit in an SSE pass.
  for (int i = last_index; i < len; ++i)
    dest[i] += src[i] * scale;
}

}  // namespace vector_math
}  // namespace media
