// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/numerics/safe_conversions.h"

#include <stdint.h>

#include <limits>

#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

// Enumerates the five different conversions types we need to test.
enum NumericConversionType {
  SIGN_PRESERVING_VALUE_PRESERVING,
  SIGN_PRESERVING_NARROW,
  SIGN_TO_UNSIGN_WIDEN_OR_EQUAL,
  SIGN_TO_UNSIGN_NARROW,
  UNSIGN_TO_SIGN_NARROW_OR_EQUAL,
};

// Template covering the different conversion tests.
template <typename Dst, typename Src, NumericConversionType conversion>
struct TestNumericConversion {};

// EXPECT_EQ wrapper providing specific detail on test failures.
#define TEST_EXPECTED_RANGE(expected, actual) \
  EXPECT_EQ(expected, RangeCheck<Dst>(actual)) << \
  "Conversion test: " << src << " value " << actual << \
  " to " << dst << " on line " << line;

template <typename Dst, typename Src>
struct TestNumericConversion<Dst, Src, SIGN_PRESERVING_VALUE_PRESERVING> {
  static void Test(const char *dst, const char *src, int line) {
    typedef std::numeric_limits<Src> SrcLimits;
    typedef std::numeric_limits<Dst> DstLimits;
                   // Integral to floating.
    COMPILE_ASSERT((DstLimits::is_iec559 && SrcLimits::is_integer) ||
                   // Not floating to integral and...
                   (!(DstLimits::is_integer && SrcLimits::is_iec559) &&
                    // Same sign, same numeric, source is narrower or same.
                    ((SrcLimits::is_signed == DstLimits::is_signed &&
                     sizeof(Dst) >= sizeof(Src)) ||
                    // Or signed destination and source is smaller
                     (DstLimits::is_signed && sizeof(Dst) > sizeof(Src)))),
                   comparison_must_be_sign_preserving_and_value_preserving);

    TEST_EXPECTED_RANGE(TYPE_VALID, SrcLimits::max());
    TEST_EXPECTED_RANGE(TYPE_VALID, static_cast<Src>(1));
    if (SrcLimits::is_iec559) {
      TEST_EXPECTED_RANGE(TYPE_VALID, SrcLimits::max() * static_cast<Src>(-1));
      TEST_EXPECTED_RANGE(TYPE_OVERFLOW, SrcLimits::infinity());
      TEST_EXPECTED_RANGE(TYPE_UNDERFLOW, SrcLimits::infinity() * -1);
      TEST_EXPECTED_RANGE(TYPE_INVALID, SrcLimits::quiet_NaN());
    } else if (std::numeric_limits<Src>::is_signed) {
      TEST_EXPECTED_RANGE(TYPE_VALID, static_cast<Src>(-1));
      TEST_EXPECTED_RANGE(TYPE_VALID, SrcLimits::min());
    }
  }
};

template <typename Dst, typename Src>
struct TestNumericConversion<Dst, Src, SIGN_PRESERVING_NARROW> {
  static void Test(const char *dst, const char *src, int line) {
    typedef std::numeric_limits<Src> SrcLimits;
    typedef std::numeric_limits<Dst> DstLimits;
    COMPILE_ASSERT(SrcLimits::is_signed == DstLimits::is_signed,
                   destination_and_source_sign_must_be_the_same);
    COMPILE_ASSERT(sizeof(Dst) < sizeof(Src) ||
                   (DstLimits::is_integer && SrcLimits::is_iec559),
                   destination_must_be_narrower_than_source);

    TEST_EXPECTED_RANGE(TYPE_OVERFLOW, SrcLimits::max());
    TEST_EXPECTED_RANGE(TYPE_VALID, static_cast<Src>(1));
    if (SrcLimits::is_iec559) {
      TEST_EXPECTED_RANGE(TYPE_UNDERFLOW, SrcLimits::max() * -1);
      TEST_EXPECTED_RANGE(TYPE_VALID, static_cast<Src>(-1));
      TEST_EXPECTED_RANGE(TYPE_OVERFLOW, SrcLimits::infinity());
      TEST_EXPECTED_RANGE(TYPE_UNDERFLOW, SrcLimits::infinity() * -1);
      TEST_EXPECTED_RANGE(TYPE_INVALID, SrcLimits::quiet_NaN());
    } else if (SrcLimits::is_signed) {
      TEST_EXPECTED_RANGE(TYPE_UNDERFLOW, SrcLimits::min());
      TEST_EXPECTED_RANGE(TYPE_VALID, static_cast<Src>(-1));
    } else {
      TEST_EXPECTED_RANGE(TYPE_VALID, SrcLimits::min());
    }
  }
};

