// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/gles2/mojo_gpu_memory_buffer_manager.h"

#include "base/logging.h"
#include "components/view_manager/gles2/mojo_gpu_memory_buffer.h"

namespace gles2 {

MojoGpuMemoryBufferManager::MojoGpuMemoryBufferManager() {
}

MojoGpuMemoryBufferManager::~MojoGpuMemoryBufferManager() {
}

scoped_ptr<gfx::GpuMemoryBuffer>
MojoGpuMemoryBufferManager::AllocateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::GpuMemoryBuffer::Format format,
    gfx::GpuMemoryBuffer::Usage usage) {
  return MojoGpuMemoryBufferImpl::Create(size, format, usage);
}

gfx::GpuMemoryBuffer*
MojoGpuMemoryBufferManager::GpuMemoryBufferFromClientBuffer(
    ClientBuffer buffer)  {
  return MojoGpuMemoryBufferImpl::FromClientBuffer(buffer);
}

void MojoGpuMemoryBufferManager::SetDestructionSyncPoint(
    gfx::GpuMemoryBuffer* buffer, uint32 sync_point) {
  NOTIMPLEMENTED();
}

}  // namespace gles2
