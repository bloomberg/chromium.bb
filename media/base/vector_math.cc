// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/vector_math.h"
#include "media/base/vector_math_testing.h"

#include "base/cpu.h"
#include "base/logging.h"

namespace media {
namespace vector_math {

void FMAC(const float src[], float scale, int len, float dest[]) {
  // Ensure |src| and |dest| are 16-byte aligned.
  DCHECK_EQ(0u, reinterpret_cast<uintptr_t>(src) & (kRequiredAlignment - 1));
  DCHECK_EQ(0u, reinterpret_cast<uintptr_t>(dest) & (kRequiredAlignment - 1));

  // Rely on function level static initialization to keep VectorFMACProc
  // selection thread safe.
  typedef void (*VectorFMACProc)(const float src[], float scale, int len,
                                 float dest[]);
#if defined(ARCH_CPU_X86_FAMILY)
#if defined(__SSE__)
  static const VectorFMACProc kVectorFMACProc = FMAC_SSE;
#else
  static const VectorFMACProc kVectorFMACProc =
      base::CPU().has_sse() ? FMAC_SSE : FMAC_C;
#endif
#else
  static const VectorFMACProc kVectorFMACProc = FMAC_C;
#endif

  return kVectorFMACProc(src, scale, len, dest);
}

void FMAC_C(const float src[], float scale, int len, float dest[]) {
  for (int i = 0; i < len; ++i)
    dest[i] += src[i] * scale;
}

}  // namespace vector_math
}  // namespace media
