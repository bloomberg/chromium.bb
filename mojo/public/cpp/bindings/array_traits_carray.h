// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_CARRAY_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_CARRAY_H_

#include <cstddef>

#include "mojo/public/cpp/bindings/array_traits.h"

namespace mojo {

template <typename T>
class CArray {
 public:
  constexpr CArray() noexcept : size_(0), data_(nullptr) {}
  constexpr CArray(T* data, size_t size) noexcept : size_(size), data_(data) {}
  template <size_t N>
  constexpr CArray(T (&array)[N]) noexcept : size_(N), data_(array) {}

  constexpr size_t size() const noexcept { return size_; }
  constexpr T* data() const noexcept { return data_; }

  constexpr CArray subspan(size_t pos, size_t count) const {
    // Note: ideally this would DCHECK, but it requires fairly horrible
    // contortions.
    return CArray(data_ + pos, count);
  }

 private:
  size_t size_;
  T* data_;
};

// TODO(dcheng): Not sure if this is needed. Maybe code should just use
// CArray<const T> rather than ConstCArray<T>?
template <typename T>
class ConstCArray {
 public:
  constexpr ConstCArray() noexcept : size_(0), data_(nullptr) {}
  constexpr ConstCArray(const T* data, size_t size) noexcept
      : size_(size), data_(data) {}
  template <size_t N>
  constexpr ConstCArray(const T (&array)[N]) noexcept
      : size_(N), data_(array) {}

  constexpr size_t size() const noexcept { return size_; }
  constexpr const T* data() const noexcept { return data_; }

  constexpr ConstCArray subspan(size_t pos, size_t count) const {
    // Note: ideally this would DCHECK, but it requires fairly horrible
    // contortions.
    return ConstCArray(data_ + pos, count);
  }

 private:
  size_t size_;
  const T* data_;
};

template <typename T>
struct ArrayTraits<CArray<T>> {
  using Element = T;

  static bool IsNull(const CArray<T>& input) { return !input.data(); }

  static void SetToNull(CArray<T>* output) { *output = CArray<T>(); }

  static size_t GetSize(const CArray<T>& input) { return input.size(); }

  static T* GetData(CArray<T>& input) { return input.data(); }

  static const T* GetData(const CArray<T>& input) { return input.data(); }

  static T& GetAt(CArray<T>& input, size_t index) {
    return input.data()[index];
  }

  static const T& GetAt(const CArray<T>& input, size_t index) {
    return input.data()[index];
  }

  static bool Resize(CArray<T>& input, size_t size) {
    if (size > input.size())
      return false;
    input = input.subspan(0, size);
    return true;
  }
};

template <typename T>
struct ArrayTraits<ConstCArray<T>> {
  using Element = T;

  static bool IsNull(const ConstCArray<T>& input) { return !input.data(); }

  static void SetToNull(ConstCArray<T>* output) { *output = ConstCArray<T>(); }

  static size_t GetSize(const ConstCArray<T>& input) { return input.size(); }

  static const T* GetData(const ConstCArray<T>& input) { return input.data(); }

  static const T& GetAt(const ConstCArray<T>& input, size_t index) {
    return input.data()[index];
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_CARRAY_H_
