// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/shared_memory_buffer_handle.h"

#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace media {

SharedMemoryBufferHandle::SharedMemoryBufferHandle(
    base::SharedMemory* shared_memory,
    size_t mapped_size)
    : shared_memory_(shared_memory), mapped_size_(mapped_size) {}

SharedMemoryBufferHandle::~SharedMemoryBufferHandle() = default;

size_t SharedMemoryBufferHandle::mapped_size() const {
  return mapped_size_;
}

uint8_t* SharedMemoryBufferHandle::data() const {
  return static_cast<uint8_t*>(shared_memory_->memory());
}

const uint8_t* SharedMemoryBufferHandle::const_data() const {
  return static_cast<const uint8_t*>(shared_memory_->memory());
}

}  // namespace media
