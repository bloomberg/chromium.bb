// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/vector_math.h"
#include "media/base/vector_math_testing.h"

#include <algorithm>

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
// methods and plumbing the -msse built library is non-trivial.
#if defined(ARCH_CPU_X86_FAMILY) && !defined(OS_NACL)
#if defined(__SSE__)
#define FMAC_FUNC FMAC_SSE
#define FMUL_FUNC FMUL_SSE
#define EWMAAndMaxPower_FUNC EWMAAndMaxPower_SSE
void Initialize() {}
#else
// X86 CPU detection required.  Functions will be set by Initialize().
// TODO(dalecurtis): Once Chrome moves to an SSE baseline this can be removed.
#define FMAC_FUNC g_fmac_proc_
#define FMUL_FUNC g_fmul_proc_
#define EWMAAndMaxPower_FUNC g_ewma_power_proc_

typedef void (*MathProc)(const float src[], float scale, int len, float dest[]);
static MathProc g_fmac_proc_ = NULL;
static MathProc g_fmul_proc_ = NULL;
typedef std::pair<float, float> (*EWMAAndMaxPowerProc)(
    float initial_value, const float src[], int len, float smoothing_factor);
static EWMAAndMaxPowerProc g_ewma_power_proc_ = NULL;

void Initialize() {
  CHECK(!g_fmac_proc_);
  CHECK(!g_fmul_proc_);
  CHECK(!g_ewma_power_proc_);
  const bool kUseSSE = base::CPU().has_sse();
  g_fmac_proc_ = kUseSSE ? FMAC_SSE : FMAC_C;
  g_fmul_proc_ = kUseSSE ? FMUL_SSE : FMUL_C;
  g_ewma_power_proc_ = kUseSSE ? EWMAAndMaxPower_SSE : EWMAAndMaxPower_C;
}
#endif
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
#define FMAC_FUNC FMAC_NEON
#define FMUL_FUNC FMUL_NEON
#define EWMAAndMaxPower_FUNC EWMAAndMaxPower_NEON
void Initialize() {}
#else
// Unknown architecture.
#define FMAC_FUNC FMAC_C
#define FMUL_FUNC FMUL_C
#define EWMAAndMaxPower_FUNC EWMAAndMaxPower_C
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

std::pair<float, float> EWMAAndMaxPower(
    float initial_value, const float src[], int len, float smoothing_factor) {
  // Ensure |src| is 16-byte aligned.
  DCHECK_EQ(0u, reinterpret_cast<uintptr_t>(src) & (kRequiredAlignment - 1));
  return EWMAAndMaxPower_FUNC(initial_value, src, len, smoothing_factor);
}

std::pair<float, float> EWMAAndMaxPower_C(
    float initial_value, const float src[], int len, float smoothing_factor) {
  std::pair<float, float> result(initial_value, 0.0f);
  const float weight_prev = 1.0f - smoothing_factor;
  for (int i = 0; i < len; ++i) {
    result.first *= weight_prev;
    const float sample = src[i];
    const float sample_squared = sample * sample;
    result.first += sample_squared * smoothing_factor;
    result.second = std::max(result.second, sample_squared);
  }
  return result;
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

std::pair<float, float> EWMAAndMaxPower_NEON(
    float initial_value, const float src[], int len, float smoothing_factor) {
  // When the recurrence is unrolled, we see that we can split it into 4
  // separate lanes of evaluation:
  //
  // y[n] = a(S[n]^2) + (1-a)(y[n-1])
  //      = a(S[n]^2) + (1-a)^1(aS[n-1]^2) + (1-a)^2(aS[n-2]^2) + ...
  //      = z[n] + (1-a)^1(z[n-1]) + (1-a)^2(z[n-2]) + (1-a)^3(z[n-3])
  //
  // where z[n] = a(S[n]^2) + (1-a)^4(z[n-4]) + (1-a)^8(z[n-8]) + ...
  //
  // Thus, the strategy here is to compute z[n], z[n-1], z[n-2], and z[n-3] in
  // each of the 4 lanes, and then combine them to give y[n].

  const int rem = len % 4;
  const int last_index = len - rem;

  const float32x4_t smoothing_factor_x4 = vdupq_n_f32(smoothing_factor);
  const float weight_prev = 1.0f - smoothing_factor;
  const float32x4_t weight_prev_x4 = vdupq_n_f32(weight_prev);
  const float32x4_t weight_prev_squared_x4 =
      vmulq_f32(weight_prev_x4, weight_prev_x4);
  const float32x4_t weight_prev_4th_x4 =
      vmulq_f32(weight_prev_squared_x4, weight_prev_squared_x4);

  // Compute z[n], z[n-1], z[n-2], and z[n-3] in parallel in lanes 3, 2, 1 and
  // 0, respectively.
  float32x4_t max_x4 = vdupq_n_f32(0.0f);
  float32x4_t ewma_x4 = vsetq_lane_f32(initial_value, vdupq_n_f32(0.0f), 3);
  int i;
  for (i = 0; i < last_index; i += 4) {
    ewma_x4 = vmulq_f32(ewma_x4, weight_prev_4th_x4);
    const float32x4_t sample_x4 = vld1q_f32(src + i);
    const float32x4_t sample_squared_x4 = vmulq_f32(sample_x4, sample_x4);
    max_x4 = vmaxq_f32(max_x4, sample_squared_x4);
    ewma_x4 = vmlaq_f32(ewma_x4, sample_squared_x4, smoothing_factor_x4);
  }

  // y[n] = z[n] + (1-a)^1(z[n-1]) + (1-a)^2(z[n-2]) + (1-a)^3(z[n-3])
  float ewma = vgetq_lane_f32(ewma_x4, 3);
  ewma_x4 = vmulq_f32(ewma_x4, weight_prev_x4);
  ewma += vgetq_lane_f32(ewma_x4, 2);
  ewma_x4 = vmulq_f32(ewma_x4, weight_prev_x4);
  ewma += vgetq_lane_f32(ewma_x4, 1);
  ewma_x4 = vmulq_f32(ewma_x4, weight_prev_x4);
  ewma += vgetq_lane_f32(ewma_x4, 0);

  // Fold the maximums together to get the overall maximum.
  float32x2_t max_x2 = vpmax_f32(vget_low_f32(max_x4), vget_high_f32(max_x4));
  max_x2 = vpmax_f32(max_x2, max_x2);

  std::pair<float, float> result(ewma, vget_lane_f32(max_x2, 0));

  // Handle remaining values at the end of |src|.
  for (; i < len; ++i) {
    result.first *= weight_prev;
    const float sample = src[i];
    const float sample_squared = sample * sample;
    result.first += sample_squared * smoothing_factor;
    result.second = std::max(result.second, sample_squared);
  }

  return result;
}
#endif

}  // namespace vector_math
}  // namespace media
