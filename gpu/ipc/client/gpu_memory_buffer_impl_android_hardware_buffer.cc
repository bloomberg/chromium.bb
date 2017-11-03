// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/gpu_memory_buffer_impl_android_hardware_buffer.h"

#include <utility>

#include "base/android/android_hardware_buffer_compat.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory_handle.h"
#include "gpu/ipc/client/gpu_memory_buffer_impl_android_hardware_buffer.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {

namespace {

AHardwareBuffer_Desc GetBufferDescription(const gfx::Size& size,
                                          gfx::BufferFormat format,
                                          gfx::BufferUsage usage) {
  // On create, all elements must be initialized, including setting the
  // "reserved for future use" (rfu) fields to zero.
  AHardwareBuffer_Desc desc = {};
  desc.width = size.width();
  desc.height = size.height();
  desc.layers = 1;  // number of images

  switch (format) {
    case gfx::BufferFormat::RGBA_8888:
      desc.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
      break;
    case gfx::BufferFormat::RGBX_8888:
      desc.format = AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM;
      break;
    default:
      NOTREACHED();
  }

  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
    case gfx::BufferUsage::SCANOUT:
      desc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
                   AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT;
      break;
    default:
      NOTREACHED();
  }
  return desc;
}

}  // namespace

GpuMemoryBufferImplAndroidHardwareBuffer::
    GpuMemoryBufferImplAndroidHardwareBuffer(
        gfx::GpuMemoryBufferId id,
        const gfx::Size& size,
        gfx::BufferFormat format,
        const DestructionCallback& callback,
        const base::SharedMemoryHandle& handle)
    : GpuMemoryBufferImpl(id, size, format, callback),
      shared_memory_(handle, false) {}

GpuMemoryBufferImplAndroidHardwareBuffer::
    ~GpuMemoryBufferImplAndroidHardwareBuffer() {}

// static
std::unique_ptr<GpuMemoryBufferImplAndroidHardwareBuffer>
GpuMemoryBufferImplAndroidHardwareBuffer::Create(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    const DestructionCallback& callback) {
  DCHECK(base::AndroidHardwareBufferCompat::IsSupportAvailable());

  AHardwareBuffer* buffer = nullptr;
  AHardwareBuffer_Desc desc = GetBufferDescription(size, format, usage);
  base::AndroidHardwareBufferCompat::GetInstance().Allocate(&desc, &buffer);
  if (!buffer) {
    LOG(ERROR) << "Failed to allocate AHardwareBuffer";
    return nullptr;
  }

  return base::WrapUnique(new GpuMemoryBufferImplAndroidHardwareBuffer(
      id, size, format, callback,
      base::SharedMemoryHandle(buffer, 0, base::UnguessableToken::Create())));
}

// static
std::unique_ptr<GpuMemoryBufferImplAndroidHardwareBuffer>
GpuMemoryBufferImplAndroidHardwareBuffer::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    const DestructionCallback& callback) {
  DCHECK(base::SharedMemory::IsHandleValid(handle.handle));
  return base::WrapUnique(new GpuMemoryBufferImplAndroidHardwareBuffer(
      handle.id, size, format, callback, handle.handle));
}

// static
bool GpuMemoryBufferImplAndroidHardwareBuffer::IsConfigurationSupported(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  return gpu::IsNativeGpuMemoryBufferConfigurationSupported(format, usage);
}

bool GpuMemoryBufferImplAndroidHardwareBuffer::Map() {
  return false;
}

void* GpuMemoryBufferImplAndroidHardwareBuffer::memory(size_t plane) {
  return nullptr;
}

void GpuMemoryBufferImplAndroidHardwareBuffer::Unmap() {}

int GpuMemoryBufferImplAndroidHardwareBuffer::stride(size_t plane) const {
  return 0;
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplAndroidHardwareBuffer::GetHandle()
    const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::ANDROID_HARDWARE_BUFFER;
  handle.id = id_;
  handle.handle = shared_memory_.handle();

  return handle;
}

// static
base::Closure GpuMemoryBufferImplAndroidHardwareBuffer::AllocateForTesting(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gfx::GpuMemoryBufferHandle* handle) {
  DCHECK(IsConfigurationSupported(format, usage));
  gfx::GpuMemoryBufferId kBufferId(1);
  handle->type = gfx::ANDROID_HARDWARE_BUFFER;
  handle->id = kBufferId;
  AHardwareBuffer* buffer = nullptr;
  AHardwareBuffer_Desc desc = GetBufferDescription(size, format, usage);
  base::AndroidHardwareBufferCompat::GetInstance().Allocate(&desc, &buffer);
  DCHECK(buffer);
  handle->handle =
      base::SharedMemoryHandle(buffer, 0, base::UnguessableToken::Create());
  return base::Bind(&base::DoNothing);
}

}  // namespace gpu
