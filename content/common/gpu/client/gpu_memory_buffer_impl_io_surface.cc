// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl_io_surface.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/common/gpu/client/gpu_memory_buffer_factory_host.h"
#include "ui/gl/gl_bindings.h"

namespace content {
namespace {

void GpuMemoryBufferDeleted(gfx::GpuMemoryBufferId id,
                            int client_id,
                            uint32 sync_point) {
  GpuMemoryBufferFactoryHost::GetInstance()->DestroyGpuMemoryBuffer(
      gfx::IO_SURFACE_BUFFER, id, client_id, sync_point);
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

  DCHECK_EQ(gfx::IO_SURFACE_BUFFER, handle.type);
  callback.Run(GpuMemoryBufferImplIOSurface::CreateFromHandle(
      handle,
      size,
      format,
      base::Bind(&GpuMemoryBufferDeleted, handle.id, client_id)));
}

void GpuMemoryBufferCreatedForChildProcess(
    const GpuMemoryBufferImpl::AllocationCallback& callback,
    const gfx::GpuMemoryBufferHandle& handle) {
  DCHECK_IMPLIES(!handle.is_null(), gfx::IO_SURFACE_BUFFER == handle.type);

  callback.Run(handle);
}

}  // namespace

GpuMemoryBufferImplIOSurface::GpuMemoryBufferImplIOSurface(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    Format format,
    const DestructionCallback& callback,
    IOSurfaceRef io_surface)
    : GpuMemoryBufferImpl(id, size, format, callback), io_surface_(io_surface) {
}

GpuMemoryBufferImplIOSurface::~GpuMemoryBufferImplIOSurface() {
}

// static
void GpuMemoryBufferImplIOSurface::Create(gfx::GpuMemoryBufferId id,
                                          const gfx::Size& size,
                                          Format format,
                                          int client_id,
                                          const CreationCallback& callback) {
  GpuMemoryBufferFactoryHost::GetInstance()->CreateGpuMemoryBuffer(
      gfx::IO_SURFACE_BUFFER,
      id,
      size,
      format,
      MAP,
      client_id,
      base::Bind(&GpuMemoryBufferCreated, size, format, client_id, callback));
}

// static
void GpuMemoryBufferImplIOSurface::AllocateForChildProcess(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    Format format,
    int child_client_id,
    const AllocationCallback& callback) {
  GpuMemoryBufferFactoryHost::GetInstance()->CreateGpuMemoryBuffer(
      gfx::IO_SURFACE_BUFFER,
      id,
      size,
      format,
      MAP,
      child_client_id,
      base::Bind(&GpuMemoryBufferCreatedForChildProcess, callback));
}

// static
scoped_ptr<GpuMemoryBufferImpl> GpuMemoryBufferImplIOSurface::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    Format format,
    const DestructionCallback& callback) {
  DCHECK(IsFormatSupported(format));

  base::ScopedCFTypeRef<IOSurfaceRef> io_surface(
      IOSurfaceLookup(handle.io_surface_id));
  if (!io_surface)
    return scoped_ptr<GpuMemoryBufferImpl>();

  return make_scoped_ptr<GpuMemoryBufferImpl>(new GpuMemoryBufferImplIOSurface(
      handle.id, size, format, callback, io_surface.get()));
}

// static
void GpuMemoryBufferImplIOSurface::DeletedByChildProcess(
    gfx::GpuMemoryBufferId id,
    int child_client_id,
    uint32_t sync_point) {
  GpuMemoryBufferFactoryHost::GetInstance()->DestroyGpuMemoryBuffer(
      gfx::IO_SURFACE_BUFFER, id, child_client_id, sync_point);
}

// static
bool GpuMemoryBufferImplIOSurface::IsFormatSupported(Format format) {
  switch (format) {
    case BGRA_8888:
      return true;
    case RGBA_8888:
    case RGBX_8888:
      return false;
  }

  NOTREACHED();
  return false;
}

// static
bool GpuMemoryBufferImplIOSurface::IsUsageSupported(Usage usage) {
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
bool GpuMemoryBufferImplIOSurface::IsConfigurationSupported(Format format,
                                                            Usage usage) {
  return IsFormatSupported(format) && IsUsageSupported(usage);
}

// static
uint32 GpuMemoryBufferImplIOSurface::PixelFormat(Format format) {
  switch (format) {
    case BGRA_8888:
      return 'BGRA';
    case RGBA_8888:
    case RGBX_8888:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

void* GpuMemoryBufferImplIOSurface::Map() {
  DCHECK(!mapped_);
  IOSurfaceLock(io_surface_, 0, NULL);
  mapped_ = true;
  return IOSurfaceGetBaseAddress(io_surface_);
}

void GpuMemoryBufferImplIOSurface::Unmap() {
  DCHECK(mapped_);
  IOSurfaceUnlock(io_surface_, 0, NULL);
  mapped_ = false;
}

uint32 GpuMemoryBufferImplIOSurface::GetStride() const {
  return IOSurfaceGetBytesPerRow(io_surface_);
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplIOSurface::GetHandle() const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::IO_SURFACE_BUFFER;
  handle.id = id_;
  handle.io_surface_id = IOSurfaceGetID(io_surface_);
  return handle;
}

}  // namespace content
