// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/gpu_memory_buffer_factory.h"

#include "base/logging.h"

namespace gpu {

namespace {
GpuMemoryBuffer::Creator* g_gpu_memory_buffer_factory_ = NULL;
}

const GpuMemoryBuffer::Creator& GetProcessDefaultGpuMemoryBufferFactory() {
  return *g_gpu_memory_buffer_factory_;
}

void SetProcessDefaultGpuMemoryBufferFactory(
    const GpuMemoryBuffer::Creator& factory) {
  DCHECK(g_gpu_memory_buffer_factory_ == NULL);
  g_gpu_memory_buffer_factory_ = new GpuMemoryBuffer::Creator(factory);
}

}  // namespace gpu
