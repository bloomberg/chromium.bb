// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/binder/buffer_reader.h"

namespace binder {

BufferReader::BufferReader(const char* data, size_t size)
    : data_(data), size_(size), position_(0) {}

BufferReader::~BufferReader() {}

bool BufferReader::HasMoreData() const {
  return position_ < size_;
}

bool BufferReader::Read(void* out, size_t num_bytes) {
  if (position_ + num_bytes > size_)
    return false;
  memcpy(out, data_ + position_, num_bytes);
  position_ += num_bytes;
  return true;
}

bool BufferReader::Skip(size_t num_bytes) {
  if (position_ + num_bytes > size_)
    return false;
  position_ += num_bytes;
  return true;
}

}  // namespace binder
