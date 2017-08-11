// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SPAN_H_
#define BASE_SPAN_H_

#include <stddef.h>

namespace base {

// A Span represents an array of elements of type T. It consists of a pointer to
// memory with an associated size. A Span does not own the underlying memory, so
// care must be taken to ensure that a Span does not outlive the backing store.
// Spans should be passed by value.
//
// Span is somewhat analogous to StringPiece, but with arbitrary element types,
// allowing mutation if T is non-const.
//
// TODO(https://crbug.com/754077): Document differences from the working group
// proposal: http://open-std.org/JTC1/SC22/WG21/docs/papers/2016/p0122r1.pdf.
// TODO(https://crbug.com/754077): Implement more Span support, such as
// initialization from containers and iterators and document why this is useful
// (greater safety since no need to manually pass in data + size)
template <typename T>
class Span {
 public:
  constexpr Span() noexcept : data_(nullptr), size_(0) {}
  constexpr Span(T* data, size_t size) noexcept : data_(data), size_(size) {}
  template <size_t N>
  constexpr Span(T (&array)[N]) noexcept : data_(array), size_(N) {}

  constexpr T* data() const noexcept { return data_; }
  constexpr size_t size() const noexcept { return size_; }

  constexpr Span subspan(size_t pos, size_t count) const {
    // Note: ideally this would DCHECK, but it requires fairly horrible
    // contortions.
    return Span(data_ + pos, count);
  }

 private:
  T* data_;
  size_t size_;
};

}  // namespace base

#endif  // BASE_SPAN_H_
