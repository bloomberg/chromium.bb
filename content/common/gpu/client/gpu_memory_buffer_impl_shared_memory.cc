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
  DCHECK(IsFormatSupported(format));
  DCHECK(IsSizeValidForFormat(size, format));
}

GpuMemoryBufferImplSharedMemory::~GpuMemoryBufferImplSharedMemory() {
}

// static
scoped_ptr<GpuMemoryBufferImpl> GpuMemoryBufferImplSharedMemory::Create(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    Format format) {
  scoped_ptr<base::SharedMemory> shared_memory(new base::SharedMemory());

  size_t stride_in_bytes = 0;
  if (!StrideInBytes(size.width(), format, &stride_in_bytes))
    return scoped_ptr<GpuMemoryBufferImpl>();

  base::CheckedNumeric<size_t> size_in_bytes = stride_in_bytes;
  size_in_bytes *= size.height();
  if (!size_in_bytes.IsValid())
    return scoped_ptr<GpuMemoryBufferImpl>();

  if (!shared_memory->CreateAndMapAnonymous(size_in_bytes.ValueOrDie()))
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
  size_t stride_in_bytes = 0;
  if (!StrideInBytes(size.width(), format, &stride_in_bytes))
    return gfx::GpuMemoryBufferHandle();

  base::CheckedNumeric<int> buffer_size = stride_in_bytes;
  buffer_size *= size.height();
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

  size_t stride_in_bytes = 0;
  if (!StrideInBytes(size.width(), format, &stride_in_bytes))
    return scoped_ptr<GpuMemoryBufferImpl>();

  base::CheckedNumeric<size_t> size_in_bytes = stride_in_bytes;
  size_in_bytes *= size.height();
  if (!size_in_bytes.IsValid())
    return scoped_ptr<GpuMemoryBufferImpl>();

  scoped_ptr<base::SharedMemory> shared_memory(
      new base::SharedMemory(handle.handle, false));
  if (!shared_memory->Map(size_in_bytes.ValueOrDie()))
    return scoped_ptr<GpuMemoryBufferImpl>();

  return make_scoped_ptr<GpuMemoryBufferImpl>(
      new GpuMemoryBufferImplSharedMemory(
          handle.id,
          size,
          format,
          callback,
          shared_memory.Pass()));
}

// static
bool GpuMemoryBufferImplSharedMemory::IsFormatSupported(Format format) {
  switch (format) {
    case ATC:
    case ATCIA:
    case DXT1:
    case DXT5:
    case ETC1:
    case RGBA_8888:
    case BGRA_8888:
      return true;
    case RGBX_8888:
      return false;
  }

  NOTREACHED();
  return false;
}

// static
bool GpuMemoryBufferImplSharedMemory::IsSizeValidForFormat(
    const gfx::Size& size,
    Format format) {
  switch (format) {
    case ATC:
    case ATCIA:
    case DXT1:
    case DXT5:
    case ETC1:
      // Compressed images must have a width and height that's evenly divisible
      // by the block size.
      return size.width() % 4 == 0 && size.height() % 4 == 0;
    case RGBA_8888:
    case BGRA_8888:
    case RGBX_8888:
      return true;
  }

  NOTREACHED();
  return false;
}

void* GpuMemoryBufferImplSharedMemory::Map() {
  DCHECK(!mapped_);
  mapped_ = true;
  return shared_memory_->memory();
}

void GpuMemoryBufferImplSharedMemory::Unmap() {
  DCHECK(mapped_);
  mapped_ = false;
}

uint32 GpuMemoryBufferImplSharedMemory::GetStride() const {
  size_t stride_in_bytes = 0;
  bool valid_stride = StrideInBytes(size_.width(), format_, &stride_in_bytes);
  DCHECK(valid_stride);
  return stride_in_bytes;
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplSharedMemory::GetHandle() const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.id = id_;
  handle.handle = shared_memory_->handle();
  return handle;
}

}  // namespace content
