// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_RASTER_INTERFACE_H_
#define GPU_COMMAND_BUFFER_CLIENT_RASTER_INTERFACE_H_

#include <GLES2/gl2.h>
#include "base/compiler_specific.h"
#include "components/viz/common/resources/resource_format.h"

namespace cc {
class DisplayItemList;
class ImageProvider;
struct RasterColorSpace;
}  // namespace cc

namespace gfx {
class Rect;
class Vector2d;
class Vector2dF;
}  // namespace gfx

extern "C" typedef struct _ClientBuffer* ClientBuffer;
extern "C" typedef struct _GLColorSpace* GLColorSpace;

namespace gpu {
namespace raster {

enum RasterTexStorageFlags { kNone = 0, kOverlay = (1 << 0) };

class RasterInterface {
 public:
  RasterInterface() {}
  virtual ~RasterInterface() {}

  // Texture objects.
  virtual void GenTextures(GLsizei n, GLuint* textures) = 0;
  virtual void BindTexture(GLenum target, GLuint texture) = 0;
  virtual void ActiveTexture(GLenum texture) = 0;
  virtual void GenerateMipmap(GLenum target) = 0;
  virtual void SetColorSpaceMetadataCHROMIUM(GLuint texture_id,
                                             GLColorSpace color_space) = 0;

  // Mailboxes.
  virtual void GenMailboxCHROMIUM(GLbyte* mailbox) = 0;
  virtual void ProduceTextureDirectCHROMIUM(GLuint texture,
                                            const GLbyte* mailbox) = 0;
  virtual GLuint CreateAndConsumeTextureCHROMIUM(const GLbyte* mailbox) = 0;

  // Image objects.
  virtual void BindTexImage2DCHROMIUM(GLenum target, GLint imageId) = 0;
  virtual void ReleaseTexImage2DCHROMIUM(GLenum target, GLint imageId) = 0;

  // Texture allocation and copying.
  virtual void TexImage2D(GLenum target,
                          GLint level,
                          GLint internalformat,
                          GLsizei width,
                          GLsizei height,
                          GLint border,
                          GLenum format,
                          GLenum type,
                          const void* pixels) = 0;
  virtual void TexSubImage2D(GLenum target,
                             GLint level,
                             GLint xoffset,
                             GLint yoffset,
                             GLsizei width,
                             GLsizei height,
                             GLenum format,
                             GLenum type,
                             const void* pixels) = 0;
  virtual void CompressedTexImage2D(GLenum target,
                                    GLint level,
                                    GLenum internalformat,
                                    GLsizei width,
                                    GLsizei height,
                                    GLint border,
                                    GLsizei imageSize,
                                    const void* data) = 0;
  virtual void TexStorageForRaster(GLenum target,
                                   viz::ResourceFormat format,
                                   GLsizei width,
                                   GLsizei height,
                                   RasterTexStorageFlags flags) = 0;
  virtual void CopySubTextureCHROMIUM(GLuint source_id,
                                      GLint source_level,
                                      GLenum dest_target,
                                      GLuint dest_id,
                                      GLint dest_level,
                                      GLint xoffset,
                                      GLint yoffset,
                                      GLint x,
                                      GLint y,
                                      GLsizei width,
                                      GLsizei height,
                                      GLboolean unpack_flip_y,
                                      GLboolean unpack_premultiply_alpha,
                                      GLboolean unpack_unmultiply_alpha) = 0;
  // OOP-Raster
  virtual void BeginRasterCHROMIUM(
      GLuint texture_id,
      GLuint sk_color,
      GLuint msaa_sample_count,
      GLboolean can_use_lcd_text,
      GLboolean use_distance_field_text,
      GLint pixel_config,
      const cc::RasterColorSpace& raster_color_space) = 0;
  virtual void RasterCHROMIUM(const cc::DisplayItemList* list,
                              cc::ImageProvider* provider,
                              const gfx::Vector2d& translate,
                              const gfx::Rect& playback_rect,
                              const gfx::Vector2dF& post_translate,
                              GLfloat post_scale) = 0;

  // Raster via GrContext.
  virtual void BeginGpuRaster() = 0;
  virtual void EndGpuRaster() = 0;

// Include the auto-generated part of this class. We split this because
// it means we can easily edit the non-auto generated parts right here in
// this file instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/raster_interface_autogen.h"
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_RASTER_INTERFACE_H_
