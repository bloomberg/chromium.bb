// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/buffer.h"

#include "base/logging.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"

namespace mojo {
namespace internal {

Buffer::Buffer() = default;

Buffer::Buffer(void* data, size_t size) : data_(data), size_(size), cursor_(0) {
  DCHECK(IsAligned(data_));
}

Buffer::Buffer(Buffer&& other) {
  *this = std::move(other);
}

Buffer::~Buffer() = default;

Buffer& Buffer::operator=(Buffer&& other) {
  data_ = other.data_;
  size_ = other.size_;
  cursor_ = other.cursor_;
  other.Reset();
  return *this;
}

void* Buffer::Allocate(size_t num_bytes) {
  const size_t block_start = cursor_;
  cursor_ += Align(num_bytes);
  if (cursor_ > size_ || cursor_ < block_start) {
    NOTREACHED();
    cursor_ = block_start;
    return nullptr;
  }
  DCHECK_LE(cursor_, size_);
  return reinterpret_cast<char*>(data_) + block_start;
}

void Buffer::Reset() {
  data_ = nullptr;
  size_ = 0;
  cursor_ = 0;
}

}  // namespace internal
}  // namespace mojo
