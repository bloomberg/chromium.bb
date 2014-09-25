// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl_io_surface.h"

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/logging.h"
#include "content/common/gpu/client/gpu_memory_buffer_factory_host.h"
#include "ui/gl/gl_bindings.h"

namespace content {
namespace {

base::StaticAtomicSequenceNumber g_next_buffer_id;

void Noop() {
}

void GpuMemoryBufferCreated(
    const gfx::Size& size,
    unsigned internalformat,
    const GpuMemoryBufferImpl::CreationCallback& callback,
    const gfx::GpuMemoryBufferHandle& handle) {
  DCHECK_EQ(gfx::IO_SURFACE_BUFFER, handle.type);

  callback.Run(GpuMemoryBufferImplIOSurface::CreateFromHandle(
      handle, size, internalformat, base::Bind(&Noop)));
}

void GpuMemoryBufferCreatedForChildProcess(
    const GpuMemoryBufferImpl::AllocationCallback& callback,
    const gfx::GpuMemoryBufferHandle& handle) {
  DCHECK_EQ(gfx::IO_SURFACE_BUFFER, handle.type);

  callback.Run(handle);
}

}  // namespace

GpuMemoryBufferImplIOSurface::GpuMemoryBufferImplIOSurface(
    const gfx::Size& size,
    unsigned internalformat,
    const DestructionCallback& callback,
    IOSurfaceRef io_surface)
    : GpuMemoryBufferImpl(size, internalformat, callback),
      io_surface_(io_surface) {
}

GpuMemoryBufferImplIOSurface::~GpuMemoryBufferImplIOSurface() {
}

// static
void GpuMemoryBufferImplIOSurface::Create(const gfx::Size& size,
                                          unsigned internalformat,
                                          unsigned usage,
                                          int client_id,
                                          const CreationCallback& callback) {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::IO_SURFACE_BUFFER;
  handle.global_id.primary_id = g_next_buffer_id.GetNext();
  handle.global_id.secondary_id = client_id;
  GpuMemoryBufferFactoryHost::GetInstance()->CreateGpuMemoryBuffer(
      handle,
      size,
      internalformat,
      usage,
      base::Bind(&GpuMemoryBufferCreated, size, internalformat, callback));
}

// static
void GpuMemoryBufferImplIOSurface::AllocateForChildProcess(
    const gfx::Size& size,
    unsigned internalformat,
    unsigned usage,
    int child_client_id,
    const AllocationCallback& callback) {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::IO_SURFACE_BUFFER;
  handle.global_id.primary_id = g_next_buffer_id.GetNext();
  handle.global_id.secondary_id = child_client_id;
  GpuMemoryBufferFactoryHost::GetInstance()->CreateGpuMemoryBuffer(
      handle,
      size,
      internalformat,
      usage,
      base::Bind(&GpuMemoryBufferCreatedForChildProcess, callback));
}

// static
scoped_ptr<GpuMemoryBufferImpl> GpuMemoryBufferImplIOSurface::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    unsigned internalformat,
    const DestructionCallback& callback) {
  DCHECK(IsFormatSupported(internalformat));

  base::ScopedCFTypeRef<IOSurfaceRef> io_surface(
      IOSurfaceLookup(handle.io_surface_id));
  if (!io_surface)
    return scoped_ptr<GpuMemoryBufferImpl>();

  return make_scoped_ptr<GpuMemoryBufferImpl>(new GpuMemoryBufferImplIOSurface(
      size, internalformat, callback, io_surface.get()));
}

// static
bool GpuMemoryBufferImplIOSurface::IsFormatSupported(unsigned internalformat) {
  switch (internalformat) {
    case GL_BGRA8_EXT:
      return true;
    default:
      return false;
  }
}

// static
bool GpuMemoryBufferImplIOSurface::IsUsageSupported(unsigned usage) {
  switch (usage) {
    case GL_IMAGE_MAP_CHROMIUM:
      return true;
    default:
      return false;
  }
}

// static
bool GpuMemoryBufferImplIOSurface::IsConfigurationSupported(
    unsigned internalformat,
    unsigned usage) {
  return IsFormatSupported(internalformat) && IsUsageSupported(usage);
}

// static
uint32 GpuMemoryBufferImplIOSurface::PixelFormat(unsigned internalformat) {
  switch (internalformat) {
    case GL_BGRA8_EXT:
      return 'BGRA';
    default:
      NOTREACHED();
      return 0;
  }
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
  handle.io_surface_id = IOSurfaceGetID(io_surface_);
  return handle;
}

}  // namespace content
