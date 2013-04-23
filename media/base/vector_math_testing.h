// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VECTOR_MATH_TESTING_H_
#define MEDIA_BASE_VECTOR_MATH_TESTING_H_

#include "build/build_config.h"
#include "media/base/media_export.h"

namespace media {
namespace vector_math {

// Optimized versions exposed for testing.  See vector_math.h for details.
MEDIA_EXPORT void FMAC_C(const float src[], float scale, int len, float dest[]);
MEDIA_EXPORT void FMUL_C(const float src[], float scale, int len, float dest[]);

#if defined(ARCH_CPU_X86_FAMILY)
MEDIA_EXPORT void FMAC_SSE(const float src[], float scale, int len,
                           float dest[]);
MEDIA_EXPORT void FMUL_SSE(const float src[], float scale, int len,
                           float dest[]);
#endif

#if defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
MEDIA_EXPORT void FMAC_NEON(const float src[], float scale, int len,
                            float dest[]);
MEDIA_EXPORT void FMUL_NEON(const float src[], float scale, int len,
                            float dest[]);
#endif

}  // namespace vector_math
}  // namespace media

#endif  // MEDIA_BASE_VECTOR_MATH_TESTING_H_
