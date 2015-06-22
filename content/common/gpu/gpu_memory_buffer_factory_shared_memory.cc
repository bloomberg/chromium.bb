// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_buffer_factory_shared_memory.h"

#include <vector>

#include "base/logging.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl_shared_memory.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_image_shared_memory.h"

namespace content {
namespace {

const GpuMemoryBufferFactory::Configuration kSupportedConfigurations[] = {
    {gfx::GpuMemoryBuffer::R_8, gfx::GpuMemoryBuffer::MAP},
    {gfx::GpuMemoryBuffer::R_8, gfx::GpuMemoryBuffer::PERSISTENT_MAP},
    {gfx::GpuMemoryBuffer::RGBA_4444, gfx::GpuMemoryBuffer::MAP},
    {gfx::GpuMemoryBuffer::RGBA_4444, gfx::GpuMemoryBuffer::PERSISTENT_MAP},
    {gfx::GpuMemoryBuffer::RGBA_8888, gfx::GpuMemoryBuffer::MAP},
    {gfx::GpuMemoryBuffer::RGBA_8888, gfx::GpuMemoryBuffer::PERSISTENT_MAP},
    {gfx::GpuMemoryBuffer::BGRA_8888, gfx::GpuMemoryBuffer::MAP},
    {gfx::GpuMemoryBuffer::BGRA_8888, gfx::GpuMemoryBuffer::PERSISTENT_MAP},
    {gfx::GpuMemoryBuffer::YUV_420, gfx::GpuMemoryBuffer::MAP},
    {gfx::GpuMemoryBuffer::YUV_420, gfx::GpuMemoryBuffer::PERSISTENT_MAP}};

}  // namespace

GpuMemoryBufferFactorySharedMemory::GpuMemoryBufferFactorySharedMemory() {
}

GpuMemoryBufferFactorySharedMemory::~GpuMemoryBufferFactorySharedMemory() {
}

// static
bool GpuMemoryBufferFactorySharedMemory::
    IsGpuMemoryBufferConfigurationSupported(gfx::GpuMemoryBuffer::Format format,
                                            gfx::GpuMemoryBuffer::Usage usage) {
  for (auto& configuration : kSupportedConfigurations) {
    if (configuration.format == format && configuration.usage == usage)
      return true;
  }

  return false;
}

void GpuMemoryBufferFactorySharedMemory::
    GetSupportedGpuMemoryBufferConfigurations(
        std::vector<Configuration>* configurations) {
  configurations->assign(
      kSupportedConfigurations,
      kSupportedConfigurations + arraysize(kSupportedConfigurations));
}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferFactorySharedMemory::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::GpuMemoryBuffer::Format format,
    gfx::GpuMemoryBuffer::Usage usage,
    int client_id,
    gfx::PluginWindowHandle surface_handle) {
  size_t buffer_size = 0u;
  if (!GpuMemoryBufferImplSharedMemory::BufferSizeInBytes(size, format,
                                                          &buffer_size))
    return gfx::GpuMemoryBufferHandle();

  base::SharedMemory shared_memory;
  if (!shared_memory.CreateAnonymous(buffer_size))
    return gfx::GpuMemoryBufferHandle();

#if DCHECK_IS_ON()
  DCHECK(buffers_.insert(id).second);
#endif
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.id = id;
  shared_memory.ShareToProcess(base::GetCurrentProcessHandle(), &handle.handle);
  return handle;
}

void GpuMemoryBufferFactorySharedMemory::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id) {
#if DCHECK_IS_ON()
  DCHECK_EQ(buffers_.erase(id), 1u);
#endif
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
#if DCHECK_IS_ON()
  DCHECK_EQ(buffers_.count(handle.id), 1u);
#endif
  scoped_refptr<gfx::GLImageSharedMemory> image(
      new gfx::GLImageSharedMemory(size, internalformat));
  if (!image->Initialize(handle, format))
    return scoped_refptr<gfx::GLImage>();

  return image;
}

}  // namespace content
