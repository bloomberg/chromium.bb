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

void Noop() {
}

void GpuMemoryBufferCreated(
    const gfx::Size& size,
    unsigned internalformat,
    const GpuMemoryBufferImpl::CreationCallback& callback,
    const gfx::GpuMemoryBufferHandle& handle) {
  DCHECK_EQ(gfx::OZONE_NATIVE_BUFFER, handle.type);

  callback.Run(GpuMemoryBufferImplOzoneNativeBuffer::CreateFromHandle(
      handle, size, internalformat, base::Bind(&Noop)));
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
    unsigned internalformat,
    const DestructionCallback& callback,
    const gfx::GpuMemoryBufferId& id)
    : GpuMemoryBufferImpl(size, internalformat, callback), id_(id) {
}

GpuMemoryBufferImplOzoneNativeBuffer::~GpuMemoryBufferImplOzoneNativeBuffer() {
}

// static
void GpuMemoryBufferImplOzoneNativeBuffer::Create(
    const gfx::Size& size,
    unsigned internalformat,
    unsigned usage,
    int client_id,
    const CreationCallback& callback) {
  gfx::GpuMemoryBufferHandle handle;
  handle.global_id.primary_id = g_next_buffer_id.GetNext();
  handle.global_id.secondary_id = client_id;
  handle.type = gfx::OZONE_NATIVE_BUFFER;
  GpuMemoryBufferFactoryHost::GetInstance()->CreateGpuMemoryBuffer(
      handle,
      size,
      internalformat,
      usage,
      base::Bind(&GpuMemoryBufferCreated, size, internalformat, callback));
}

// static
void GpuMemoryBufferImplOzoneNativeBuffer::AllocateForChildProcess(
    const gfx::Size& size,
    unsigned internalformat,
    unsigned usage,
    int child_client_id,
    const AllocationCallback& callback) {
  gfx::GpuMemoryBufferHandle handle;
  handle.global_id.primary_id = g_next_buffer_id.GetNext();
  handle.global_id.secondary_id = child_client_id;
  handle.type = gfx::OZONE_NATIVE_BUFFER;
  GpuMemoryBufferFactoryHost::GetInstance()->CreateGpuMemoryBuffer(
      handle,
      size,
      internalformat,
      usage,
      base::Bind(&GpuMemoryBufferCreatedForChildProcess, callback));
}

// static
scoped_ptr<GpuMemoryBufferImpl>
GpuMemoryBufferImplOzoneNativeBuffer::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    unsigned internalformat,
    const DestructionCallback& callback) {
  DCHECK(IsFormatSupported(internalformat));

  return make_scoped_ptr<GpuMemoryBufferImpl>(
      new GpuMemoryBufferImplOzoneNativeBuffer(
          size, internalformat, callback, handle.global_id));
}

// static
bool GpuMemoryBufferImplOzoneNativeBuffer::IsFormatSupported(
    unsigned internalformat) {
  switch (internalformat) {
    case GL_RGBA8_OES:
    case GL_RGB8_OES:
      return true;
    default:
      return false;
  }
}

// static
bool GpuMemoryBufferImplOzoneNativeBuffer::IsUsageSupported(unsigned usage) {
  switch (usage) {
    case GL_IMAGE_SCANOUT_CHROMIUM:
      return true;
    default:
      return false;
  }
}

// static
bool GpuMemoryBufferImplOzoneNativeBuffer::IsConfigurationSupported(
    unsigned internalformat,
    unsigned usage) {
  return IsFormatSupported(internalformat) && IsUsageSupported(usage);
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
