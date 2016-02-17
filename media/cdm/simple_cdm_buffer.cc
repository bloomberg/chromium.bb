// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/simple_cdm_buffer.h"

#include "base/logging.h"

namespace media {

// static
SimpleCdmBuffer* SimpleCdmBuffer::Create(uint32_t capacity) {
  DCHECK(capacity);
  return new SimpleCdmBuffer(capacity);
}

SimpleCdmBuffer::SimpleCdmBuffer(uint32_t capacity)
    : buffer_(capacity), size_(0) {}

SimpleCdmBuffer::~SimpleCdmBuffer() {}

void SimpleCdmBuffer::Destroy() {
  delete this;
}

uint32_t SimpleCdmBuffer::Capacity() const {
  return buffer_.size();
}

uint8_t* SimpleCdmBuffer::Data() {
  return buffer_.data();
}

void SimpleCdmBuffer::SetSize(uint32_t size) {
  DCHECK(size <= Capacity());
  size_ = size > Capacity() ? 0 : size;
}

uint32_t SimpleCdmBuffer::Size() const {
  return size_;
}

}  // namespace media
