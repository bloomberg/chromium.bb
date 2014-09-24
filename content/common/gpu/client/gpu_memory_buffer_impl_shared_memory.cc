// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl_shared_memory.h"

#include "base/bind.h"
#include "base/numerics/safe_math.h"
#include "ui/gl/gl_bindings.h"

namespace content {
namespace {

void Noop() {
}

}  // namespace

GpuMemoryBufferImplSharedMemory::GpuMemoryBufferImplSharedMemory(
    const gfx::Size& size,
    unsigned internalformat,
    const DestructionCallback& callback,
    scoped_ptr<base::SharedMemory> shared_memory)
    : GpuMemoryBufferImpl(size, internalformat, callback),
      shared_memory_(shared_memory.Pass()) {
}

GpuMemoryBufferImplSharedMemory::~GpuMemoryBufferImplSharedMemory() {
}

// static
void GpuMemoryBufferImplSharedMemory::Create(const gfx::Size& size,
                                             unsigned internalformat,
                                             unsigned usage,
                                             const CreationCallback& callback) {
  DCHECK(GpuMemoryBufferImplSharedMemory::IsConfigurationSupported(
      size, internalformat, usage));

  scoped_ptr<base::SharedMemory> shared_memory(new base::SharedMemory());
  if (!shared_memory->CreateAnonymous(size.GetArea() *
                                      BytesPerPixel(internalformat))) {
    callback.Run(scoped_ptr<GpuMemoryBufferImpl>());
    return;
  }

  callback.Run(
      make_scoped_ptr<GpuMemoryBufferImpl>(new GpuMemoryBufferImplSharedMemory(
          size, internalformat, base::Bind(&Noop), shared_memory.Pass())));
}

// static
void GpuMemoryBufferImplSharedMemory::AllocateForChildProcess(
    const gfx::Size& size,
    unsigned internalformat,
    base::ProcessHandle child_process,
    const AllocationCallback& callback) {
  DCHECK(IsLayoutSupported(size, internalformat));
  base::SharedMemory shared_memory;
  if (!shared_memory.CreateAnonymous(size.GetArea() *
                                     BytesPerPixel(internalformat))) {
    callback.Run(gfx::GpuMemoryBufferHandle());
    return;
  }
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  shared_memory.GiveToProcess(child_process, &handle.handle);
  callback.Run(handle);
}

// static
scoped_ptr<GpuMemoryBufferImpl>
GpuMemoryBufferImplSharedMemory::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    unsigned internalformat,
    const DestructionCallback& callback) {
  DCHECK(IsLayoutSupported(size, internalformat));

  if (!base::SharedMemory::IsHandleValid(handle.handle))
    return scoped_ptr<GpuMemoryBufferImpl>();

  return make_scoped_ptr<GpuMemoryBufferImpl>(
      new GpuMemoryBufferImplSharedMemory(
          size,
          internalformat,
          callback,
          make_scoped_ptr(new base::SharedMemory(handle.handle, false))));
}

// static
bool GpuMemoryBufferImplSharedMemory::IsLayoutSupported(
    const gfx::Size& size,
    unsigned internalformat) {
  base::CheckedNumeric<int> buffer_size = size.width();
  buffer_size *= size.height();
  buffer_size *= BytesPerPixel(internalformat);
  return buffer_size.IsValid();
}

// static
bool GpuMemoryBufferImplSharedMemory::IsUsageSupported(unsigned usage) {
  switch (usage) {
    case GL_IMAGE_MAP_CHROMIUM:
      return true;
    default:
      return false;
  }
}

// static
bool GpuMemoryBufferImplSharedMemory::IsConfigurationSupported(
    const gfx::Size& size,
    unsigned internalformat,
    unsigned usage) {
  return IsLayoutSupported(size, internalformat) && IsUsageSupported(usage);
}

void* GpuMemoryBufferImplSharedMemory::Map() {
  DCHECK(!mapped_);
  if (!shared_memory_->Map(size_.GetArea() * BytesPerPixel(internalformat_)))
    return NULL;
  mapped_ = true;
  return shared_memory_->memory();
}

void GpuMemoryBufferImplSharedMemory::Unmap() {
  DCHECK(mapped_);
  shared_memory_->Unmap();
  mapped_ = false;
}

uint32 GpuMemoryBufferImplSharedMemory::GetStride() const {
  return size_.width() * BytesPerPixel(internalformat_);
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplSharedMemory::GetHandle() const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.handle = shared_memory_->handle();
  return handle;
}

}  // namespace content
