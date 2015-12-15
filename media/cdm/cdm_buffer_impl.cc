// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_buffer_impl.h"

#include "base/logging.h"

namespace media {

// static
CdmBuffer* CdmBuffer::Create(uint32_t capacity) {
  DCHECK(capacity);
  return new CdmBuffer(capacity);
}

CdmBuffer::CdmBuffer(uint32_t capacity) : buffer_(capacity), size_(0) {}

CdmBuffer::~CdmBuffer() {}

void CdmBuffer::Destroy() {
  delete this;
}

uint32_t CdmBuffer::Capacity() const {
  return buffer_.size();
}

uint8_t* CdmBuffer::Data() {
  return buffer_.data();
}

void CdmBuffer::SetSize(uint32_t size) {
  DCHECK(size <= Capacity());
  size_ = size > Capacity() ? 0 : size;
}

uint32_t CdmBuffer::Size() const {
  return size_;
}

}  // namespace media
