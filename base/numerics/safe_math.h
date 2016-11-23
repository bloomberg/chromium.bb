// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NUMERICS_SAFE_MATH_H_
#define BASE_NUMERICS_SAFE_MATH_H_

#include <stddef.h>

#include <limits>
#include <type_traits>

#include "base/numerics/safe_math_impl.h"

namespace base {

namespace internal {

// CheckedNumeric implements all the logic and operators for detecting integer
// boundary conditions such as overflow, underflow, and invalid conversions.
// The CheckedNumeric type implicitly converts from floating point and integer
// data types, and contains overloads for basic arithmetic operations (i.e.: +,
// -, *, /, %).
//
// The following methods convert from CheckedNumeric to standard numeric values:
// IsValid() - Returns true if the underlying numeric value is valid (i.e. has
//             has not wrapped and is not the result of an invalid conversion).
// ValueOrDie() - Returns the underlying value. If the state is not valid this
//                call will crash on a CHECK.
// ValueOrDefault() - Returns the current value, or the supplied default if the
//                    state is not valid.
// ValueFloating() - Returns the underlying floating point value (valid only
//                   only for floating point CheckedNumeric types).
//
// Bitwise operations are explicitly not supported, because correct
// handling of some cases (e.g. sign manipulation) is ambiguous. Comparison
// operations are explicitly not supported because they could result in a crash
// on a CHECK condition. You should use patterns like the following for these
// operations:
// Bitwise operation:
//     CheckedNumeric<int> checked_int = untrusted_input_value;
//     int x = checked_int.ValueOrDefault(0) | kFlagValues;
// Comparison:
//   CheckedNumeric<size_t> checked_size = untrusted_input_value;
//   checked_size += HEADER LENGTH;
//   if (checked_size.IsValid() && checked_size.ValueOrDie() < buffer_size)
//     Do stuff...

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

template <typename T>
class CheckedNumeric {
  static_assert(std::is_arithmetic<T>::value,
                "CheckedNumeric<T>: T must be a numeric type.");

 public:
  typedef T type;

  constexpr CheckedNumeric() {}

  // Copy constructor.
  template <typename Src>
  constexpr CheckedNumeric(const CheckedNumeric<Src>& rhs)
      : state_(rhs.state_.value(), rhs.IsValid()) {}

  template <typename Src>
  friend class CheckedNumeric;

  // This is not an explicit constructor because we implicitly upgrade regular
  // numerics to CheckedNumerics to make them easier to use.
  template <typename Src>
  constexpr CheckedNumeric(Src value)  // NOLINT(runtime/explicit)
      : state_(value) {
    static_assert(std::numeric_limits<Src>::is_specialized,
                  "Argument must be numeric.");
  }

  // This is not an explicit constructor because we want a seamless conversion
  // from StrictNumeric types.
  template <typename Src>
  constexpr CheckedNumeric(
      StrictNumeric<Src> value)  // NOLINT(runtime/explicit)
      : state_(static_cast<Src>(value)) {}

  // IsValid() is the public API to test if a CheckedNumeric is currently valid.
  constexpr bool IsValid() const { return state_.is_valid(); }

  // ValueOrDie() The primary accessor for the underlying value. If the current
  // state is not valid it will CHECK and crash.
  constexpr T ValueOrDie() const {
    return IsValid() ? state_.value() : CheckOnFailure::HandleFailure<T>();
  }

  // ValueOrDefault(T default_value) A convenience method that returns the
  // current value if the state is valid, and the supplied default_value for
  // any other state.
  constexpr T ValueOrDefault(T default_value) const {
    return IsValid() ? state_.value() : default_value;
  }

  // ValueFloating() - Since floating point values include their validity state,
  // we provide an easy method for extracting them directly, without a risk of
  // crashing on a CHECK.
  constexpr T ValueFloating() const {
    static_assert(std::numeric_limits<T>::is_iec559, "Argument must be float.");
    return state_.value();
  }

  // This friend method is available solely for providing more detailed logging
  // in the the tests. Do not implement it in production code, because the
  // underlying values may change at any time.
  template <typename U>
  friend U GetNumericValueForTest(const CheckedNumeric<U>& src);

  // Prototypes for the supported arithmetic operator overloads.
  template <typename Src>
  CheckedNumeric& operator+=(const Src rhs);
  template <typename Src>
  CheckedNumeric& operator-=(const Src rhs);
  template <typename Src>
  CheckedNumeric& operator*=(const Src rhs);
  template <typename Src>
  CheckedNumeric& operator/=(const Src rhs);
  template <typename Src>
  CheckedNumeric& operator%=(const Src rhs);
  template <typename Src>
  CheckedNumeric& operator<<=(const Src rhs);
  template <typename Src>
  CheckedNumeric& operator>>=(const Src rhs);

  CheckedNumeric operator-() const {
    // Negation is always valid for floating point.
    T value = 0;
    bool is_valid = (std::numeric_limits<T>::is_iec559 || IsValid()) &&
                    CheckedNeg(state_.value(), &value);
    return CheckedNumeric<T>(value, is_valid);
  }

