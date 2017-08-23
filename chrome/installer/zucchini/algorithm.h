// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_ZUCCHINI_ALGORITHM_H_
#define CHROME_INSTALLER_ZUCCHINI_ALGORITHM_H_

#include <type_traits>

#include <stddef.h>

// Collection of simple utilities used in for low-level computation.

namespace zucchini {

// Safely determines whether [begin, begin + size) is in [0, bound). Note that
// the special case [bound, bound) is not considered to be in [0, bound).
template <typename T>
bool RangeIsBounded(T begin, T size, size_t bound) {
  static_assert(std::is_unsigned<T>::value, "Value type must be unsigned.");
  return begin < bound && size <= bound - begin;
}

}  // namespace zucchini

#endif  // CHROME_INSTALLER_ZUCCHINI_ALGORITHM_H_
