// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_buffer_factory_shared_memory.h"

#include "base/logging.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_image_shared_memory.h"

namespace content {

GpuMemoryBufferFactorySharedMemory::GpuMemoryBufferFactorySharedMemory() {
}

GpuMemoryBufferFactorySharedMemory::~GpuMemoryBufferFactorySharedMemory() {
}

void GpuMemoryBufferFactorySharedMemory::
    GetSupportedGpuMemoryBufferConfigurations(
        std::vector<Configuration>* configurations) {
  const Configuration supported_configurations[] = {
    { gfx::GpuMemoryBuffer::RGBA_8888, gfx::GpuMemoryBuffer::MAP },
    { gfx::GpuMemoryBuffer::BGRA_8888, gfx::GpuMemoryBuffer::MAP }
  };
  configurations->assign(
      supported_configurations,
      supported_configurations + arraysize(supported_configurations));
}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferFactorySharedMemory::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::GpuMemoryBuffer::Format format,
    gfx::GpuMemoryBuffer::Usage usage,
    int client_id,
    gfx::PluginWindowHandle surface_handle) {
  base::SharedMemory shared_memory;

  size_t stride_in_bytes = 0;
  if (!GpuMemoryBufferImpl::StrideInBytes(
          size.width(), format, &stride_in_bytes))
    return gfx::GpuMemoryBufferHandle();

  base::CheckedNumeric<size_t> size_in_bytes = stride_in_bytes;
  size_in_bytes *= size.height();
  if (!size_in_bytes.IsValid())
    return gfx::GpuMemoryBufferHandle();

  if (!shared_memory.CreateAnonymous(size_in_bytes.ValueOrDie()))
    return gfx::GpuMemoryBufferHandle();

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.id = id;
  shared_memory.ShareToProcess(base::GetCurrentProcessHandle(), &handle.handle);
  return handle;
}

void GpuMemoryBufferFactorySharedMemory::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id) {
}

gpu::ImageFactory* GpuMemoryBufferFactorySharedMemory::AsImageFactory() {
  return this;
}

scoped_refptr<gfx::GLImage>
GpuMemoryBufferFactorySharedMemory::CreateImageForGpuMemoryBuffer(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::GpuMemoryBuffer::Format format,
    unsigned internalformat,
    int client_id) {
  scoped_refptr<gfx::GLImageSharedMemory> image(
      new gfx::GLImageSharedMemory(size, internalformat));
  if (!image->Initialize(handle, format))
    return scoped_refptr<gfx::GLImage>();

  return image;
}

}  // namespace content
