// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "media/omx/omx_input_buffer.h"

#include <algorithm>

#include "base/logging.h"

namespace media {

OmxInputBuffer::OmxInputBuffer()
    : size_(0),
      used_(0) {
}

OmxInputBuffer::OmxInputBuffer(uint8* data, int size)
    : data_(data),
      size_(size),
      used_(0) {
  DCHECK_GE(size, 0);
}

OmxInputBuffer::~OmxInputBuffer() {
}

int OmxInputBuffer::Read(uint8* output_data, int output_size) {
  int copy = std::min(output_size, size_ - used_);
  memcpy(output_data, data_.get() + used_, copy);
  used_ += copy;
  return copy;
}

bool OmxInputBuffer::Used() {
  return used_ == size_;
}

}  // namespace media
