// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_buffer_factory_surface_texture.h"

#include "content/common/android/surface_texture_manager.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_image_surface_texture.h"

namespace content {

GpuMemoryBufferFactorySurfaceTexture::GpuMemoryBufferFactorySurfaceTexture() {
}

GpuMemoryBufferFactorySurfaceTexture::~GpuMemoryBufferFactorySurfaceTexture() {
}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferFactorySurfaceTexture::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    unsigned internalformat,
    int client_id) {
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

  SurfaceTextureMapKey key(id, client_id);
  DCHECK(surface_textures_.find(key) == surface_textures_.end());
  surface_textures_[key] = surface_texture;

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SURFACE_TEXTURE_BUFFER;
  handle.id = id;
  return handle;
}

void GpuMemoryBufferFactorySurfaceTexture::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id) {
  SurfaceTextureMapKey key(id, client_id);
  SurfaceTextureMap::iterator it = surface_textures_.find(key);
  if (it != surface_textures_.end())
    surface_textures_.erase(it);

  SurfaceTextureManager::GetInstance()->UnregisterSurfaceTexture(id, client_id);
}

scoped_refptr<gfx::GLImage>
GpuMemoryBufferFactorySurfaceTexture::CreateImageForGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    unsigned internalformat,
    int client_id) {
  SurfaceTextureMapKey key(id, client_id);
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