template <typename Dst, typename Src>
struct TestNumericConversion<Dst, Src, SIGN_TO_UNSIGN_WIDEN_OR_EQUAL> {
  static void Test(const char *dst, const char *src, int line) {
    typedef std::numeric_limits<Src> SrcLimits;
    typedef std::numeric_limits<Dst> DstLimits;
    COMPILE_ASSERT(sizeof(Dst) >= sizeof(Src),
                   destination_must_be_equal_or_wider_than_source);
    COMPILE_ASSERT(SrcLimits::is_signed, source_must_be_signed);
    COMPILE_ASSERT(!DstLimits::is_signed, destination_must_be_unsigned);

    TEST_EXPECTED_RANGE(TYPE_UNDERFLOW, SrcLimits::min());
    TEST_EXPECTED_RANGE(TYPE_VALID, SrcLimits::max());
    TEST_EXPECTED_RANGE(TYPE_VALID, static_cast<Src>(1));
    TEST_EXPECTED_RANGE(TYPE_UNDERFLOW, static_cast<Src>(-1));
  }
};

template <typename Dst, typename Src>
struct TestNumericConversion<Dst, Src, SIGN_TO_UNSIGN_NARROW> {
  static void Test(const char *dst, const char *src, int line) {
    typedef std::numeric_limits<Src> SrcLimits;
    typedef std::numeric_limits<Dst> DstLimits;
    COMPILE_ASSERT((DstLimits::is_integer && SrcLimits::is_iec559) ||
                   (sizeof(Dst) < sizeof(Src)),
      destination_must_be_narrower_than_source);
    COMPILE_ASSERT(SrcLimits::is_signed, source_must_be_signed);
    COMPILE_ASSERT(!DstLimits::is_signed, destination_must_be_unsigned);

    TEST_EXPECTED_RANGE(TYPE_OVERFLOW, SrcLimits::max());
    TEST_EXPECTED_RANGE(TYPE_VALID, static_cast<Src>(1));
    TEST_EXPECTED_RANGE(TYPE_UNDERFLOW, static_cast<Src>(-1));
    if (SrcLimits::is_iec559) {
      TEST_EXPECTED_RANGE(TYPE_UNDERFLOW, SrcLimits::max() * -1);
      TEST_EXPECTED_RANGE(TYPE_OVERFLOW, SrcLimits::infinity());
      TEST_EXPECTED_RANGE(TYPE_UNDERFLOW, SrcLimits::infinity() * -1);
      TEST_EXPECTED_RANGE(TYPE_INVALID, SrcLimits::quiet_NaN());
    } else {
      TEST_EXPECTED_RANGE(TYPE_UNDERFLOW, SrcLimits::min());
    }
  }
};

template <typename Dst, typename Src>
struct TestNumericConversion<Dst, Src, UNSIGN_TO_SIGN_NARROW_OR_EQUAL> {
  static void Test(const char *dst, const char *src, int line) {
    typedef std::numeric_limits<Src> SrcLimits;
    typedef std::numeric_limits<Dst> DstLimits;
    COMPILE_ASSERT(sizeof(Dst) <= sizeof(Src),
                   destination_must_be_narrower_or_equal_to_source);
    COMPILE_ASSERT(!SrcLimits::is_signed, source_must_be_unsigned);
    COMPILE_ASSERT(DstLimits::is_signed, destination_must_be_signed);

    TEST_EXPECTED_RANGE(TYPE_VALID, SrcLimits::min());
    TEST_EXPECTED_RANGE(TYPE_OVERFLOW, SrcLimits::max());
    TEST_EXPECTED_RANGE(TYPE_VALID, static_cast<Src>(1));
  }
};

