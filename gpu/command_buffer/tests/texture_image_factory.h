// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/image_factory.h"

namespace gpu {

// The images created by this factory have no inherent storage. When the image
// is bound to a texture, storage is allocated for the texture via glTexImage2D.
class TextureImageFactory : public gpu::ImageFactory {
 public:
  scoped_refptr<gl::GLImage> CreateImageForGpuMemoryBuffer(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      unsigned internalformat,
      int client_id,
      SurfaceHandle surface_handle) override;
  scoped_refptr<gl::GLImage> CreateAnonymousImage(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      unsigned internalformat) override;
  unsigned RequiredTextureType() override;
  bool SupportsFormatRGB() override;

  void SetRequiredTextureType(unsigned type);

 private:
  unsigned required_texture_type_ = 0;
};

}  // namespace gpu
