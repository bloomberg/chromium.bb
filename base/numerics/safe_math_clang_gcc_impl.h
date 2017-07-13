// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NUMERICS_SAFE_MATH_CLANG_GCC_IMPL_H_
#define BASE_NUMERICS_SAFE_MATH_CLANG_GCC_IMPL_H_

#include <cassert>
#include <limits>
#include <type_traits>

#include "base/numerics/safe_conversions.h"

namespace base {
namespace internal {

template <typename T, typename U>
struct CheckedAddFastOp {
  static const bool is_supported = true;
  template <typename V>
  __attribute__((always_inline)) static constexpr bool Do(T x, U y, V* result) {
    return !__builtin_add_overflow(x, y, result);
  }
};

template <typename T, typename U>
struct CheckedSubFastOp {
  static const bool is_supported = true;
  template <typename V>
  __attribute__((always_inline)) static constexpr bool Do(T x, U y, V* result) {
    return !__builtin_sub_overflow(x, y, result);
  }
};

template <typename T, typename U>
struct CheckedMulFastOp {
#if defined(__clang__)
  // TODO(jschuh): Get the Clang runtime library issues sorted out so we can
  // support full-width, mixed-sign multiply builtins.
  // https://crbug.com/613003
  // We can support intptr_t, uintptr_t, or a smaller common type.
  static const bool is_supported =
      (IsTypeInRangeForNumericType<intptr_t, T>::value &&
       IsTypeInRangeForNumericType<intptr_t, U>::value) ||
      (IsTypeInRangeForNumericType<uintptr_t, T>::value &&
       IsTypeInRangeForNumericType<uintptr_t, U>::value);
#else
  static const bool is_supported = true;
#endif
  template <typename V>
  __attribute__((always_inline)) static constexpr bool Do(T x, U y, V* result) {
    return !__builtin_mul_overflow(x, y, result);
  }
};

// This is a placeholder until the ARM implementation lands in the next CL.
template <typename T, typename U>
struct ClampedAddFastOp {
  static const bool is_supported = false;
  template <typename V>
  static V Do(T, U) {
    assert(false);
    return 0;
  }
};

// This is a placeholder until the ARM implementation lands in the next CL.
template <typename T, typename U>
struct ClampedSubFastOp {
  static const bool is_supported = false;
  template <typename V>
  static V Do(T, U) {
    assert(false);
    return 0;
  }
};

}  // namespace internal
}  // namespace base

#endif  // BASE_NUMERICS_SAFE_MATH_CLANG_GCC_IMPL_H_
