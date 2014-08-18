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

}  // namespace

GpuMemoryBufferImplOzoneNativeBuffer::GpuMemoryBufferImplOzoneNativeBuffer(
    const gfx::Size& size,
    unsigned internalformat)
    : GpuMemoryBufferImpl(size, internalformat) {
}

GpuMemoryBufferImplOzoneNativeBuffer::~GpuMemoryBufferImplOzoneNativeBuffer() {
}

// static
void GpuMemoryBufferImplOzoneNativeBuffer::AllocateOzoneNativeBufferForChildId(
    const gfx::Size& size,
    unsigned internalformat,
    unsigned usage,
    int child_id,
    const AllocationCallback& callback) {
  gfx::GpuMemoryBufferHandle handle;
  // +1 ensures we always get non-zero IDs.
  handle.global_id.primary_id = g_next_buffer_id.GetNext() + 1;
  handle.global_id.secondary_id = child_id;
  handle.type = gfx::OZONE_NATIVE_BUFFER;
  GpuMemoryBufferFactoryHost::GetInstance()->CreateGpuMemoryBuffer(
      handle,
      size,
      internalformat,
      usage,
      base::Bind(&OnGpuMemoryBufferCreated, callback));
}

// static
bool GpuMemoryBufferImplOzoneNativeBuffer::IsFormatSupported(
    unsigned internalformat) {
  switch (internalformat) {
    case GL_RGBA8_OES:
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

bool GpuMemoryBufferImplOzoneNativeBuffer::InitializeFromHandle(
    const gfx::GpuMemoryBufferHandle& handle) {
  id_ = handle.global_id;
  return true;
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

void GpuMemoryBufferImplOzoneNativeBuffer::OnGpuMemoryBufferCreated(
    const AllocationCallback& callback,
    const gfx::GpuMemoryBufferHandle& handle) {
  callback.Run(handle);
}

}  // namespace content
