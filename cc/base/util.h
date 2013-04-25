// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_UTIL_H_
#define CC_BASE_UTIL_H_

#include <limits>

#include "base/basictypes.h"

namespace cc {

template <typename T> T RoundUp(T n, T mul) {
  COMPILE_ASSERT(std::numeric_limits<T>::is_integer, type_must_be_integral);
  return (n > 0) ? ((n + mul - 1) / mul) * mul
                 : (n / mul) * mul;
}

template <typename T> T RoundDown(T n, T mul) {
  COMPILE_ASSERT(std::numeric_limits<T>::is_integer, type_must_be_integral);
  return (n > 0) ? (n / mul) * mul
                 : (n == 0) ? 0
                 : ((n - mul + 1) / mul) * mul;
}

}  // namespace cc

#endif  // CC_BASE_UTIL_H_
