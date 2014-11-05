// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl_surface_texture.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "content/common/android/surface_texture_manager.h"
#include "content/common/gpu/client/gpu_memory_buffer_factory_host.h"
#include "ui/gl/gl_bindings.h"

namespace content {
namespace {

void GpuMemoryBufferDeleted(gfx::GpuMemoryBufferId id,
                            int client_id,
                            uint32 sync_point) {
  GpuMemoryBufferFactoryHost::GetInstance()->DestroyGpuMemoryBuffer(
      gfx::SURFACE_TEXTURE_BUFFER, id, client_id, sync_point);
}

void GpuMemoryBufferCreated(
    const gfx::Size& size,
    gfx::GpuMemoryBuffer::Format format,
    int client_id,
    const GpuMemoryBufferImpl::CreationCallback& callback,
    const gfx::GpuMemoryBufferHandle& handle) {
  if (handle.is_null()) {
    callback.Run(scoped_ptr<GpuMemoryBufferImpl>());
    return;
  }

  DCHECK_EQ(gfx::SURFACE_TEXTURE_BUFFER, handle.type);
  callback.Run(GpuMemoryBufferImplSurfaceTexture::CreateFromHandle(
      handle,
      size,
      format,
      base::Bind(&GpuMemoryBufferDeleted, handle.id, client_id)));
}

void GpuMemoryBufferCreatedForChildProcess(
    const GpuMemoryBufferImpl::AllocationCallback& callback,
    const gfx::GpuMemoryBufferHandle& handle) {
  DCHECK_IMPLIES(!handle.is_null(), gfx::SURFACE_TEXTURE_BUFFER == handle.type);

  callback.Run(handle);
}

}  // namespace

GpuMemoryBufferImplSurfaceTexture::GpuMemoryBufferImplSurfaceTexture(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    Format format,
    const DestructionCallback& callback,
    ANativeWindow* native_window)
    : GpuMemoryBufferImpl(id, size, format, callback),
      native_window_(native_window),
      stride_(0) {
}

GpuMemoryBufferImplSurfaceTexture::~GpuMemoryBufferImplSurfaceTexture() {
  ANativeWindow_release(native_window_);
}

// static
void GpuMemoryBufferImplSurfaceTexture::Create(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    Format format,
    int client_id,
    const CreationCallback& callback) {
  GpuMemoryBufferFactoryHost::GetInstance()->CreateGpuMemoryBuffer(
      gfx::SURFACE_TEXTURE_BUFFER,
      id,
      size,
      format,
      MAP,
      client_id,
      base::Bind(&GpuMemoryBufferCreated, size, format, client_id, callback));
}

// static
void GpuMemoryBufferImplSurfaceTexture::AllocateForChildProcess(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    Format format,
    int child_client_id,
    const AllocationCallback& callback) {
  GpuMemoryBufferFactoryHost::GetInstance()->CreateGpuMemoryBuffer(
      gfx::SURFACE_TEXTURE_BUFFER,
      id,
      size,
      format,
      MAP,
      child_client_id,
      base::Bind(&GpuMemoryBufferCreatedForChildProcess, callback));
}

// static
scoped_ptr<GpuMemoryBufferImpl>
GpuMemoryBufferImplSurfaceTexture::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    Format format,
    const DestructionCallback& callback) {
  DCHECK(IsFormatSupported(format));

  ANativeWindow* native_window = SurfaceTextureManager::GetInstance()->
      AcquireNativeWidgetForSurfaceTexture(handle.id);
  if (!native_window)
    return scoped_ptr<GpuMemoryBufferImpl>();

  ANativeWindow_setBuffersGeometry(
      native_window, size.width(), size.height(), WindowFormat(format));

  return make_scoped_ptr<GpuMemoryBufferImpl>(
      new GpuMemoryBufferImplSurfaceTexture(
          handle.id, size, format, callback, native_window));
}

// static
void GpuMemoryBufferImplSurfaceTexture::DeletedByChildProcess(
    gfx::GpuMemoryBufferId id,
    int child_client_id,
    uint32_t sync_point) {
  GpuMemoryBufferFactoryHost::GetInstance()->DestroyGpuMemoryBuffer(
      gfx::SURFACE_TEXTURE_BUFFER, id, child_client_id, sync_point);
}

// static
bool GpuMemoryBufferImplSurfaceTexture::IsFormatSupported(Format format) {
  switch (format) {
    case RGBA_8888:
      return true;
    case RGBX_8888:
    case BGRA_8888:
      return false;
  }

  NOTREACHED();
  return false;
}

// static
bool GpuMemoryBufferImplSurfaceTexture::IsUsageSupported(Usage usage) {
  switch (usage) {
    case MAP:
      return true;
    case SCANOUT:
      return false;
  }

  NOTREACHED();
  return false;
}

// static
bool GpuMemoryBufferImplSurfaceTexture::IsConfigurationSupported(Format format,
                                                                 Usage usage) {
  return IsFormatSupported(format) && IsUsageSupported(usage);
}

// static
int GpuMemoryBufferImplSurfaceTexture::WindowFormat(Format format) {
  switch (format) {
    case RGBA_8888:
      return WINDOW_FORMAT_RGBA_8888;
    case RGBX_8888:
    case BGRA_8888:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

void* GpuMemoryBufferImplSurfaceTexture::Map() {
  TRACE_EVENT0("gpu", "GpuMemoryBufferImplSurfaceTexture::Map");

  DCHECK(!mapped_);
  DCHECK(native_window_);
  ANativeWindow_Buffer buffer;
  int status = ANativeWindow_lock(native_window_, &buffer, NULL);
  if (status) {
    VLOG(1) << "ANativeWindow_lock failed with error code: " << status;
    return NULL;
  }

  DCHECK_LE(size_.width(), buffer.stride);
  stride_ = buffer.stride * BytesPerPixel(format_);
  mapped_ = true;
  return buffer.bits;
}

void GpuMemoryBufferImplSurfaceTexture::Unmap() {
  TRACE_EVENT0("gpu", "GpuMemoryBufferImplSurfaceTexture::Unmap");

  DCHECK(mapped_);
  ANativeWindow_unlockAndPost(native_window_);
  mapped_ = false;
}

uint32 GpuMemoryBufferImplSurfaceTexture::GetStride() const {
  return stride_;
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplSurfaceTexture::GetHandle()
    const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SURFACE_TEXTURE_BUFFER;
  handle.id = id_;
  return handle;
}

}  // namespace content
