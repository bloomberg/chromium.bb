// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_STL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_STL_H_

#include <vector>

#include "mojo/public/cpp/bindings/array_traits.h"

namespace mojo {

template <typename T>
struct ArrayTraits<std::vector<T>> {
  using Element = T;

  static bool IsNull(const std::vector<T>& input) {
    // std::vector<> is always converted to non-null mojom array.
    return false;
  }

  static void SetToNull(std::vector<T>* output) {
    // std::vector<> doesn't support null state. Set it to empty instead.
    output->clear();
  }

  static size_t GetSize(const std::vector<T>& input) { return input.size(); }

  static T* GetData(std::vector<T>& input) { return input.data(); }

  static const T* GetData(const std::vector<T>& input) { return input.data(); }

  static typename std::vector<T>::reference GetAt(std::vector<T>& input,
                                                  size_t index) {
    return input[index];
  }

  static typename std::vector<T>::const_reference GetAt(
      const std::vector<T>& input,
      size_t index) {
    return input[index];
  }

  static bool Resize(std::vector<T>& input, size_t size) {
    if (input.size() != size) {
      // This is a hack to make compilers for Mac and Android happy. They
      // currently don't allow resizing types like
      // std::vector<std::vector<MoveOnlyType>>.
      // Because the deserialization code doesn't care about the original
      // contents of |input|, we discard them directly.
      std::vector<T> temp(size);
      input.swap(temp);
    }

    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_STL_H_
