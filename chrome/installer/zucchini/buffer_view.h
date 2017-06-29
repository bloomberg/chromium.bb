// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_ZUCCHINI_BUFFER_VIEW_H_
#define CHROME_INSTALLER_ZUCCHINI_BUFFER_VIEW_H_

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "base/logging.h"

namespace zucchini {
namespace internal {

// A class that encapsulates a contiguous sequence of raw data.
// It does not own the memory region it encapsulates.
// BufferViewBase should not be used directly; it is an implementation
// used for both BufferView and MutableBufferView.
template <class T>
class BufferViewBase {
 public:
  using value_type = T;
  using reference = T&;
  using pointer = T*;
  using iterator = T*;
  using const_iterator = typename std::add_const<T>::type*;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  static BufferViewBase FromRange(iterator first, iterator last) {
    DCHECK(last >= first);
    BufferViewBase ret;
    ret.first_ = first;
    ret.last_ = last;
    return ret;
  }

  BufferViewBase() = default;

  BufferViewBase(iterator first, size_type size)
      : first_(first), last_(first_ + size) {}
  BufferViewBase(const BufferViewBase&) = default;
  BufferViewBase& operator=(const BufferViewBase&) = default;

  // Iterators

  iterator begin() const { return first_; }
  iterator end() const { return last_; }
  const_iterator cbegin() const { return begin(); }
  const_iterator cend() const { return end(); }

  // Element access

  // Returns the raw value at specified location pos.
  reference operator[](size_type pos) const {
    DCHECK(first_ + pos < last_);
    return first_[pos];
  }

  // Capacity

  bool empty() const { return first_ == last_; }
  size_type size() const { return last_ - first_; }

  // Modifiers

  void shrink(size_type new_size) {
    DCHECK(first_ + new_size <= last_);
    last_ = first_ + new_size;
  }

 private:
  iterator first_ = nullptr;
  iterator last_ = nullptr;
};

}  // namespace internal

// A class that encapsulates a constant contiguous sequence of raw data.
// It does not own the memory region it refers to.
using BufferView = internal::BufferViewBase<const uint8_t>;

// A class that encapsulates a mutable contiguous sequence of raw data.
// It does not own the memory region it refers to.
using MutableBufferView = internal::BufferViewBase<uint8_t>;

}  // namespace zucchini

#endif  // CHROME_INSTALLER_ZUCCHINI_BUFFER_VIEW_H_
