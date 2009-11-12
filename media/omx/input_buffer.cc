// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include <algorithm>

#include "base/logging.h"
#include "media/omx/input_buffer.h"

namespace media {

InputBuffer::InputBuffer()
    : data_(NULL),
      size_(0),
      used_(0) {
}

InputBuffer::InputBuffer(uint8* data, int size)
    : data_(data),
      size_(size),
      used_(0) {
  DCHECK_GE(size, 0);
}

InputBuffer::~InputBuffer() {
  delete [] data_;
}

int InputBuffer::Read(uint8* output_data, int output_size) {
  int copy = std::min(output_size, size_ - used_);
  memcpy(output_data, data_ + used_, copy);
  used_ += copy;
  return copy;
}

bool InputBuffer::Used() {
  return used_ == size_;
}

bool InputBuffer::Eos() {
  return data_ == NULL || size_ == 0;
}

}  // namespace media
