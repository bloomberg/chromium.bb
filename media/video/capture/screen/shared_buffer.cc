// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/screen/shared_buffer.h"

const bool kReadOnly = true;

namespace media {

SharedBuffer::SharedBuffer(uint32 size)
    : id_(0),
      size_(size) {
  shared_memory_.CreateAndMapAnonymous(size);
}

SharedBuffer::SharedBuffer(
    int id, base::SharedMemoryHandle handle, uint32 size)
    : id_(id),
      shared_memory_(handle, kReadOnly),
      size_(size) {
  shared_memory_.Map(size);
}

SharedBuffer::SharedBuffer(
    int id, base::SharedMemoryHandle handle, base::ProcessHandle process,
    uint32 size)
    : id_(id),
      shared_memory_(handle, kReadOnly, process),
      size_(size) {
  shared_memory_.Map(size);
}

SharedBuffer::~SharedBuffer() {
}

bool SharedBuffer::ShareToProcess(base::ProcessHandle process,
                                  base::SharedMemoryHandle* new_handle) {
  return shared_memory_.ShareToProcess(process, new_handle);
}

}  // namespace media
