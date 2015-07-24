// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl_ozone_native_pixmap.h"

#include "ui/ozone/public/surface_factory_ozone.h"

namespace content {

GpuMemoryBufferImplOzoneNativePixmap::GpuMemoryBufferImplOzoneNativePixmap(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    Format format,
    const DestructionCallback& callback)
    : GpuMemoryBufferImpl(id, size, format, callback) {}

GpuMemoryBufferImplOzoneNativePixmap::~GpuMemoryBufferImplOzoneNativePixmap() {}

// static
scoped_ptr<GpuMemoryBufferImpl>
GpuMemoryBufferImplOzoneNativePixmap::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    Format format,
    Usage usage,
    const DestructionCallback& callback) {
  return make_scoped_ptr<GpuMemoryBufferImpl>(
      new GpuMemoryBufferImplOzoneNativePixmap(handle.id, size, format,
                                               callback));
}

bool GpuMemoryBufferImplOzoneNativePixmap::Map(void** data) {
  NOTREACHED();
  return false;
}

void GpuMemoryBufferImplOzoneNativePixmap::Unmap() {
  NOTREACHED();
}

void GpuMemoryBufferImplOzoneNativePixmap::GetStride(int* stride) const {
  NOTREACHED();
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplOzoneNativePixmap::GetHandle()
    const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::OZONE_NATIVE_PIXMAP;
  handle.id = id_;
  return handle;
}

}  // namespace content
