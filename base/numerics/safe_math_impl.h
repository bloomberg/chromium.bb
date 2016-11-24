// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NUMERICS_SAFE_MATH_IMPL_H_
#define BASE_NUMERICS_SAFE_MATH_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <climits>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <type_traits>

#include "base/numerics/safe_conversions.h"

namespace base {
namespace internal {

// Everything from here up to the floating point operations is portable C++,
// but it may not be fast. This code could be split based on
// platform/architecture and replaced with potentially faster implementations.

// Integer promotion templates used by the portable checked integer arithmetic.
template <size_t Size, bool IsSigned>
struct IntegerForSizeAndSign;
template <>
struct IntegerForSizeAndSign<1, true> {
  typedef int8_t type;
};
template <>
struct IntegerForSizeAndSign<1, false> {
  typedef uint8_t type;
};
template <>
struct IntegerForSizeAndSign<2, true> {
  typedef int16_t type;
};
template <>
struct IntegerForSizeAndSign<2, false> {
  typedef uint16_t type;
};
template <>
struct IntegerForSizeAndSign<4, true> {
  typedef int32_t type;
};
template <>
struct IntegerForSizeAndSign<4, false> {
  typedef uint32_t type;
};
template <>
struct IntegerForSizeAndSign<8, true> {
  typedef int64_t type;
};
template <>
struct IntegerForSizeAndSign<8, false> {
  typedef uint64_t type;
  static_assert(sizeof(uintmax_t) == 8,
                "Max integer size not supported for this toolchain.");
};

// WARNING: We have no IntegerForSizeAndSign<16, *>. If we ever add one to
// support 128-bit math, then the ArithmeticPromotion template below will need
// to be updated (or more likely replaced with a decltype expression).

template <typename Integer>
struct UnsignedIntegerForSize {
  typedef typename std::enable_if<
      std::numeric_limits<Integer>::is_integer,
      typename IntegerForSizeAndSign<sizeof(Integer), false>::type>::type type;
};

template <typename Integer>
struct SignedIntegerForSize {
  typedef typename std::enable_if<
      std::numeric_limits<Integer>::is_integer,
      typename IntegerForSizeAndSign<sizeof(Integer), true>::type>::type type;
};

template <typename Integer>
struct TwiceWiderInteger {
  typedef typename std::enable_if<
      std::numeric_limits<Integer>::is_integer,
      typename IntegerForSizeAndSign<
          sizeof(Integer) * 2,
          std::numeric_limits<Integer>::is_signed>::type>::type type;
};

template <typename Integer>
struct PositionOfSignBit {
  static const typename std::enable_if<std::numeric_limits<Integer>::is_integer,
                                       size_t>::type value =
      CHAR_BIT * sizeof(Integer) - 1;
};

// This is used for UnsignedAbs, where we need to support floating-point
// template instantiations even though we don't actually support the operations.
// However, there is no corresponding implementation of e.g. SafeUnsignedAbs,
// so the float versions will not compile.
template <typename Numeric,
          bool IsInteger = std::numeric_limits<Numeric>::is_integer,
          bool IsFloat = std::numeric_limits<Numeric>::is_iec559>
struct UnsignedOrFloatForSize;

template <typename Numeric>
struct UnsignedOrFloatForSize<Numeric, true, false> {
  typedef typename UnsignedIntegerForSize<Numeric>::type type;
};

template <typename Numeric>
struct UnsignedOrFloatForSize<Numeric, false, true> {
  typedef Numeric type;
};

// Helper templates for integer manipulations.

template <typename T>
constexpr bool HasSignBit(T x) {
  // Cast to unsigned since right shift on signed is undefined.
  return !!(static_cast<typename UnsignedIntegerForSize<T>::type>(x) >>
            PositionOfSignBit<T>::value);
}

// This wrapper undoes the standard integer promotions.
template <typename T>
constexpr T BinaryComplement(T x) {
  return static_cast<T>(~x);
}

enum ArithmeticPromotionCategory {
  LEFT_PROMOTION,          // Use the type of the left-hand argument.
  RIGHT_PROMOTION,         // Use the type of the right-hand argument.
  MAX_EXPONENT_PROMOTION,  // Use the type supporting the largest exponent.
  BIG_ENOUGH_PROMOTION     // Attempt to find a big enough type.
};

template <ArithmeticPromotionCategory Promotion,
          typename Lhs,
          typename Rhs = Lhs>
struct ArithmeticPromotion;

template <typename Lhs,
          typename Rhs,
          ArithmeticPromotionCategory Promotion =
              (MaxExponent<Lhs>::value > MaxExponent<Rhs>::value)
                  ? LEFT_PROMOTION
                  : RIGHT_PROMOTION>
struct MaxExponentPromotion;

template <typename Lhs, typename Rhs>
struct MaxExponentPromotion<Lhs, Rhs, LEFT_PROMOTION> {
  using type = Lhs;
};

template <typename Lhs, typename Rhs>
struct MaxExponentPromotion<Lhs, Rhs, RIGHT_PROMOTION> {
  using type = Rhs;
};

template <typename Lhs,
          typename Rhs = Lhs,
          bool is_intmax_type =
              std::is_integral<
                  typename MaxExponentPromotion<Lhs, Rhs>::type>::value &&
              sizeof(typename MaxExponentPromotion<Lhs, Rhs>::type) ==
                  sizeof(intmax_t),
          bool is_max_exponent =
              StaticDstRangeRelationToSrcRange<
                  typename MaxExponentPromotion<Lhs, Rhs>::type,
                  Lhs>::value ==
              NUMERIC_RANGE_CONTAINED&& StaticDstRangeRelationToSrcRange<
                  typename MaxExponentPromotion<Lhs, Rhs>::type,
                  Rhs>::value == NUMERIC_RANGE_CONTAINED>
struct BigEnoughPromotion;

// The side with the max exponent is big enough.
template <typename Lhs, typename Rhs, bool is_intmax_type>
struct BigEnoughPromotion<Lhs, Rhs, is_intmax_type, true> {
  using type = typename MaxExponentPromotion<Lhs, Rhs>::type;
  static const bool is_contained = true;
};

// We can use a twice wider type to fit.
template <typename Lhs, typename Rhs>
struct BigEnoughPromotion<Lhs, Rhs, false, false> {
  using type = typename IntegerForSizeAndSign<
      sizeof(typename MaxExponentPromotion<Lhs, Rhs>::type) * 2,
      std::is_signed<Lhs>::value || std::is_signed<Rhs>::value>::type;
  static const bool is_contained = true;
};

// No type is large enough.
template <typename Lhs, typename Rhs>
struct BigEnoughPromotion<Lhs, Rhs, true, false> {
  using type = typename MaxExponentPromotion<Lhs, Rhs>::type;
  static const bool is_contained = false;
};

// These are the supported promotion types.

// Use the type supporting the largest exponent.
template <typename Lhs, typename Rhs>
struct ArithmeticPromotion<MAX_EXPONENT_PROMOTION, Lhs, Rhs> {
  using type = typename MaxExponentPromotion<Lhs, Rhs>::type;
  static const bool is_contained = true;
};

// Attempt to find a big enough type.
template <typename Lhs, typename Rhs>
struct ArithmeticPromotion<BIG_ENOUGH_PROMOTION, Lhs, Rhs> {
  using type = typename BigEnoughPromotion<Lhs, Rhs>::type;
  static const bool is_contained = BigEnoughPromotion<Lhs, Rhs>::is_contained;
};

// We can statically check if operations on the provided types can wrap, so we
// can skip the checked operations if they're not needed. So, for an integer we
// care if the destination type preserves the sign and is twice the width of
// the source.
template <typename T, typename Lhs, typename Rhs>
struct IsIntegerArithmeticSafe {
  static const bool value = !std::numeric_limits<T>::is_iec559 &&
                            StaticDstRangeRelationToSrcRange<T, Lhs>::value ==
                                NUMERIC_RANGE_CONTAINED &&
                            sizeof(T) >= (2 * sizeof(Lhs)) &&
                            StaticDstRangeRelationToSrcRange<T, Rhs>::value !=
                                NUMERIC_RANGE_CONTAINED &&
                            sizeof(T) >= (2 * sizeof(Rhs));
};

// Probe for builtin math overflow support on Clang and version check on GCC.
#if defined(__has_builtin)
#define USE_OVERFLOW_BUILTINS (__has_builtin(__builtin_add_overflow))
#elif defined(__GNUC__)
#define USE_OVERFLOW_BUILTINS (__GNUC__ >= 5)
#else
#define USE_OVERFLOW_BUILTINS (0)
#endif

// Here are the actual portable checked integer math implementations.
// TODO(jschuh): Break this code out from the enable_if pattern and find a clean
// way to coalesce things into the CheckedNumericState specializations below.

template <typename T>
typename std::enable_if<std::numeric_limits<T>::is_integer, bool>::type
CheckedAddImpl(T x, T y, T* result) {
  // Since the value of x+y is undefined if we have a signed type, we compute
  // it using the unsigned type of the same size.
  typedef typename UnsignedIntegerForSize<T>::type UnsignedDst;
  UnsignedDst ux = static_cast<UnsignedDst>(x);
  UnsignedDst uy = static_cast<UnsignedDst>(y);
  UnsignedDst uresult = static_cast<UnsignedDst>(ux + uy);
  *result = static_cast<T>(uresult);
  // Addition is valid if the sign of (x + y) is equal to either that of x or
  // that of y.
  return (std::numeric_limits<T>::is_signed)
             ? HasSignBit(BinaryComplement(
                   static_cast<UnsignedDst>((uresult ^ ux) & (uresult ^ uy))))
             : (BinaryComplement(x) >=
                y);  // Unsigned is either valid or underflow.
}

template <typename T, typename U, class Enable = void>
struct CheckedAddOp {};

template <typename T, typename U>
struct CheckedAddOp<
    T,
    U,
    typename std::enable_if<std::numeric_limits<T>::is_integer &&
                            std::numeric_limits<U>::is_integer>::type> {
  using result_type =
      typename ArithmeticPromotion<MAX_EXPONENT_PROMOTION, T, U>::type;
  template <typename V>
  static bool Do(T x, U y, V* result) {
#if USE_OVERFLOW_BUILTINS
    return !__builtin_add_overflow(x, y, result);
#else
    using Promotion =
        typename ArithmeticPromotion<BIG_ENOUGH_PROMOTION, T, U>::type;
    Promotion presult;
    // Fail if either operand is out of range for the promoted type.
    // TODO(jschuh): This could be made to work for a broader range of values.
    bool is_valid = IsValueInRangeForNumericType<Promotion>(x) &&
                    IsValueInRangeForNumericType<Promotion>(y);

    if (IsIntegerArithmeticSafe<Promotion, T, U>::value) {
      presult = static_cast<Promotion>(x) + static_cast<Promotion>(y);
    } else {
      is_valid &= CheckedAddImpl(static_cast<Promotion>(x),
                                 static_cast<Promotion>(y), &presult);
    }
    *result = static_cast<V>(presult);
    return is_valid && IsValueInRangeForNumericType<V>(presult);
#endif
  }
};

template <typename T>
typename std::enable_if<std::numeric_limits<T>::is_integer, bool>::type
CheckedSubImpl(T x, T y, T* result) {
  // Since the value of x+y is undefined if we have a signed type, we compute
  // it using the unsigned type of the same size.
  typedef typename UnsignedIntegerForSize<T>::type UnsignedDst;
  UnsignedDst ux = static_cast<UnsignedDst>(x);
  UnsignedDst uy = static_cast<UnsignedDst>(y);
  UnsignedDst uresult = static_cast<UnsignedDst>(ux - uy);
  *result = static_cast<T>(uresult);
  // Subtraction is valid if either x and y have same sign, or (x-y) and x have
  // the same sign.
  return (std::numeric_limits<T>::is_signed)
             ? HasSignBit(BinaryComplement(
                   static_cast<UnsignedDst>((uresult ^ ux) & (ux ^ uy))))
             : (x >= y);
}

template <typename T, typename U, class Enable = void>
struct CheckedSubOp {};

template <typename T, typename U>
struct CheckedSubOp<
    T,
    U,
    typename std::enable_if<std::numeric_limits<T>::is_integer &&
                            std::numeric_limits<U>::is_integer>::type> {
  using result_type =
      typename ArithmeticPromotion<MAX_EXPONENT_PROMOTION, T, U>::type;
  template <typename V>
  static bool Do(T x, U y, V* result) {
#if USE_OVERFLOW_BUILTINS
    return !__builtin_sub_overflow(x, y, result);
#else
    using Promotion =
        typename ArithmeticPromotion<BIG_ENOUGH_PROMOTION, T, U>::type;
    Promotion presult;
    // Fail if either operand is out of range for the promoted type.
    // TODO(jschuh): This could be made to work for a broader range of values.
    bool is_valid = IsValueInRangeForNumericType<Promotion>(x) &&
                    IsValueInRangeForNumericType<Promotion>(y);

    if (IsIntegerArithmeticSafe<Promotion, T, U>::value) {
      presult = static_cast<Promotion>(x) - static_cast<Promotion>(y);
    } else {
      is_valid &= CheckedSubImpl(static_cast<Promotion>(x),
                                 static_cast<Promotion>(y), &presult);
    }
    *result = static_cast<V>(presult);
    return is_valid && IsValueInRangeForNumericType<V>(presult);
#endif
  }
};

// Integer multiplication is a bit complicated. In the fast case we just
// we just promote to a twice wider type, and range check the result. In the
// slow case we need to manually check that the result won't be truncated by
// checking with division against the appropriate bound.
template <typename T>
typename std::enable_if<std::numeric_limits<T>::is_integer &&
                            sizeof(T) * 2 <= sizeof(uintmax_t),
                        bool>::type
CheckedMulImpl(T x, T y, T* result) {
  typedef typename TwiceWiderInteger<T>::type IntermediateType;
  IntermediateType tmp =
      static_cast<IntermediateType>(x) * static_cast<IntermediateType>(y);
  *result = static_cast<T>(tmp);
  return DstRangeRelationToSrcRange<T>(tmp) == RANGE_VALID;
}

template <typename T>
typename std::enable_if<std::numeric_limits<T>::is_integer &&
                            std::numeric_limits<T>::is_signed &&
                            (sizeof(T) * 2 > sizeof(uintmax_t)),
                        bool>::type
CheckedMulImpl(T x, T y, T* result) {
  if (x && y) {
    if (x > 0) {
      if (y > 0) {
        if (x > std::numeric_limits<T>::max() / y)
          return false;
      } else {
        if (y < std::numeric_limits<T>::min() / x)
          return false;
      }
    } else {
      if (y > 0) {
        if (x < std::numeric_limits<T>::min() / y)
          return false;
      } else {
        if (y < std::numeric_limits<T>::max() / x)
          return false;
      }
    }
  }
  *result = x * y;
  return true;
}

template <typename T>
typename std::enable_if<std::numeric_limits<T>::is_integer &&
                            !std::numeric_limits<T>::is_signed &&
                            (sizeof(T) * 2 > sizeof(uintmax_t)),
                        bool>::type
CheckedMulImpl(T x, T y, T* result) {
  *result = x * y;
  return (y == 0 || x <= std::numeric_limits<T>::max() / y);
}

template <typename T, typename U, class Enable = void>
struct CheckedMulOp {};

template <typename T, typename U>
struct CheckedMulOp<
    T,
    U,
    typename std::enable_if<std::numeric_limits<T>::is_integer &&
                            std::numeric_limits<U>::is_integer>::type> {
  using result_type =
      typename ArithmeticPromotion<MAX_EXPONENT_PROMOTION, T, U>::type;
  template <typename V>
  static bool Do(T x, U y, V* result) {
#if USE_OVERFLOW_BUILTINS
#if defined(__clang__)
    // TODO(jschuh): Get the Clang runtime library issues sorted out so we can
    // support full-width, mixed-sign multiply builtins.
    // https://crbug.com/613003
    static const bool kUseMaxInt =
        sizeof(__typeof__(x * y)) < sizeof(intptr_t) ||
        (sizeof(__typeof__(x * y)) == sizeof(intptr_t) &&
         std::is_signed<T>::value == std::is_signed<U>::value);
#else
    static const bool kUseMaxInt = true;
#endif
    if (kUseMaxInt)
      return !__builtin_mul_overflow(x, y, result);
#endif
    using Promotion =
        typename ArithmeticPromotion<BIG_ENOUGH_PROMOTION, T, U>::type;
    Promotion presult;
    // Fail if either operand is out of range for the promoted type.
    // TODO(jschuh): This could be made to work for a broader range of values.
    bool is_valid = IsValueInRangeForNumericType<Promotion>(x) &&
                    IsValueInRangeForNumericType<Promotion>(y);

    if (IsIntegerArithmeticSafe<Promotion, T, U>::value) {
      presult = static_cast<Promotion>(x) * static_cast<Promotion>(y);
    } else {
      is_valid &= CheckedMulImpl(static_cast<Promotion>(x),
                                 static_cast<Promotion>(y), &presult);
    }
    *result = static_cast<V>(presult);
    return is_valid && IsValueInRangeForNumericType<V>(presult);
  }
};

// Avoid poluting the namespace once we're done with the macro.
#undef USE_OVERFLOW_BUILTINS

// Division just requires a check for a zero denominator or an invalid negation
// on signed min/-1.
template <typename T>
typename std::enable_if<std::numeric_limits<T>::is_integer, bool>::type
CheckedDivImpl(T x, T y, T* result) {
  if (y && (!std::numeric_limits<T>::is_signed ||
            x != std::numeric_limits<T>::min() || y != static_cast<T>(-1))) {
    *result = x / y;
    return true;
  }
  return false;
}

template <typename T, typename U, class Enable = void>
struct CheckedDivOp {};

template <typename T, typename U>
struct CheckedDivOp<
    T,
    U,
    typename std::enable_if<std::numeric_limits<T>::is_integer &&
                            std::numeric_limits<U>::is_integer>::type> {
  using result_type =
      typename ArithmeticPromotion<MAX_EXPONENT_PROMOTION, T, U>::type;
  template <typename V>
  static bool Do(T x, U y, V* result) {
    using Promotion =
        typename ArithmeticPromotion<BIG_ENOUGH_PROMOTION, T, U>::type;
    Promotion presult;
    // Fail if either operand is out of range for the promoted type.
    // TODO(jschuh): This could be made to work for a broader range of values.
    bool is_valid = IsValueInRangeForNumericType<Promotion>(x) &&
                    IsValueInRangeForNumericType<Promotion>(y);
    is_valid &= CheckedDivImpl(static_cast<Promotion>(x),
                               static_cast<Promotion>(y), &presult);
    *result = static_cast<V>(presult);
    return is_valid && IsValueInRangeForNumericType<V>(presult);
  }
};

template <typename T>
typename std::enable_if<std::numeric_limits<T>::is_integer &&
                            std::numeric_limits<T>::is_signed,
                        bool>::type
CheckedModImpl(T x, T y, T* result) {
  if (y > 0) {
    *result = static_cast<T>(x % y);
    return true;
  }
  return false;
}

template <typename T>
typename std::enable_if<std::numeric_limits<T>::is_integer &&
                            !std::numeric_limits<T>::is_signed,
                        bool>::type
CheckedModImpl(T x, T y, T* result) {
  if (y != 0) {
    *result = static_cast<T>(x % y);
    return true;
  }
  return false;
}

template <typename T, typename U, class Enable = void>
struct CheckedModOp {};

template <typename T, typename U>
struct CheckedModOp<
    T,
    U,
    typename std::enable_if<std::numeric_limits<T>::is_integer &&
                            std::numeric_limits<U>::is_integer>::type> {
  using result_type =
      typename ArithmeticPromotion<MAX_EXPONENT_PROMOTION, T, U>::type;
  template <typename V>
  static bool Do(T x, U y, V* result) {
    using Promotion =
        typename ArithmeticPromotion<BIG_ENOUGH_PROMOTION, T, U>::type;
    Promotion presult;
    bool is_valid = CheckedModImpl(static_cast<Promotion>(x),
                                   static_cast<Promotion>(y), &presult);
    *result = static_cast<V>(presult);
    return is_valid && IsValueInRangeForNumericType<V>(presult);
  }
};

template <typename T, typename U, class Enable = void>
struct CheckedLshOp {};

// Left shift. Shifts less than 0 or greater than or equal to the number
// of bits in the promoted type are undefined. Shifts of negative values
// are undefined. Otherwise it is defined when the result fits.
template <typename T, typename U>
struct CheckedLshOp<
    T,
    U,
    typename std::enable_if<std::numeric_limits<T>::is_integer &&
                            std::numeric_limits<U>::is_integer>::type> {
  using result_type = T;
  template <typename V>
  static bool Do(T x, U shift, V* result) {
    using ShiftType = typename UnsignedIntegerForSize<T>::type;
    static const ShiftType kBitWidth = CHAR_BIT * sizeof(T);
    const ShiftType real_shift = static_cast<ShiftType>(shift);
    // Signed shift is not legal on negative values.
    if (!IsValueNegative(x) && real_shift < kBitWidth) {
      // Just use a multiplication because it's easy.
      // TODO(jschuh): This could probably be made more efficient.
      if (!std::is_signed<T>::value || real_shift != kBitWidth - 1)
        return CheckedMulOp<T, T>::Do(x, static_cast<T>(1) << shift, result);
      return !x;  // Special case zero for a full width signed shift.
    }
    return false;
  }
};

template <typename T, typename U, class Enable = void>
struct CheckedRshOp {};

// Right shift. Shifts less than 0 or greater than or equal to the number
// of bits in the promoted type are undefined. Otherwise, it is always defined,
// but a right shift of a negative value is implementation-dependent.
template <typename T, typename U>
struct CheckedRshOp<
    T,
    U,
    typename std::enable_if<std::numeric_limits<T>::is_integer &&
                            std::numeric_limits<U>::is_integer>::type> {
  using result_type = T;
  template <typename V = result_type>
  static bool Do(T x, U shift, V* result) {
    // Use the type conversion push negative values out of range.
    using ShiftType = typename UnsignedIntegerForSize<T>::type;
    if (static_cast<ShiftType>(shift) < (CHAR_BIT * sizeof(T))) {
      T tmp = x >> shift;
      *result = static_cast<V>(tmp);
      return IsValueInRangeForNumericType<V>(tmp);
    }
    return false;
  }
};

template <typename T, typename U, class Enable = void>
struct CheckedAndOp {};

// For simplicity we support only unsigned integers.
template <typename T, typename U>
struct CheckedAndOp<T,
                    U,
                    typename std::enable_if<std::is_integral<T>::value &&
                                            std::is_integral<U>::value &&
                                            !std::is_signed<T>::value &&
                                            !std::is_signed<U>::value>::type> {
  using result_type =
      typename ArithmeticPromotion<MAX_EXPONENT_PROMOTION, T, U>::type;
  template <typename V = result_type>
  static bool Do(T x, U y, V* result) {
    result_type tmp = x & y;
    *result = static_cast<V>(tmp);
    return IsValueInRangeForNumericType<V>(tmp);
  }
};

template <typename T, typename U, class Enable = void>
struct CheckedOrOp {};

// For simplicity we support only unsigned integers.
template <typename T, typename U>
struct CheckedOrOp<T,
                   U,
                   typename std::enable_if<std::is_integral<T>::value &&
                                           std::is_integral<U>::value &&
                                           !std::is_signed<T>::value &&
                                           !std::is_signed<U>::value>::type> {
  using result_type =
      typename ArithmeticPromotion<MAX_EXPONENT_PROMOTION, T, U>::type;
  template <typename V = result_type>
  static bool Do(T x, U y, V* result) {
    result_type tmp = x | y;
    *result = static_cast<V>(tmp);
    return IsValueInRangeForNumericType<V>(tmp);
  }
};

template <typename T, typename U, class Enable = void>
struct CheckedXorOp {};

// For simplicity we support only unsigned integers.
template <typename T, typename U>
struct CheckedXorOp<T,
                    U,
                    typename std::enable_if<std::is_integral<T>::value &&
                                            std::is_integral<U>::value &&
                                            !std::is_signed<T>::value &&
                                            !std::is_signed<U>::value>::type> {
  using result_type =
      typename ArithmeticPromotion<MAX_EXPONENT_PROMOTION, T, U>::type;
  template <typename V = result_type>
  static bool Do(T x, U y, V* result) {
    result_type tmp = x ^ y;
    *result = static_cast<V>(tmp);
    return IsValueInRangeForNumericType<V>(tmp);
  }
};

template <typename T>
typename std::enable_if<std::numeric_limits<T>::is_integer &&
                            std::numeric_limits<T>::is_signed,
                        bool>::type
CheckedNeg(T value, T* result) {
  // The negation of signed min is min, so catch that one.
  if (value != std::numeric_limits<T>::min()) {
    *result = static_cast<T>(-value);
    return true;
  }
  return false;
}

template <typename T>
typename std::enable_if<std::numeric_limits<T>::is_integer &&
                            !std::numeric_limits<T>::is_signed,
                        bool>::type
CheckedNeg(T value, T* result) {
  if (!value) {  // The only legal unsigned negation is zero.
    *result = static_cast<T>(0);
    return true;
  }
  return false;
}

template <typename T>
typename std::enable_if<std::numeric_limits<T>::is_integer &&
                            !std::numeric_limits<T>::is_signed,
                        bool>::type
CheckedInv(T value, T* result) {
  *result = ~value;
  return true;
}

template <typename T>
typename std::enable_if<std::numeric_limits<T>::is_integer &&
                            std::numeric_limits<T>::is_signed,
                        bool>::type
CheckedAbs(T value, T* result) {
  if (value != std::numeric_limits<T>::min()) {
    *result = std::abs(value);
    return true;
  }
  return false;
}

template <typename T>
typename std::enable_if<std::numeric_limits<T>::is_integer &&
                            !std::numeric_limits<T>::is_signed,
                        bool>::type
CheckedAbs(T value, T* result) {
  // T is unsigned, so |value| must already be positive.
  *result = value;
  return true;
}

template <typename T>
constexpr
    typename std::enable_if<std::numeric_limits<T>::is_integer &&
                                std::numeric_limits<T>::is_signed,
                            typename UnsignedIntegerForSize<T>::type>::type
    SafeUnsignedAbs(T value) {
  typedef typename UnsignedIntegerForSize<T>::type UnsignedT;
  return value == std::numeric_limits<T>::min()
             ? static_cast<UnsignedT>(std::numeric_limits<T>::max()) + 1
             : static_cast<UnsignedT>(std::abs(value));
}

template <typename T>
constexpr typename std::enable_if<std::numeric_limits<T>::is_integer &&
                                      !std::numeric_limits<T>::is_signed,
                                  T>::type
SafeUnsignedAbs(T value) {
  // T is unsigned, so |value| must already be positive.
  return static_cast<T>(value);
}

// This is just boilerplate that wraps the standard floating point arithmetic.
// A macro isn't the nicest solution, but it beats rewriting these repeatedly.
#define BASE_FLOAT_ARITHMETIC_OPS(NAME, OP)                                 \
  template <typename T, typename U>                                         \
  struct Checked##NAME##Op<                                                 \
      T, U,                                                                 \
      typename std::enable_if<std::numeric_limits<T>::is_iec559 ||          \
                              std::numeric_limits<U>::is_iec559>::type> {   \
    using result_type =                                                     \
        typename ArithmeticPromotion<MAX_EXPONENT_PROMOTION, T, U>::type;   \
    template <typename V>                                                   \
    static bool Do(T x, U y, V* result) {                                   \
      using Promotion =                                                     \
          typename ArithmeticPromotion<MAX_EXPONENT_PROMOTION, T, U>::type; \
      Promotion presult = x OP y;                                           \
      *result = static_cast<V>(presult);                                    \
      return IsValueInRangeForNumericType<V>(presult);                      \
    }                                                                       \
  };

BASE_FLOAT_ARITHMETIC_OPS(Add, +)
BASE_FLOAT_ARITHMETIC_OPS(Sub, -)
BASE_FLOAT_ARITHMETIC_OPS(Mul, *)
BASE_FLOAT_ARITHMETIC_OPS(Div, /)

#undef BASE_FLOAT_ARITHMETIC_OPS

template <typename T>
typename std::enable_if<std::numeric_limits<T>::is_iec559, bool>::type
CheckedNeg(T value, T* result) {
  *result = static_cast<T>(-value);
  return true;
}

template <typename T>
typename std::enable_if<std::numeric_limits<T>::is_iec559, bool>::type
CheckedAbs(T value, T* result) {
  *result = static_cast<T>(std::abs(value));
  return true;
}

// Floats carry around their validity state with them, but integers do not. So,
// we wrap the underlying value in a specialization in order to hide that detail
// and expose an interface via accessors.
enum NumericRepresentation {
  NUMERIC_INTEGER,
  NUMERIC_FLOATING,
  NUMERIC_UNKNOWN
};

template <typename NumericType>
struct GetNumericRepresentation {
  static const NumericRepresentation value =
      std::numeric_limits<NumericType>::is_integer
          ? NUMERIC_INTEGER
          : (std::numeric_limits<NumericType>::is_iec559 ? NUMERIC_FLOATING
                                                         : NUMERIC_UNKNOWN);
};

template <typename T, NumericRepresentation type =
                          GetNumericRepresentation<T>::value>
class CheckedNumericState {};

// Integrals require quite a bit of additional housekeeping to manage state.
template <typename T>
class CheckedNumericState<T, NUMERIC_INTEGER> {
 private:
  T value_;
  bool is_valid_;

