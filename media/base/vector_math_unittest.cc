// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES
#include <cmath>

#include "base/cpu.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringize_macros.h"
#include "media/base/vector_math.h"
#include "media/base/vector_math_testing.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::fill;

namespace media {

// Default test values.
static const float kScale = 0.5;
static const float kInputFillValue = 1.0;
static const float kOutputFillValue = 3.0;
static const int kVectorSize = 8192;

class VectorMathTest : public testing::Test {
 public:

  VectorMathTest() {
    // Initialize input and output vectors.
    input_vector_.reset(static_cast<float*>(base::AlignedAlloc(
        sizeof(float) * kVectorSize, vector_math::kRequiredAlignment)));
    output_vector_.reset(static_cast<float*>(base::AlignedAlloc(
        sizeof(float) * kVectorSize, vector_math::kRequiredAlignment)));
  }

  void FillTestVectors(float input, float output) {
    // Setup input and output vectors.
    fill(input_vector_.get(), input_vector_.get() + kVectorSize, input);
    fill(output_vector_.get(), output_vector_.get() + kVectorSize, output);
  }

  void VerifyOutput(float value) {
    for (int i = 0; i < kVectorSize; ++i)
      ASSERT_FLOAT_EQ(output_vector_.get()[i], value);
  }

 protected:
  scoped_ptr_malloc<float, base::ScopedPtrAlignedFree> input_vector_;
  scoped_ptr_malloc<float, base::ScopedPtrAlignedFree> output_vector_;

  DISALLOW_COPY_AND_ASSIGN(VectorMathTest);
};

// Ensure each optimized vector_math::FMAC() method returns the same value.
TEST_F(VectorMathTest, FMAC) {
  static const float kResult = kInputFillValue * kScale + kOutputFillValue;

  {
    SCOPED_TRACE("FMAC");
    FillTestVectors(kInputFillValue, kOutputFillValue);
    vector_math::FMAC(
        input_vector_.get(), kScale, kVectorSize, output_vector_.get());
    VerifyOutput(kResult);
  }

  {
    SCOPED_TRACE("FMAC_C");
    FillTestVectors(kInputFillValue, kOutputFillValue);
    vector_math::FMAC_C(
        input_vector_.get(), kScale, kVectorSize, output_vector_.get());
    VerifyOutput(kResult);
  }

#if defined(ARCH_CPU_X86_FAMILY)
  {
    ASSERT_TRUE(base::CPU().has_sse());
    SCOPED_TRACE("FMAC_SSE");
    FillTestVectors(kInputFillValue, kOutputFillValue);
    vector_math::FMAC_SSE(
        input_vector_.get(), kScale, kVectorSize, output_vector_.get());
    VerifyOutput(kResult);
  }
#endif

#if defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
  {
    SCOPED_TRACE("FMAC_NEON");
    FillTestVectors(kInputFillValue, kOutputFillValue);
    vector_math::FMAC_NEON(
        input_vector_.get(), kScale, kVectorSize, output_vector_.get());
    VerifyOutput(kResult);
  }
#endif
}

// Ensure each optimized vector_math::FMUL() method returns the same value.
TEST_F(VectorMathTest, FMUL) {
  static const float kResult = kInputFillValue * kScale;

  {
    SCOPED_TRACE("FMUL");
    FillTestVectors(kInputFillValue, kOutputFillValue);
    vector_math::FMUL(
        input_vector_.get(), kScale, kVectorSize, output_vector_.get());
    VerifyOutput(kResult);
  }

  {
    SCOPED_TRACE("FMUL_C");
    FillTestVectors(kInputFillValue, kOutputFillValue);
    vector_math::FMUL_C(
        input_vector_.get(), kScale, kVectorSize, output_vector_.get());
    VerifyOutput(kResult);
  }

#if defined(ARCH_CPU_X86_FAMILY)
  {
    ASSERT_TRUE(base::CPU().has_sse());
    SCOPED_TRACE("FMUL_SSE");
    FillTestVectors(kInputFillValue, kOutputFillValue);
    vector_math::FMUL_SSE(
        input_vector_.get(), kScale, kVectorSize, output_vector_.get());
    VerifyOutput(kResult);
  }
#endif

#if defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
  {
    SCOPED_TRACE("FMUL_NEON");
    FillTestVectors(kInputFillValue, kOutputFillValue);
    vector_math::FMUL_NEON(
        input_vector_.get(), kScale, kVectorSize, output_vector_.get());
    VerifyOutput(kResult);
  }
#endif
}

}  // namespace media
