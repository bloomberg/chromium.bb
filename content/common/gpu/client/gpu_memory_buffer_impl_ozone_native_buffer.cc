// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl_ozone_native_buffer.h"

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
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
  DCHECK_EQ(gfx::OZONE_NATIVE_BUFFER, handle.type);

  callback.Run(GpuMemoryBufferImplOzoneNativeBuffer::CreateFromHandle(
      handle, size, format, base::Bind(&GpuMemoryBufferDeleted, handle)));
}

void GpuMemoryBufferCreatedForChildProcess(
    const GpuMemoryBufferImpl::AllocationCallback& callback,
    const gfx::GpuMemoryBufferHandle& handle) {
  DCHECK_EQ(gfx::OZONE_NATIVE_BUFFER, handle.type);

  callback.Run(handle);
}

}  // namespace

GpuMemoryBufferImplOzoneNativeBuffer::GpuMemoryBufferImplOzoneNativeBuffer(
    const gfx::Size& size,
    Format format,
    const DestructionCallback& callback,
    const gfx::GpuMemoryBufferId& id)
    : GpuMemoryBufferImpl(size, format, callback), id_(id) {
}

GpuMemoryBufferImplOzoneNativeBuffer::~GpuMemoryBufferImplOzoneNativeBuffer() {
}

// static
void GpuMemoryBufferImplOzoneNativeBuffer::Create(
    const gfx::Size& size,
    Format format,
    int client_id,
    const CreationCallback& callback) {
  gfx::GpuMemoryBufferHandle handle;
  handle.global_id.primary_id = g_next_buffer_id.GetNext();
  handle.global_id.secondary_id = client_id;
  handle.type = gfx::OZONE_NATIVE_BUFFER;
  GpuMemoryBufferFactoryHost::GetInstance()->CreateGpuMemoryBuffer(
      handle,
      size,
      format,
      SCANOUT,
      base::Bind(&GpuMemoryBufferCreated, size, format, callback));
}

// static
void GpuMemoryBufferImplOzoneNativeBuffer::AllocateForChildProcess(
    const gfx::Size& size,
    Format format,
    int child_client_id,
    const AllocationCallback& callback) {
  gfx::GpuMemoryBufferHandle handle;
  handle.global_id.primary_id = g_next_buffer_id.GetNext();
  handle.global_id.secondary_id = child_client_id;
  handle.type = gfx::OZONE_NATIVE_BUFFER;
  GpuMemoryBufferFactoryHost::GetInstance()->CreateGpuMemoryBuffer(
      handle,
      size,
      format,
      SCANOUT,
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
          size, format, callback, handle.global_id));
}

// static
void GpuMemoryBufferImplOzoneNativeBuffer::DeletedByChildProcess(
    const gfx::GpuMemoryBufferId& id,
    uint32_t sync_point) {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::OZONE_NATIVE_BUFFER;
  handle.global_id = id;
  GpuMemoryBufferFactoryHost::GetInstance()->DestroyGpuMemoryBuffer(handle,
                                                                    sync_point);
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
  handle.global_id = id_;
  return handle;
}

}  // namespace content