 public:
  template <typename Src, NumericRepresentation type>
  friend class CheckedNumericState;

  constexpr CheckedNumericState() : value_(0), is_valid_(true) {}

  template <typename Src>
  constexpr CheckedNumericState(Src value, bool is_valid)
      : value_(static_cast<T>(value)),
        is_valid_(is_valid && IsValueInRangeForNumericType<T>(value)) {
    static_assert(std::numeric_limits<Src>::is_specialized,
                  "Argument must be numeric.");
  }

  // Copy constructor.
  template <typename Src>
  constexpr CheckedNumericState(const CheckedNumericState<Src>& rhs)
      : value_(static_cast<T>(rhs.value())), is_valid_(rhs.IsValid()) {}

  template <typename Src>
  constexpr explicit CheckedNumericState(
      Src value,
      typename std::enable_if<std::numeric_limits<Src>::is_specialized,
                              int>::type = 0)
      : value_(static_cast<T>(value)),
        is_valid_(IsValueInRangeForNumericType<T>(value)) {}

  constexpr bool is_valid() const { return is_valid_; }
  constexpr T value() const { return value_; }
};

// Floating points maintain their own validity, but need translation wrappers.
template <typename T>
class CheckedNumericState<T, NUMERIC_FLOATING> {
 private:
  T value_;

