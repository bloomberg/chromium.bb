// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl_ozone_native_buffer.h"

#include "base/bind.h"
#include "content/common/gpu/client/gpu_memory_buffer_factory_host.h"
#include "ui/gl/gl_bindings.h"

namespace content {
namespace {

void GpuMemoryBufferDeleted(gfx::GpuMemoryBufferId id,
                            int client_id,
                            uint32 sync_point) {
  GpuMemoryBufferFactoryHost::GetInstance()->DestroyGpuMemoryBuffer(
      gfx::OZONE_NATIVE_BUFFER, id, client_id, sync_point);
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

  DCHECK_EQ(gfx::OZONE_NATIVE_BUFFER, handle.type);
  callback.Run(GpuMemoryBufferImplOzoneNativeBuffer::CreateFromHandle(
      handle,
      size,
      format,
      base::Bind(&GpuMemoryBufferDeleted, handle.id, client_id)));
}

void GpuMemoryBufferCreatedForChildProcess(
    const GpuMemoryBufferImpl::AllocationCallback& callback,
    const gfx::GpuMemoryBufferHandle& handle) {
  DCHECK_IMPLIES(!handle.is_null(), gfx::OZONE_NATIVE_BUFFER == handle.type);

  callback.Run(handle);
}

}  // namespace

GpuMemoryBufferImplOzoneNativeBuffer::GpuMemoryBufferImplOzoneNativeBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    Format format,
    const DestructionCallback& callback)
    : GpuMemoryBufferImpl(id, size, format, callback) {
}

GpuMemoryBufferImplOzoneNativeBuffer::~GpuMemoryBufferImplOzoneNativeBuffer() {
}

// static
void GpuMemoryBufferImplOzoneNativeBuffer::Create(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    Format format,
    int client_id,
    const CreationCallback& callback) {
  GpuMemoryBufferFactoryHost::GetInstance()->CreateGpuMemoryBuffer(
      gfx::OZONE_NATIVE_BUFFER,
      id,
      size,
      format,
      SCANOUT,
      client_id,
      base::Bind(&GpuMemoryBufferCreated, size, format, client_id, callback));
}

// static
void GpuMemoryBufferImplOzoneNativeBuffer::AllocateForChildProcess(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    Format format,
    int child_client_id,
    const AllocationCallback& callback) {
  GpuMemoryBufferFactoryHost::GetInstance()->CreateGpuMemoryBuffer(
      gfx::OZONE_NATIVE_BUFFER,
      id,
      size,
      format,
      SCANOUT,
      child_client_id,
      base::Bind(&GpuMemoryBufferCreatedForChildProcess, callback));
}

// static
scoped_ptr<GpuMemoryBufferImpl>
GpuMemoryBufferImplOzoneNativeBuffer::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    Format format,
    const DestructionCallback& callback) {
  DCHECK(IsFormatSupported(format));

  return make_scoped_ptr<GpuMemoryBufferImpl>(
      new GpuMemoryBufferImplOzoneNativeBuffer(
          handle.id, size, format, callback));
}

// static
void GpuMemoryBufferImplOzoneNativeBuffer::DeletedByChildProcess(
    gfx::GpuMemoryBufferId id,
    int child_client_id,
    uint32_t sync_point) {
  GpuMemoryBufferFactoryHost::GetInstance()->DestroyGpuMemoryBuffer(
      gfx::OZONE_NATIVE_BUFFER, id, child_client_id, sync_point);
}

// static
bool GpuMemoryBufferImplOzoneNativeBuffer::IsFormatSupported(Format format) {
  switch (format) {
    case RGBA_8888:
    case RGBX_8888:
      return true;
    case BGRA_8888:
      return false;
  }

  NOTREACHED();
  return false;
}

// static
bool GpuMemoryBufferImplOzoneNativeBuffer::IsUsageSupported(Usage usage) {
  switch (usage) {
    case MAP:
      return false;
    case SCANOUT:
      return true;
  }

  NOTREACHED();
  return false;
}

// static
bool GpuMemoryBufferImplOzoneNativeBuffer::IsConfigurationSupported(
    Format format,
    Usage usage) {
  return IsFormatSupported(format) && IsUsageSupported(usage);
}

void* GpuMemoryBufferImplOzoneNativeBuffer::Map() {
  NOTREACHED();
  return NULL;
}

void GpuMemoryBufferImplOzoneNativeBuffer::Unmap() {
  NOTREACHED();
}

uint32 GpuMemoryBufferImplOzoneNativeBuffer::GetStride() const {
  NOTREACHED();
  return 0;
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplOzoneNativeBuffer::GetHandle()
    const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::OZONE_NATIVE_BUFFER;
  handle.id = id_;
  return handle;
}

}  // namespace content
