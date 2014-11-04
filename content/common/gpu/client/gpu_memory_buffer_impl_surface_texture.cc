// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl_surface_texture.h"

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "content/common/android/surface_texture_manager.h"
#include "content/common/gpu/client/gpu_memory_buffer_factory_host.h"
#include "ui/gl/gl_bindings.h"

namespace content {
namespace {

base::StaticAtomicSequenceNumber g_next_buffer_id;

void GpuMemoryBufferDeleted(const gfx::GpuMemoryBufferHandle& handle,
                            uint32 sync_point) {
  GpuMemoryBufferFactoryHost::GetInstance()->DestroyGpuMemoryBuffer(handle,
                                                                    sync_point);
}

void GpuMemoryBufferCreated(
    const gfx::Size& size,
    gfx::GpuMemoryBuffer::Format format,
    const GpuMemoryBufferImpl::CreationCallback& callback,
    const gfx::GpuMemoryBufferHandle& handle) {
  DCHECK_EQ(gfx::SURFACE_TEXTURE_BUFFER, handle.type);

  callback.Run(GpuMemoryBufferImplSurfaceTexture::CreateFromHandle(
      handle, size, format, base::Bind(&GpuMemoryBufferDeleted, handle)));
}

void GpuMemoryBufferCreatedForChildProcess(
    const GpuMemoryBufferImpl::AllocationCallback& callback,
    const gfx::GpuMemoryBufferHandle& handle) {
  DCHECK_EQ(gfx::SURFACE_TEXTURE_BUFFER, handle.type);

  callback.Run(handle);
}

}  // namespace

GpuMemoryBufferImplSurfaceTexture::GpuMemoryBufferImplSurfaceTexture(
    const gfx::Size& size,
    Format format,
    const DestructionCallback& callback,
    const gfx::GpuMemoryBufferId& id,
    ANativeWindow* native_window)
    : GpuMemoryBufferImpl(size, format, callback),
      id_(id),
      native_window_(native_window),
      stride_(0u) {
}

GpuMemoryBufferImplSurfaceTexture::~GpuMemoryBufferImplSurfaceTexture() {
  ANativeWindow_release(native_window_);
}

// static
void GpuMemoryBufferImplSurfaceTexture::Create(
    const gfx::Size& size,
    Format format,
    int client_id,
    const CreationCallback& callback) {
  gfx::GpuMemoryBufferHandle handle;
  handle.global_id.primary_id = g_next_buffer_id.GetNext();
  handle.global_id.secondary_id = client_id;
  handle.type = gfx::SURFACE_TEXTURE_BUFFER;
  GpuMemoryBufferFactoryHost::GetInstance()->CreateGpuMemoryBuffer(
      handle,
      size,
      format,
      MAP,
      base::Bind(&GpuMemoryBufferCreated, size, format, callback));
}

// static
void GpuMemoryBufferImplSurfaceTexture::AllocateForChildProcess(
    const gfx::Size& size,
    Format format,
    int child_client_id,
    const AllocationCallback& callback) {
  gfx::GpuMemoryBufferHandle handle;
  handle.global_id.primary_id = g_next_buffer_id.GetNext();
  handle.global_id.secondary_id = child_client_id;
  handle.type = gfx::SURFACE_TEXTURE_BUFFER;
  GpuMemoryBufferFactoryHost::GetInstance()->CreateGpuMemoryBuffer(
      handle,
      size,
      format,
      MAP,
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

  ANativeWindow* native_window =
      SurfaceTextureManager::GetInstance()->AcquireNativeWidget(
          handle.global_id.primary_id, handle.global_id.secondary_id);
  if (!native_window)
    return scoped_ptr<GpuMemoryBufferImpl>();

  ANativeWindow_setBuffersGeometry(
      native_window, size.width(), size.height(), WindowFormat(format));

  return make_scoped_ptr<GpuMemoryBufferImpl>(
      new GpuMemoryBufferImplSurfaceTexture(
          size, format, callback, handle.global_id, native_window));
}

// static
void GpuMemoryBufferImplSurfaceTexture::DeletedByChildProcess(
    const gfx::GpuMemoryBufferId& id,
    uint32_t sync_point) {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SURFACE_TEXTURE_BUFFER;
  handle.global_id = id;
  GpuMemoryBufferFactoryHost::GetInstance()->DestroyGpuMemoryBuffer(handle,
                                                                    sync_point);
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
  handle.global_id = id_;
  return handle;
}

}  // namespace content
