// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_RASTER_INTERFACE_H_
#define GPU_COMMAND_BUFFER_CLIENT_RASTER_INTERFACE_H_

#include <GLES2/gl2.h>
#include "base/compiler_specific.h"
#include "components/viz/common/resources/resource_format.h"
#include "ui/gfx/buffer_types.h"

namespace cc {
class DisplayItemList;
class ImageProvider;
struct RasterColorSpace;
}  // namespace cc

namespace gfx {
class Rect;
class Size;
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
  virtual GLuint CreateTexture(bool use_buffer,
                               gfx::BufferUsage buffer_usage,
                               viz::ResourceFormat format) = 0;
  virtual void SetColorSpaceMetadata(GLuint texture_id,
                                     GLColorSpace color_space) = 0;

  // Mailboxes.
  virtual void GenMailbox(GLbyte* mailbox) = 0;
  virtual void ProduceTextureDirect(GLuint texture, const GLbyte* mailbox) = 0;
  virtual GLuint CreateAndConsumeTexture(bool use_buffer,
                                         gfx::BufferUsage buffer_usage,
                                         viz::ResourceFormat format,
                                         const GLbyte* mailbox) = 0;

  // Image objects.
  virtual void BindTexImage2DCHROMIUM(GLuint texture_id, GLint image_id) = 0;
  virtual void ReleaseTexImage2DCHROMIUM(GLuint texture_id, GLint image_id) = 0;

  // Texture allocation and copying.
  virtual void TexStorage2D(GLuint texture_id,
                            GLint levels,
                            GLsizei width,
                            GLsizei height) = 0;
  virtual void CopySubTexture(GLuint source_id,
                              GLuint dest_id,
                              GLint xoffset,
                              GLint yoffset,
                              GLint x,
                              GLint y,
                              GLsizei width,
                              GLsizei height) = 0;

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
                              const gfx::Size& content_size,
                              const gfx::Rect& full_raster_rect,
                              const gfx::Rect& playback_rect,
                              const gfx::Vector2dF& post_translate,
                              GLfloat post_scale,
                              bool requires_clear) = 0;

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
