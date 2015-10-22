// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gles2/mojo_gpu_memory_buffer.h"

#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/numerics/safe_conversions.h"
#include "ui/gfx/buffer_format_util.h"

namespace mus {

MojoGpuMemoryBufferImpl::MojoGpuMemoryBufferImpl(
    const gfx::Size& size,
    gfx::BufferFormat format,
    scoped_ptr<base::SharedMemory> shared_memory)
    : size_(size),
      format_(format),
      shared_memory_(shared_memory.Pass()),
      mapped_(false) {}

MojoGpuMemoryBufferImpl::~MojoGpuMemoryBufferImpl() {}

scoped_ptr<gfx::GpuMemoryBuffer> MojoGpuMemoryBufferImpl::Create(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  size_t bytes = gfx::BufferSizeForBufferFormat(size, format);
  scoped_ptr<base::SharedMemory> shared_memory(new base::SharedMemory);
  if (!shared_memory->CreateAnonymous(bytes))
    return nullptr;
  return make_scoped_ptr<gfx::GpuMemoryBuffer>(
      new MojoGpuMemoryBufferImpl(size, format, shared_memory.Pass()));
}

MojoGpuMemoryBufferImpl* MojoGpuMemoryBufferImpl::FromClientBuffer(
    ClientBuffer buffer) {
  return reinterpret_cast<MojoGpuMemoryBufferImpl*>(buffer);
}

const unsigned char* MojoGpuMemoryBufferImpl::GetMemory() const {
  return static_cast<const unsigned char*>(shared_memory_->memory());
}

bool MojoGpuMemoryBufferImpl::Map() {
  DCHECK(!mapped_);
  if (!shared_memory_->Map(gfx::BufferSizeForBufferFormat(size_, format_)))
    return false;
  mapped_ = true;
  return true;
}

void* MojoGpuMemoryBufferImpl::memory(size_t plane) {
  DCHECK(mapped_);
  DCHECK_LT(plane, gfx::NumberOfPlanesForBufferFormat(format_));
  return reinterpret_cast<uint8*>(shared_memory_->memory()) +
         gfx::BufferOffsetForBufferFormat(size_, format_, plane);
}

void MojoGpuMemoryBufferImpl::Unmap() {
  DCHECK(mapped_);
  shared_memory_->Unmap();
  mapped_ = false;
}

gfx::Size MojoGpuMemoryBufferImpl::GetSize() const {
  return size_;
}

gfx::BufferFormat MojoGpuMemoryBufferImpl::GetFormat() const {
  return format_;
}

int MojoGpuMemoryBufferImpl::stride(size_t plane) const {
  DCHECK_LT(plane, gfx::NumberOfPlanesForBufferFormat(format_));
  return base::checked_cast<int>(gfx::RowSizeForBufferFormat(
      size_.width(), format_, static_cast<int>(plane)));
}

gfx::GpuMemoryBufferId MojoGpuMemoryBufferImpl::GetId() const {
  return gfx::GpuMemoryBufferId(0);
}

gfx::GpuMemoryBufferHandle MojoGpuMemoryBufferImpl::GetHandle() const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.handle = shared_memory_->handle();
  return handle;
}

ClientBuffer MojoGpuMemoryBufferImpl::AsClientBuffer() {
  return reinterpret_cast<ClientBuffer>(this);
}

}  // namespace mus
