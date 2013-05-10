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

// If we know the minimum architecture at compile time, avoid CPU detection.
// Force NaCl code to use C routines since (at present) nothing there uses these
// methods and plumbing the -msse built library is non-trivial.  iOS lies about
// its architecture, so we also need to exclude it here.
#if defined(ARCH_CPU_X86_FAMILY) && !defined(OS_NACL) && !defined(OS_IOS)
#if defined(__SSE__)
#define FMAC_FUNC FMAC_SSE
#define FMUL_FUNC FMUL_SSE
void Initialize() {}
#else
// X86 CPU detection required.  Functions will be set by Initialize().
// TODO(dalecurtis): Once Chrome moves to an SSE baseline this can be removed.
#define FMAC_FUNC g_fmac_proc_
#define FMUL_FUNC g_fmul_proc_

typedef void (*MathProc)(const float src[], float scale, int len, float dest[]);
static MathProc g_fmac_proc_ = NULL;
static MathProc g_fmul_proc_ = NULL;

void Initialize() {
  CHECK(!g_fmac_proc_);
  CHECK(!g_fmul_proc_);
  const bool kUseSSE = base::CPU().has_sse();
  g_fmac_proc_ = kUseSSE ? FMAC_SSE : FMAC_C;
  g_fmul_proc_ = kUseSSE ? FMUL_SSE : FMUL_C;
}
#endif
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
#define FMAC_FUNC FMAC_NEON
#define FMUL_FUNC FMUL_NEON
void Initialize() {}
#else
// Unknown architecture.
#define FMAC_FUNC FMAC_C
#define FMUL_FUNC FMUL_C
void Initialize() {}
#endif

void FMAC(const float src[], float scale, int len, float dest[]) {
  // Ensure |src| and |dest| are 16-byte aligned.
  DCHECK_EQ(0u, reinterpret_cast<uintptr_t>(src) & (kRequiredAlignment - 1));
  DCHECK_EQ(0u, reinterpret_cast<uintptr_t>(dest) & (kRequiredAlignment - 1));
  return FMAC_FUNC(src, scale, len, dest);
}

void FMAC_C(const float src[], float scale, int len, float dest[]) {
  for (int i = 0; i < len; ++i)
    dest[i] += src[i] * scale;
}

void FMUL(const float src[], float scale, int len, float dest[]) {
  // Ensure |src| and |dest| are 16-byte aligned.
  DCHECK_EQ(0u, reinterpret_cast<uintptr_t>(src) & (kRequiredAlignment - 1));
  DCHECK_EQ(0u, reinterpret_cast<uintptr_t>(dest) & (kRequiredAlignment - 1));
  return FMUL_FUNC(src, scale, len, dest);
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
