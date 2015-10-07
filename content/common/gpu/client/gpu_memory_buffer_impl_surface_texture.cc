// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl_surface_texture.h"

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "content/common/android/surface_texture_manager.h"
#include "content/common/gpu/gpu_memory_buffer_factory_surface_texture.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"

namespace content {
namespace {

int WindowFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::RGBA_8888:
      return WINDOW_FORMAT_RGBA_8888;
    case gfx::BufferFormat::ATC:
    case gfx::BufferFormat::ATCIA:
    case gfx::BufferFormat::DXT1:
    case gfx::BufferFormat::DXT5:
    case gfx::BufferFormat::ETC1:
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::YUV_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::UYVY_422:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

void FreeSurfaceTextureForTesting(
    scoped_refptr<gfx::SurfaceTexture> surface_texture,
    gfx::GpuMemoryBufferId id) {
  SurfaceTextureManager::GetInstance()->UnregisterSurfaceTexture(id.id, 0);
}

}  // namespace

GpuMemoryBufferImplSurfaceTexture::GpuMemoryBufferImplSurfaceTexture(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    const DestructionCallback& callback,
    ANativeWindow* native_window)
    : GpuMemoryBufferImpl(id, size, format, callback),
      native_window_(native_window),
      stride_(0) {}

GpuMemoryBufferImplSurfaceTexture::~GpuMemoryBufferImplSurfaceTexture() {
  ANativeWindow_release(native_window_);
}

// static
scoped_ptr<GpuMemoryBufferImplSurfaceTexture>
GpuMemoryBufferImplSurfaceTexture::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    const DestructionCallback& callback) {
  ANativeWindow* native_window =
      SurfaceTextureManager::GetInstance()
          ->AcquireNativeWidgetForSurfaceTexture(handle.id.id);
  if (!native_window)
    return nullptr;

  ANativeWindow_setBuffersGeometry(
      native_window, size.width(), size.height(), WindowFormat(format));

  return make_scoped_ptr(new GpuMemoryBufferImplSurfaceTexture(
      handle.id, size, format, callback, native_window));
}

// static
bool GpuMemoryBufferImplSurfaceTexture::IsConfigurationSupported(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  return GpuMemoryBufferFactorySurfaceTexture::
      IsGpuMemoryBufferConfigurationSupported(format, usage);
}

// static
base::Closure GpuMemoryBufferImplSurfaceTexture::AllocateForTesting(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gfx::GpuMemoryBufferHandle* handle) {
  scoped_refptr<gfx::SurfaceTexture> surface_texture =
      gfx::SurfaceTexture::Create(0);
  DCHECK(surface_texture);
  gfx::GpuMemoryBufferId kBufferId(1);
  SurfaceTextureManager::GetInstance()->RegisterSurfaceTexture(
      kBufferId.id, 0, surface_texture.get());
  handle->type = gfx::SURFACE_TEXTURE_BUFFER;
  handle->id = kBufferId;
  return base::Bind(&FreeSurfaceTextureForTesting, surface_texture, kBufferId);
}

bool GpuMemoryBufferImplSurfaceTexture::Map(void** data) {
  TRACE_EVENT0("gpu", "GpuMemoryBufferImplSurfaceTexture::Map");

  DCHECK(!mapped_);
  DCHECK(native_window_);
  ANativeWindow_Buffer buffer;
  int status = ANativeWindow_lock(native_window_, &buffer, NULL);
  if (status) {
    VLOG(1) << "ANativeWindow_lock failed with error code: " << status;
    return false;
  }

  DCHECK_LE(size_.width(), buffer.stride);
  stride_ = gfx::RowSizeForBufferFormat(buffer.stride, format_, 0);
  mapped_ = true;
  *data = buffer.bits;
  return true;
}

void GpuMemoryBufferImplSurfaceTexture::Unmap() {
  TRACE_EVENT0("gpu", "GpuMemoryBufferImplSurfaceTexture::Unmap");

  DCHECK(mapped_);
  ANativeWindow_unlockAndPost(native_window_);
  mapped_ = false;
}

void GpuMemoryBufferImplSurfaceTexture::GetStride(int* stride) const {
  *stride = stride_;
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplSurfaceTexture::GetHandle()
    const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SURFACE_TEXTURE_BUFFER;
  handle.id = id_;
  return handle;
}

}  // namespace content
