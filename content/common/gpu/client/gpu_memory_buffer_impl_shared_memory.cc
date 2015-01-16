// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl_shared_memory.h"

#include "base/bind.h"
#include "base/numerics/safe_math.h"
#include "ui/gl/gl_bindings.h"

namespace content {
namespace {

void Noop(uint32 sync_point) {
}

}  // namespace

GpuMemoryBufferImplSharedMemory::GpuMemoryBufferImplSharedMemory(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    Format format,
    const DestructionCallback& callback,
    scoped_ptr<base::SharedMemory> shared_memory)
    : GpuMemoryBufferImpl(id, size, format, callback),
      shared_memory_(shared_memory.Pass()) {
}

GpuMemoryBufferImplSharedMemory::~GpuMemoryBufferImplSharedMemory() {
}

// static
scoped_ptr<GpuMemoryBufferImpl> GpuMemoryBufferImplSharedMemory::Create(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    Format format) {
  scoped_ptr<base::SharedMemory> shared_memory(new base::SharedMemory());
  if (!shared_memory->CreateAnonymous(size.GetArea() * BytesPerPixel(format)))
    return scoped_ptr<GpuMemoryBufferImpl>();

  return make_scoped_ptr(new GpuMemoryBufferImplSharedMemory(
      id, size, format, base::Bind(&Noop), shared_memory.Pass()));
}

// static
gfx::GpuMemoryBufferHandle
GpuMemoryBufferImplSharedMemory::AllocateForChildProcess(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    Format format,
    base::ProcessHandle child_process) {
  base::CheckedNumeric<int> buffer_size = size.width();
  buffer_size *= size.height();
  buffer_size *= BytesPerPixel(format);
  if (!buffer_size.IsValid())
    return gfx::GpuMemoryBufferHandle();

  base::SharedMemory shared_memory;
  if (!shared_memory.CreateAnonymous(buffer_size.ValueOrDie()))
    return gfx::GpuMemoryBufferHandle();

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.id = id;
  shared_memory.GiveToProcess(child_process, &handle.handle);
  return handle;
}

// static
scoped_ptr<GpuMemoryBufferImpl>
GpuMemoryBufferImplSharedMemory::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    Format format,
    const DestructionCallback& callback) {
  if (!base::SharedMemory::IsHandleValid(handle.handle))
    return scoped_ptr<GpuMemoryBufferImpl>();

  return make_scoped_ptr<GpuMemoryBufferImpl>(
      new GpuMemoryBufferImplSharedMemory(
          handle.id,
          size,
          format,
          callback,
          make_scoped_ptr(new base::SharedMemory(handle.handle, false))));
}

// static
bool GpuMemoryBufferImplSharedMemory::IsFormatSupported(Format format) {
  switch (format) {
    case RGBA_8888:
    case BGRA_8888:
      return true;
    case RGBX_8888:
      return false;
  }

  NOTREACHED();
  return false;
}

void* GpuMemoryBufferImplSharedMemory::Map() {
  DCHECK(!mapped_);
  if (!shared_memory_->Map(size_.GetArea() * BytesPerPixel(format_)))
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
  return size_.width() * BytesPerPixel(format_);
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplSharedMemory::GetHandle() const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.id = id_;
  handle.handle = shared_memory_->handle();
  return handle;
}

}  // namespace content