 public:
  template <typename Src, NumericRepresentation type>
  friend class CheckedNumericState;

  constexpr CheckedNumericState() : value_(0.0) {}

  template <typename Src>
  constexpr CheckedNumericState(
      Src value,
      bool is_valid,
      typename std::enable_if<std::numeric_limits<Src>::is_integer, int>::type =
          0)
      : value_((is_valid && IsValueInRangeForNumericType<T>(value))
                   ? static_cast<T>(value)
                   : std::numeric_limits<T>::quiet_NaN()) {}

  template <typename Src>
  constexpr explicit CheckedNumericState(
      Src value,
      typename std::enable_if<std::numeric_limits<Src>::is_specialized,
                              int>::type = 0)
      : value_(static_cast<T>(value)) {}

  // Copy constructor.
  template <typename Src>
  constexpr CheckedNumericState(const CheckedNumericState<Src>& rhs)
      : value_(static_cast<T>(rhs.value())) {}

  constexpr bool is_valid() const { return std::isfinite(value_); }
  constexpr T value() const { return value_; }
};

// The following are helper templates used in the CheckedNumeric class.
template <typename T>
class CheckedNumeric;

// Used to treat CheckedNumeric and arithmetic underlying types the same.
template <typename T>
struct UnderlyingType {
  using type = T;
  static const bool is_numeric = std::is_arithmetic<T>::value;
  static const bool is_checked = false;
};

template <typename T>
struct UnderlyingType<CheckedNumeric<T>> {
  using type = T;
  static const bool is_numeric = true;
  static const bool is_checked = true;
};

template <typename L, typename R>
struct IsCheckedOp {
  static const bool value =
      UnderlyingType<L>::is_numeric && UnderlyingType<R>::is_numeric &&
      (UnderlyingType<L>::is_checked || UnderlyingType<R>::is_checked);
};

template <template <typename, typename, typename> class M,
          typename L,
          typename R>
struct MathWrapper {
  using math = M<typename UnderlyingType<L>::type,
                 typename UnderlyingType<R>::type,
                 void>;
  using type = typename math::result_type;
};

}  // namespace internal
}  // namespace base

#endif  // BASE_NUMERICS_SAFE_MATH_IMPL_H_
