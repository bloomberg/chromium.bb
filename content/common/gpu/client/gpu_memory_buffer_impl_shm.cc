// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl_shm.h"

#include "base/logging.h"

namespace content {

GpuMemoryBufferImplShm::GpuMemoryBufferImplShm(
    gfx::Size size, unsigned internalformat)
    : GpuMemoryBufferImpl(size, internalformat) {
}

GpuMemoryBufferImplShm::~GpuMemoryBufferImplShm() {
}

bool GpuMemoryBufferImplShm::Initialize(gfx::GpuMemoryBufferHandle handle) {
  if (!base::SharedMemory::IsHandleValid(handle.handle))
    return false;
  shared_memory_.reset(new base::SharedMemory(handle.handle, false));
  DCHECK(!shared_memory_->memory());
  return true;
}

bool GpuMemoryBufferImplShm::InitializeFromSharedMemory(
    scoped_ptr<base::SharedMemory> shared_memory) {
  shared_memory_ = shared_memory.Pass();
  DCHECK(!shared_memory_->memory());
  return true;
}

void GpuMemoryBufferImplShm::Map(AccessMode mode, void** vaddr) {
  DCHECK(!mapped_);
  *vaddr = NULL;
  if (!shared_memory_->Map(size_.GetArea() * BytesPerPixel(internalformat_)))
    return;
  *vaddr = shared_memory_->memory();
  mapped_ = true;
}

void GpuMemoryBufferImplShm::Unmap() {
  DCHECK(mapped_);
  shared_memory_->Unmap();
  mapped_ = false;
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplShm::GetHandle() const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.handle = shared_memory_->handle();
  return handle;
}

}  // namespace content
