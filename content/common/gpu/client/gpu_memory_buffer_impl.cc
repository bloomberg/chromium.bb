// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl.h"

#include "ui/gl/gl_bindings.h"

namespace content {

GpuMemoryBufferImpl::GpuMemoryBufferImpl(
    gfx::Size size, unsigned internalformat)
    : size_(size),
      internalformat_(internalformat),
      mapped_(false) {
  DCHECK(IsFormatValid(internalformat));
}

GpuMemoryBufferImpl::~GpuMemoryBufferImpl() {
}

// static
bool GpuMemoryBufferImpl::IsFormatValid(unsigned internalformat) {
  switch (internalformat) {
    case GL_BGRA8_EXT:
    case GL_RGBA8_OES:
      return true;
    default:
      return false;
  }
}

// static
size_t GpuMemoryBufferImpl::BytesPerPixel(unsigned internalformat) {
  switch (internalformat) {
    case GL_BGRA8_EXT:
    case GL_RGBA8_OES:
      return 4;
    default:
      NOTREACHED();
      return 0;
  }
}

bool GpuMemoryBufferImpl::IsMapped() const {
  return mapped_;
}

uint32 GpuMemoryBufferImpl::GetStride() const {
  return size_.width() * BytesPerPixel(internalformat_);
}

}  // namespace content
