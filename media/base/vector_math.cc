// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/vector_math.h"
#include "media/base/vector_math_testing.h"

#include "base/cpu.h"
#include "base/logging.h"
#include "build/build_config.h"

#if defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
#include <arm_neon.h>
#endif

namespace media {
namespace vector_math {

void FMAC(const float src[], float scale, int len, float dest[]) {
  // Ensure |src| and |dest| are 16-byte aligned.
  DCHECK_EQ(0u, reinterpret_cast<uintptr_t>(src) & (kRequiredAlignment - 1));
  DCHECK_EQ(0u, reinterpret_cast<uintptr_t>(dest) & (kRequiredAlignment - 1));

  typedef void (*VectorFMACProc)(const float src[], float scale, int len,
                                 float dest[]);

  // No NaCl code uses the SSE functionality of AudioBus and plumbing the -msse
  // built library is non-trivial, so simply disable for now.  iOS lies about
  // its architecture, so we need to exclude it here.
#if defined(ARCH_CPU_X86_FAMILY) && !defined(OS_NACL) && !defined(OS_IOS)
#if defined(__SSE__)
  static const VectorFMACProc kVectorFMACProc = FMAC_SSE;
#else
  // TODO(dalecurtis): Remove function level static initialization, it's not
  // thread safe: http://crbug.com/224662.
  static const VectorFMACProc kVectorFMACProc =
      base::CPU().has_sse() ? FMAC_SSE : FMAC_C;
#endif
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
  static const VectorFMACProc kVectorFMACProc = FMAC_NEON;
#else
  static const VectorFMACProc kVectorFMACProc = FMAC_C;
#endif

  return kVectorFMACProc(src, scale, len, dest);
}

void FMAC_C(const float src[], float scale, int len, float dest[]) {
  for (int i = 0; i < len; ++i)
    dest[i] += src[i] * scale;
}

void FMUL(const float src[], float scale, int len, float dest[]) {
  // Ensure |src| and |dest| are 16-byte aligned.
  DCHECK_EQ(0u, reinterpret_cast<uintptr_t>(src) & (kRequiredAlignment - 1));
  DCHECK_EQ(0u, reinterpret_cast<uintptr_t>(dest) & (kRequiredAlignment - 1));

  typedef void (*VectorFMULProc)(const float src[], float scale, int len,
                                 float dest[]);

  // No NaCl code uses the SSE functionality of AudioBus and plumbing the -msse
  // built library is non-trivial, so simply disable for now.  iOS lies about
  // its architecture, so we need to exclude it here.
#if defined(ARCH_CPU_X86_FAMILY) && !defined(OS_NACL) && !defined(OS_IOS)
#if defined(__SSE__)
  static const VectorFMULProc kVectorFMULProc = FMUL_SSE;
#else
  // TODO(dalecurtis): Remove function level static initialization, it's not
  // thread safe: http://crbug.com/224662.
  static const VectorFMULProc kVectorFMULProc =
      base::CPU().has_sse() ? FMUL_SSE : FMUL_C;
#endif
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
  static const VectorFMULProc kVectorFMULProc = FMUL_NEON;
#else
  static const VectorFMULProc kVectorFMULProc = FMUL_C;
#endif

  return kVectorFMULProc(src, scale, len, dest);
}

void FMUL_C(const float src[], float scale, int len, float dest[]) {
  for (int i = 0; i < len; ++i)
    dest[i] = src[i] * scale;
}

#if defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
void FMAC_NEON(const float src[], float scale, int len, float dest[]) {
  const int rem = len % 4;
  const int last_index = len - rem;
  float32x4_t m_scale = vmovq_n_f32(scale);
  for (int i = 0; i < last_index; i += 4) {
    vst1q_f32(dest + i, vmlaq_f32(
        vld1q_f32(dest + i), vld1q_f32(src + i), m_scale));
  }

  // Handle any remaining values that wouldn't fit in an NEON pass.
  for (int i = last_index; i < len; ++i)
    dest[i] += src[i] * scale;
}

void FMUL_NEON(const float src[], float scale, int len, float dest[]) {
  const int rem = len % 4;
  const int last_index = len - rem;
  float32x4_t m_scale = vmovq_n_f32(scale);
  for (int i = 0; i < last_index; i += 4)
    vst1q_f32(dest + i, vmulq_f32(vld1q_f32(src + i), m_scale));

  // Handle any remaining values that wouldn't fit in an NEON pass.
  for (int i = last_index; i < len; ++i)
    dest[i] = src[i] * scale;
}
#endif

}  // namespace vector_math
}  // namespace media
