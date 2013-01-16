// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RING_BUFFER_H_
#define CC_RING_BUFFER_H_

#include "base/logging.h"

namespace cc {

template<typename T, size_t size>
class RingBuffer {
 public:
  explicit RingBuffer()
    : current_index_(0) {
  }

  size_t BufferSize() const {
    return size;
  }

  size_t CurrentIndex() const {
    return current_index_;
  }

  // tests if a value was saved to this index
  bool IsFilledIndex(size_t n) const {
    return BufferIndex(n) < current_index_;
  }

  // n = 0 returns the oldest value and
  // n = bufferSize() - 1 returns the most recent value.
  T ReadBuffer(size_t n) const {
    DCHECK(IsFilledIndex(n));
    return buffer_[BufferIndex(n)];
  }

  void SaveToBuffer(T value) {
    buffer_[BufferIndex(0)] = value;
    current_index_++;
  }

 private:
  inline size_t BufferIndex(size_t n) const {
    return (current_index_ + n) % size;
  }

  T buffer_[size];
  size_t current_index_;
};

}  // namespace cc

#endif  // CC_RING_BUFFER_H_
