// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_ZUCCHINI_BUFFER_VIEW_H_
#define CHROME_INSTALLER_ZUCCHINI_BUFFER_VIEW_H_

#include <stddef.h>
#include <stdint.h>

#include <type_traits>

#include "base/logging.h"
#include "chrome/installer/zucchini/algorithm.h"

namespace zucchini {

// Describes a region within a buffer, with starting offset and size.
struct BufferRegion {
  // The region data are stored as |offset| and |size|, but often it is useful
  // to represent it as an interval [lo(), hi()) = [offset, offset + size).
  size_t lo() const { return offset; }
  size_t hi() const { return offset + size; }

  // Returns whether the Region fits in |[0, container_size)|. Special case:
  // a size-0 region starting at |container_size| does not fit.
  bool FitsIn(size_t container_size) const {
    return offset < container_size && container_size - offset >= size;
  }

  // Returns |v| clipped to the inclusive range |[lo(), hi()]|.
  size_t InclusiveClamp(size_t v) const {
    return zucchini::InclusiveClamp(v, lo(), hi());
  }
  friend bool operator==(const BufferRegion& a, const BufferRegion& b) {
    return a.offset == b.offset && a.size == b.size;
  }
  friend bool operator!=(const BufferRegion& a, const BufferRegion& b) {
    return !(a == b);
  }

  // Region data use size_t to match BufferViewBase::size_type, to make it
  // convenient to index into buffer view.
  size_t offset;
  size_t size;
};

namespace internal {

// BufferViewBase should not be used directly; it is an implementation used for
// both BufferView and MutableBufferView.
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
    DCHECK_GE(last, first);
    BufferViewBase ret;
    ret.first_ = first;
    ret.last_ = last;
    return ret;
  }

  BufferViewBase() = default;

  BufferViewBase(iterator first, size_type size)
      : first_(first), last_(first_ + size) {
    DCHECK_GE(last_, first_);
  }

  template <class U>
  BufferViewBase(const BufferViewBase<U>& that)
      : first_(that.begin()), last_(that.end()) {}

  template <class U>
  BufferViewBase(BufferViewBase<U>&& that)
      : first_(that.begin()), last_(that.end()) {}

  BufferViewBase(const BufferViewBase&) = default;
  BufferViewBase& operator=(const BufferViewBase&) = default;

  // Iterators

  iterator begin() const { return first_; }
  iterator end() const { return last_; }
  const_iterator cbegin() const { return begin(); }
  const_iterator cend() const { return end(); }

  // Capacity

  bool empty() const { return first_ == last_; }
  size_type size() const { return last_ - first_; }

  // Returns whether the buffer is large enough to cover |region|.
  bool covers(const BufferRegion& region) const {
    return region.FitsIn(size());
  }

  // Element access

  // Returns the raw value at specified location |pos|.
  // If |pos| is not within the range of the buffer, the process is terminated.
  reference operator[](size_type pos) const {
    CHECK_LT(pos, size());
    return first_[pos];
  }

  // Returns a sub-buffer described by |region|.
  BufferViewBase operator[](BufferRegion region) const {
    DCHECK_LE(region.offset, size());
    DCHECK_LE(region.size, size() - region.offset);
    return {begin() + region.offset, region.size};
  }

  template <class U>
  const U& read(size_type pos) const {
    CHECK_LE(pos + sizeof(U), size());
    return *reinterpret_cast<const U*>(begin() + pos);
  }

  template <class U>
  void write(size_type pos, const U& value) {
    CHECK_LE(pos + sizeof(U), size());
    *reinterpret_cast<U*>(begin() + pos) = value;
  }

  template <class U>
  bool can_access(size_type pos) const {
    return pos < size() && size() - pos >= sizeof(U);
  }

  // Returns a BufferRegion describing the full view.
  BufferRegion region() const { return BufferRegion{0, size()}; }

  // Modifiers

  void shrink(size_type new_size) {
    DCHECK_LE(first_ + new_size, last_);
    last_ = first_ + new_size;
  }

  // Moves the start of the view forward by n bytes.
  void remove_prefix(size_type n) {
    DCHECK_LE(n, size());
    first_ += n;
  }

  // Moves the start of the view to |pos| which is in range [begin(), end()).
  void seek(iterator pos) {
    DCHECK_GE(pos, begin());
    DCHECK_LE(pos, end());
    first_ = pos;
  }

 private:
  iterator first_ = nullptr;
  iterator last_ = nullptr;
};

}  // namespace internal

// Classes to encapsulate a contiguous sequence of raw data, without owning the
// encapsulated memory regions. These are intended to be used as value types.

using ConstBufferView = internal::BufferViewBase<const uint8_t>;
using MutableBufferView = internal::BufferViewBase<uint8_t>;

}  // namespace zucchini

#endif  // CHROME_INSTALLER_ZUCCHINI_BUFFER_VIEW_H_
