// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_buffer_factory_surface_texture.h"

#include "content/common/android/surface_texture_manager.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_image_surface_texture.h"

namespace content {
namespace {

const GpuMemoryBufferFactory::Configuration kSupportedConfigurations[] = {
  { gfx::GpuMemoryBuffer::RGBA_8888, gfx::GpuMemoryBuffer::MAP }
};

}  // namespace

GpuMemoryBufferFactorySurfaceTexture::GpuMemoryBufferFactorySurfaceTexture() {
}

GpuMemoryBufferFactorySurfaceTexture::~GpuMemoryBufferFactorySurfaceTexture() {
}

// static
bool GpuMemoryBufferFactorySurfaceTexture::
    IsGpuMemoryBufferConfigurationSupported(gfx::GpuMemoryBuffer::Format format,
                                            gfx::GpuMemoryBuffer::Usage usage) {
  for (auto& configuration : kSupportedConfigurations) {
    if (configuration.format == format && configuration.usage == usage)
      return true;
  }

  return false;
}

void GpuMemoryBufferFactorySurfaceTexture::
    GetSupportedGpuMemoryBufferConfigurations(
        std::vector<Configuration>* configurations) {
  configurations->assign(
      kSupportedConfigurations,
      kSupportedConfigurations + arraysize(kSupportedConfigurations));
}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferFactorySurfaceTexture::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::GpuMemoryBuffer::Format format,
    gfx::GpuMemoryBuffer::Usage usage,
    int client_id,
    gfx::PluginWindowHandle surface_handle) {
  // Note: this needs to be 0 as the surface texture implemenation will take
  // ownership of the texture and call glDeleteTextures when the GPU service
  // attaches the surface texture to a real texture id. glDeleteTextures
  // silently ignores 0.
  const int kDummyTextureId = 0;
  scoped_refptr<gfx::SurfaceTexture> surface_texture =
      gfx::SurfaceTexture::Create(kDummyTextureId);
  if (!surface_texture.get())
    return gfx::GpuMemoryBufferHandle();

  SurfaceTextureManager::GetInstance()->RegisterSurfaceTexture(
      id, client_id, surface_texture.get());

  {
    base::AutoLock lock(surface_textures_lock_);

    SurfaceTextureMapKey key(id, client_id);
    DCHECK(surface_textures_.find(key) == surface_textures_.end());
    surface_textures_[key] = surface_texture;
  }

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SURFACE_TEXTURE_BUFFER;
  handle.id = id;
  return handle;
}

void GpuMemoryBufferFactorySurfaceTexture::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id) {
  {
    base::AutoLock lock(surface_textures_lock_);

    SurfaceTextureMapKey key(id, client_id);
    SurfaceTextureMap::iterator it = surface_textures_.find(key);
    if (it != surface_textures_.end())
      surface_textures_.erase(it);
  }

  SurfaceTextureManager::GetInstance()->UnregisterSurfaceTexture(id, client_id);
}

gpu::ImageFactory* GpuMemoryBufferFactorySurfaceTexture::AsImageFactory() {
  return this;
}

scoped_refptr<gfx::GLImage>
GpuMemoryBufferFactorySurfaceTexture::CreateImageForGpuMemoryBuffer(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::GpuMemoryBuffer::Format format,
    unsigned internalformat,
    int client_id) {
  base::AutoLock lock(surface_textures_lock_);

  DCHECK_EQ(handle.type, gfx::SURFACE_TEXTURE_BUFFER);

  SurfaceTextureMapKey key(handle.id, client_id);
  SurfaceTextureMap::iterator it = surface_textures_.find(key);
  if (it == surface_textures_.end())
    return scoped_refptr<gfx::GLImage>();

  scoped_refptr<gfx::GLImageSurfaceTexture> image(
      new gfx::GLImageSurfaceTexture(size));
  if (!image->Initialize(it->second.get()))
    return scoped_refptr<gfx::GLImage>();

  return image;
}

}  // namespace content
