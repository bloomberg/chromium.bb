// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/buffer.h"

namespace gpu {

Buffer::Buffer(scoped_ptr<base::SharedMemory> shared_memory, size_t size)
    : shared_memory_(shared_memory.Pass()),
      memory_(shared_memory_->memory()),
      size_(size) {}

Buffer::~Buffer() {}

} // namespace gpu
