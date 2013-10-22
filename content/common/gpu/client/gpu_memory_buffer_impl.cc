// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl.h"

#include "ui/gl/gl_bindings.h"

namespace content {

GpuMemoryBufferImpl::GpuMemoryBufferImpl(
    scoped_ptr<base::SharedMemory> shared_memory,
    size_t width,
    size_t height,
    unsigned internalformat)
    : shared_memory_(shared_memory.Pass()),
      size_(gfx::Size(width, height)),
      internalformat_(internalformat),
      mapped_(false) {
  DCHECK(!shared_memory_->memory());
  DCHECK(IsFormatValid(internalformat));
}

GpuMemoryBufferImpl::~GpuMemoryBufferImpl() {
}

void GpuMemoryBufferImpl::Map(AccessMode mode, void** vaddr) {
  DCHECK(!mapped_);
  *vaddr = NULL;
  if (!shared_memory_->Map(size_.GetArea() * BytesPerPixel(internalformat_)))
    return;
  *vaddr = shared_memory_->memory();
  mapped_ = true;
}

void GpuMemoryBufferImpl::Unmap() {
  DCHECK(mapped_);
  shared_memory_->Unmap();
  mapped_ = false;
}

bool GpuMemoryBufferImpl::IsMapped() const {
  return mapped_;
}

uint32 GpuMemoryBufferImpl::GetStride() const {
  return size_.width() * BytesPerPixel(internalformat_);
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImpl::GetHandle() const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.handle = shared_memory_->handle();
  return handle;
}

// static
bool GpuMemoryBufferImpl::IsFormatValid(unsigned internalformat) {
  // GL_RGBA8_OES is the only supported format at the moment.
  switch (internalformat) {
    case GL_RGBA8_OES:
      return true;
    default:
      return false;
  }
}

// static
size_t GpuMemoryBufferImpl::BytesPerPixel(unsigned internalformat) {
  // GL_RGBA_OES has 4 bytes per pixel.
  switch (internalformat) {
    case GL_RGBA8_OES:
      return 4;
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace content
