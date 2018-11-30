// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_CARRAY_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_CARRAY_H_

#include <cstddef>

#include "base/containers/span.h"
#include "mojo/public/cpp/bindings/array_traits.h"

namespace mojo {

template <typename T, size_t Extent>
struct ArrayTraits<base::span<T, Extent>> {
  using Element = T;

  // There is no concept of a null span, as it is indistinguishable from the
  // empty span.
  static bool IsNull(const base::span<T>& input) { return false; }

  static size_t GetSize(const base::span<T>& input) { return input.size(); }

  static T* GetData(base::span<T>& input) { return input.data(); }

  static const T* GetData(const base::span<T>& input) { return input.data(); }

  static T& GetAt(base::span<T>& input, size_t index) {
    return input.data()[index];
  }

  static const T& GetAt(const base::span<T>& input, size_t index) {
    return input.data()[index];
  }

  static bool Resize(base::span<T>& input, size_t size) {
    if (size > input.size())
      return false;
    input = input.subspan(0, size);
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_CARRAY_H_