// Helper macro to wrap displaying the conversion types and line numbers
#define TEST_NUMERIC_CONVERSION(d, s, t) \
  TestNumericConversion<d, s, t>::Test(#d, #s, __LINE__)

TEST(SafeNumerics, IntMinConversions) {
  TEST_NUMERIC_CONVERSION(int8_t, int8_t, SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(uint8_t, uint8_t, SIGN_PRESERVING_VALUE_PRESERVING);

  TEST_NUMERIC_CONVERSION(int8_t, int, SIGN_PRESERVING_NARROW);
  TEST_NUMERIC_CONVERSION(uint8_t, unsigned int, SIGN_PRESERVING_NARROW);
  TEST_NUMERIC_CONVERSION(int8_t, float, SIGN_PRESERVING_NARROW);

  TEST_NUMERIC_CONVERSION(uint8_t, int8_t, SIGN_TO_UNSIGN_WIDEN_OR_EQUAL);

  TEST_NUMERIC_CONVERSION(uint8_t, int, SIGN_TO_UNSIGN_NARROW);
  TEST_NUMERIC_CONVERSION(uint8_t, intmax_t, SIGN_TO_UNSIGN_NARROW);
  TEST_NUMERIC_CONVERSION(uint8_t, float, SIGN_TO_UNSIGN_NARROW);

  TEST_NUMERIC_CONVERSION(int8_t, unsigned int, UNSIGN_TO_SIGN_NARROW_OR_EQUAL);
  TEST_NUMERIC_CONVERSION(int8_t, uintmax_t, UNSIGN_TO_SIGN_NARROW_OR_EQUAL);
}

TEST(SafeNumerics, IntConversions) {
  TEST_NUMERIC_CONVERSION(int, int, SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(unsigned int, unsigned int,
                          SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(int, int8_t, SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(unsigned int, uint8_t,
                          SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(int, uint8_t, SIGN_PRESERVING_VALUE_PRESERVING);

  TEST_NUMERIC_CONVERSION(int, intmax_t, SIGN_PRESERVING_NARROW);
  TEST_NUMERIC_CONVERSION(unsigned int, uintmax_t, SIGN_PRESERVING_NARROW);
  TEST_NUMERIC_CONVERSION(int, float, SIGN_PRESERVING_NARROW);
  TEST_NUMERIC_CONVERSION(int, double, SIGN_PRESERVING_NARROW);

  TEST_NUMERIC_CONVERSION(unsigned int, int, SIGN_TO_UNSIGN_WIDEN_OR_EQUAL);
  TEST_NUMERIC_CONVERSION(unsigned int, int8_t, SIGN_TO_UNSIGN_WIDEN_OR_EQUAL);

  TEST_NUMERIC_CONVERSION(unsigned int, intmax_t, SIGN_TO_UNSIGN_NARROW);
  TEST_NUMERIC_CONVERSION(unsigned int, float, SIGN_TO_UNSIGN_NARROW);
  TEST_NUMERIC_CONVERSION(unsigned int, double, SIGN_TO_UNSIGN_NARROW);

  TEST_NUMERIC_CONVERSION(int, unsigned int, UNSIGN_TO_SIGN_NARROW_OR_EQUAL);
  TEST_NUMERIC_CONVERSION(int, uintmax_t, UNSIGN_TO_SIGN_NARROW_OR_EQUAL);
}

TEST(SafeNumerics, IntMaxConversions) {
  TEST_NUMERIC_CONVERSION(intmax_t, intmax_t, SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(uintmax_t, uintmax_t,
                          SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(intmax_t, int, SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(uintmax_t, unsigned int,
                          SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(intmax_t, unsigned int,
                          SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(intmax_t, uint8_t, SIGN_PRESERVING_VALUE_PRESERVING);

  TEST_NUMERIC_CONVERSION(intmax_t, float, SIGN_PRESERVING_NARROW);
  TEST_NUMERIC_CONVERSION(intmax_t, double, SIGN_PRESERVING_NARROW);

  TEST_NUMERIC_CONVERSION(uintmax_t, int, SIGN_TO_UNSIGN_WIDEN_OR_EQUAL);
  TEST_NUMERIC_CONVERSION(uintmax_t, int8_t, SIGN_TO_UNSIGN_WIDEN_OR_EQUAL);

  TEST_NUMERIC_CONVERSION(uintmax_t, float, SIGN_TO_UNSIGN_NARROW);
  TEST_NUMERIC_CONVERSION(uintmax_t, double, SIGN_TO_UNSIGN_NARROW);

  TEST_NUMERIC_CONVERSION(intmax_t, uintmax_t, UNSIGN_TO_SIGN_NARROW_OR_EQUAL);
}

TEST(SafeNumerics, FloatConversions) {
  TEST_NUMERIC_CONVERSION(float, intmax_t, SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(float, uintmax_t,
                          SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(float, int, SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(float, unsigned int,
                          SIGN_PRESERVING_VALUE_PRESERVING);

  TEST_NUMERIC_CONVERSION(float, double, SIGN_PRESERVING_NARROW);
}

TEST(SafeNumerics, DoubleConversions) {
  TEST_NUMERIC_CONVERSION(double, intmax_t, SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(double, uintmax_t,
                          SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(double, int, SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(double, unsigned int,
                          SIGN_PRESERVING_VALUE_PRESERVING);
}

TEST(SafeNumerics, SizeTConversions) {
  TEST_NUMERIC_CONVERSION(size_t, int, SIGN_TO_UNSIGN_WIDEN_OR_EQUAL);
  TEST_NUMERIC_CONVERSION(int, size_t, UNSIGN_TO_SIGN_NARROW_OR_EQUAL);
}

TEST(SafeNumerics, CastTests) {
// MSVC catches and warns that we're forcing saturation in these tests.
// Since that's intentional, we need to shut this warning off.
#if defined(COMPILER_MSVC)
#pragma warning(disable : 4756)
#endif

  int small_positive = 1;
  int small_negative = -1;
  double double_small = 1.0;
  double double_large = std::numeric_limits<double>::max();
  double double_infinity = std::numeric_limits<float>::infinity();

  // Just test that the cast compiles, since the other tests cover logic.
  EXPECT_EQ(0, base::checked_cast<int>(static_cast<size_t>(0)));

  // Test various saturation corner cases.
  EXPECT_EQ(saturated_cast<int>(small_negative),
            static_cast<int>(small_negative));
  EXPECT_EQ(saturated_cast<int>(small_positive),
            static_cast<int>(small_positive));
  EXPECT_EQ(saturated_cast<unsigned>(small_negative),
            static_cast<unsigned>(0));
  EXPECT_EQ(saturated_cast<int>(double_small),
            static_cast<int>(double_small));
  EXPECT_EQ(saturated_cast<int>(double_large),
            std::numeric_limits<int>::max());
  EXPECT_EQ(saturated_cast<float>(double_large), double_infinity);
  EXPECT_EQ(saturated_cast<float>(-double_large), -double_infinity);
}

}  // namespace internal
}  // namespace base