  CheckedNumeric Abs() const {
    // Absolute value is always valid for floating point.
    T value = 0;
    bool is_valid = (std::numeric_limits<T>::is_iec559 || IsValid()) &&
                    CheckedAbs(state_.value(), &value);
    return CheckedNumeric<T>(value, is_valid);
  }

  // This function is available only for integral types. It returns an unsigned
  // integer of the same width as the source type, containing the absolute value
  // of the source, and properly handling signed min.
  constexpr CheckedNumeric<typename UnsignedOrFloatForSize<T>::type>
  UnsignedAbs() const {
    return CheckedNumeric<typename UnsignedOrFloatForSize<T>::type>(
        SafeUnsignedAbs(state_.value()), state_.is_valid());
  }

  CheckedNumeric& operator++() {
    *this += 1;
    return *this;
  }

  CheckedNumeric operator++(int) {
    CheckedNumeric value = *this;
    *this += 1;
    return value;
  }

  CheckedNumeric& operator--() {
    *this -= 1;
    return *this;
  }

  CheckedNumeric operator--(int) {
    CheckedNumeric value = *this;
    *this -= 1;
    return value;
  }

  // These perform the actual math operations on the CheckedNumerics.
  // Binary arithmetic operations.
  template <template <typename, typename, typename> class M,
            typename L,
            typename R>
  static CheckedNumeric MathOp(const L& lhs, const R& rhs) {
    using Math = M<typename UnderlyingType<L>::type,
                   typename UnderlyingType<R>::type, void>;
    T result = 0;
    bool is_valid =
        Wrapper<L>::is_valid(lhs) && Wrapper<R>::is_valid(rhs) &&
        Math::Op(Wrapper<L>::value(lhs), Wrapper<R>::value(rhs), &result);
    return CheckedNumeric<T>(result, is_valid);
  };

  // Assignment arithmetic operations.
  template <template <typename, typename, typename> class M, typename R>
  CheckedNumeric& MathOp(const R& rhs) {
    using Math = M<T, typename UnderlyingType<R>::type, void>;
    T result = 0;  // Using T as the destination saves a range check.
    bool is_valid = state_.is_valid() && Wrapper<R>::is_valid(rhs) &&
                    Math::Op(state_.value(), Wrapper<R>::value(rhs), &result);
    *this = CheckedNumeric<T>(result, is_valid);
    return *this;
  };

 private:
  CheckedNumericState<T> state_;

  template <typename Src>
  constexpr CheckedNumeric(Src value, bool is_valid)
      : state_(value, is_valid) {}

  // These wrappers allow us to handle state the same way for both
  // CheckedNumeric and POD arithmetic types.
  template <typename Src>
  struct Wrapper {
    static constexpr bool is_valid(Src) { return true; }
    static constexpr Src value(Src value) { return value; }
  };

  template <typename Src>
  struct Wrapper<CheckedNumeric<Src>> {
    static constexpr bool is_valid(const CheckedNumeric<Src>& v) {
      return v.IsValid();
    }
    static constexpr Src value(const CheckedNumeric<Src>& v) {
      return v.state_.value();
    }
  };
};

// This is just boilerplate for the standard arithmetic operator overloads.
// A macro isn't the nicest solution, but it beats rewriting these repeatedly.
#define BASE_NUMERIC_ARITHMETIC_OPERATORS(NAME, OP, COMPOUND_OP)              \
  /* Binary arithmetic operator for all CheckedNumeric operations. */         \
  template <typename L, typename R,                                           \
            typename std::enable_if<UnderlyingType<L>::is_numeric &&          \
                                    UnderlyingType<R>::is_numeric &&          \
                                    (UnderlyingType<L>::is_checked ||         \
                                     UnderlyingType<R>::is_checked)>::type* = \
                nullptr>                                                      \
  CheckedNumeric<                                                             \
      typename Checked##NAME<typename UnderlyingType<L>::type,                \
                             typename UnderlyingType<R>::type>::result_type>  \
  operator OP(const L lhs, const R rhs) {                                     \
    return decltype(lhs OP rhs)::template MathOp<Checked##NAME>(lhs, rhs);    \
  }                                                                           \
  /* Assignment arithmetic operator implementation from CheckedNumeric. */    \
  template <typename L>                                                       \
  template <typename R>                                                       \
  CheckedNumeric<L>& CheckedNumeric<L>::operator COMPOUND_OP(const R rhs) {   \
    return MathOp<Checked##NAME>(rhs);                                        \
  }

BASE_NUMERIC_ARITHMETIC_OPERATORS(Add, +, +=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Sub, -, -=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Mul, *, *=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Div, /, /=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Mod, %, %=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(LeftShift, <<, <<=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(RightShift, >>, >>=)

#undef BASE_NUMERIC_ARITHMETIC_OPERATORS

}  // namespace internal

using internal::CheckedNumeric;

}  // namespace base

#endif  // BASE_NUMERICS_SAFE_MATH_H_
