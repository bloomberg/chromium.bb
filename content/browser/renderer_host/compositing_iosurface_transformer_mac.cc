// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositing_iosurface_transformer_mac.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "content/browser/renderer_host/compositing_iosurface_shader_programs_mac.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace content {

namespace {

// Simple auto-delete scoping support for an owned Framebuffer object.
class ScopedFramebuffer {
 public:
  ScopedFramebuffer() {
    glGenFramebuffersEXT(1, &name_);
  }

  ~ScopedFramebuffer() {
    if (name_ != 0u)
      glDeleteFramebuffersEXT(1, &name_);
  }

  bool is_valid() const { return name_ != 0u; }
  GLuint name() const { return name_; }

 private:
  GLuint name_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFramebuffer);
};

// Simple auto-delete scoping support for an owned texture object.
class ScopedTexture {
 public:
  ScopedTexture() : name_(0u) {}
  ScopedTexture(GLenum target, const gfx::Size& size);

  ~ScopedTexture() {
    if (name_ != 0u)
      glDeleteTextures(1, &name_);
  }

  bool is_valid() const { return name_ != 0u; }
  GLuint name() const { return name_; }

  void Reset(GLuint texture) {
    if (name_ != 0u)
      glDeleteTextures(1, &name_);
    name_ = texture;
  }

  GLuint Release() {
    GLuint ret = name_;
    name_ = 0u;
    return ret;
  }

 private:
  GLuint name_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTexture);
};

ScopedTexture::ScopedTexture(GLenum target, const gfx::Size& size) {
  glGenTextures(1, &name_);
  glBindTexture(target, name_);
  glTexImage2D(target, 0, GL_RGBA, size.width(), size.height(), 0, GL_BGRA,
               GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
  DCHECK(glGetError() == GL_NO_ERROR);
  glBindTexture(target, 0u);
}

// Set viewport and model/projection matrices for drawing to a framebuffer of
// size dst_size, with coordinates starting at (0, 0).
void SetTransformationsForOffScreenRendering(const gfx::Size& dst_size) {
  glViewport(0, 0, dst_size.width(), dst_size.height());
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, dst_size.width(), 0, dst_size.height(), -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

// Configure texture sampling parameters.
void SetTextureParameters(GLenum target, GLint min_mag_filter, GLint wrap) {
  glTexParameteri(target, GL_TEXTURE_MIN_FILTER, min_mag_filter);
  glTexParameteri(target, GL_TEXTURE_MAG_FILTER, min_mag_filter);
  glTexParameteri(target, GL_TEXTURE_WRAP_S, wrap);
  glTexParameteri(target, GL_TEXTURE_WRAP_T, wrap);
}

// Draw the currently-bound texture.  The src region is applied to the entire
// destination framebuffer of the given size.  Specify |flip_y| is the src
// texture is upside-down relative to the destination.
//
// Assumption: The orthographic projection is set up as
// (0,0)x(dst_width,dst_height).
void DrawQuad(float src_x, float src_y, float src_width, float src_height,
              bool flip_y, float dst_width, float dst_height) {
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);

  float vertices[4][2] = {
    { 0.0f, dst_height },
    { 0.0f, 0.0f },
    { dst_width, 0.0f },
    { dst_width, dst_height }
  };
  glVertexPointer(arraysize(vertices[0]), GL_FLOAT, sizeof(vertices[0]),
                  vertices);

  float tex_coords[4][2] = {
    { src_x, src_y + src_height },
    { src_x, src_y },
    { src_x + src_width, src_y },
    { src_x + src_width, src_y + src_height }
  };
  if (flip_y) {
    std::swap(tex_coords[0][1], tex_coords[1][1]);
    std::swap(tex_coords[2][1], tex_coords[3][1]);
  }
  glTexCoordPointer(arraysize(tex_coords[0]), GL_FLOAT, sizeof(tex_coords[0]),
                    tex_coords);

  COMPILE_ASSERT(arraysize(vertices) == arraysize(tex_coords),
                 same_number_of_points_in_both);
  glDrawArrays(GL_QUADS, 0, arraysize(vertices));

  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

}  // namespace

CompositingIOSurfaceTransformer::CompositingIOSurfaceTransformer(
    GLenum texture_target, bool src_texture_needs_y_flip,
    CompositingIOSurfaceShaderPrograms* shader_program_cache)
    : texture_target_(texture_target),
      src_texture_needs_y_flip_(src_texture_needs_y_flip),
      shader_program_cache_(shader_program_cache) {
  DCHECK(texture_target_ == GL_TEXTURE_RECTANGLE_ARB)
      << "Fragment shaders currently only support RECTANGLE textures.";
  DCHECK(shader_program_cache_);

  // The RGB-to-YV12 transform requires that the driver/hardware supports
  // multiple draw buffers.
  GLint max_draw_buffers = 1;
  glGetIntegerv(GL_MAX_DRAW_BUFFERS, &max_draw_buffers);
  system_supports_multiple_draw_buffers_ = (max_draw_buffers >= 2);
}

CompositingIOSurfaceTransformer::~CompositingIOSurfaceTransformer() {
}

bool CompositingIOSurfaceTransformer::ResizeBilinear(
    GLuint src_texture, const gfx::Rect& src_subrect, const gfx::Size& dst_size,
    GLuint* texture) {
  if (src_subrect.IsEmpty() || dst_size.IsEmpty())
    return false;

  glActiveTexture(GL_TEXTURE0);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);

  ScopedTexture dst_texture(texture_target_, dst_size);
  if (!dst_texture.is_valid())
    return false;

  ScopedFramebuffer temp_frame_buffer;
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, temp_frame_buffer.name());
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                            texture_target_, dst_texture.name(), 0);
  DCHECK(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) ==
             GL_FRAMEBUFFER_COMPLETE_EXT);

  glBindTexture(texture_target_, src_texture);
  SetTextureParameters(
      texture_target_, src_subrect.size() == dst_size ? GL_NEAREST : GL_LINEAR,
      GL_CLAMP_TO_EDGE);

  const bool prepared = shader_program_cache_->UseBlitProgram();
  DCHECK(prepared);
  SetTransformationsForOffScreenRendering(dst_size);
  DrawQuad(src_subrect.x(), src_subrect.y(),
           src_subrect.width(), src_subrect.height(),
           src_texture_needs_y_flip_,
           dst_size.width(), dst_size.height());
  glUseProgram(0);
  glBindTexture(texture_target_, 0u);

  *texture = dst_texture.Release();
  return true;
}

bool CompositingIOSurfaceTransformer::TransformRGBToYV12(
    GLuint src_texture,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    GLuint* texture_y,
    GLuint* texture_u,
    GLuint* texture_v,
    gfx::Size* packed_y_size,
    gfx::Size* packed_uv_size) {
  if (!system_supports_multiple_draw_buffers_)
    return false;
  if (src_subrect.IsEmpty() || dst_size.IsEmpty())
    return false;

  TRACE_EVENT0("gpu", "TransformRGBToYV12");

  glActiveTexture(GL_TEXTURE0);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);

  // Allocate output textures for each plane, and the temporary one for the UUVV
  // that becomes an input into pass #2.  |packed_y_size| is the size of the Y
  // output texture, where its width is 1/4 the number of Y pixels because 4 Y
  // pixels are packed into a single quad.  |packed_uv_size| is half the size of
  // Y in both dimensions, rounded up.
  *packed_y_size = gfx::Size((dst_size.width() + 3) / 4, dst_size.height());
  *packed_uv_size = gfx::Size((packed_y_size->width() + 1) / 2,
                              (packed_y_size->height() + 1) / 2);
  ScopedTexture temp_texture_y(texture_target_, *packed_y_size);
  if (!temp_texture_y.is_valid())
    return false;
  ScopedTexture temp_texture_u(texture_target_, *packed_uv_size);
  if (!temp_texture_u.is_valid())
    return false;
  ScopedTexture temp_texture_v(texture_target_, *packed_uv_size);
  if (!temp_texture_v.is_valid())
    return false;

  // Create a temporary texture for the UUVV that becomes an input into pass #2.
  ScopedTexture temp_texture_uuvv(texture_target_, *packed_y_size);
  if (!temp_texture_uuvv.is_valid())
    return false;

  // Create a temporary FBO for writing to the textures off-screen.
  ScopedFramebuffer temp_frame_buffer;
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, temp_frame_buffer.name());

  /////////////////////////////////////////
  // Pass 1: RGB --(scaled)--> YYYY + UUVV
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                            texture_target_, temp_texture_y.name(), 0);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT,
                            texture_target_, temp_texture_uuvv.name(), 0);
  DCHECK(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) ==
             GL_FRAMEBUFFER_COMPLETE_EXT);
  static const GLenum kAttachments[] =
      { GL_COLOR_ATTACHMENT0_EXT, GL_COLOR_ATTACHMENT1_EXT };
  glDrawBuffers(2, kAttachments);

  // Read from |src_texture|.  Enable bilinear filtering only if scaling is
  // required.  The filtering will take place entirely in the first pass.
  glBindTexture(texture_target_, src_texture);
  SetTextureParameters(
      texture_target_, src_subrect.size() == dst_size ? GL_NEAREST : GL_LINEAR,
      GL_CLAMP_TO_EDGE);

  // Use the first-pass shader program and draw the scene.
  const bool prepared_pass_1 = shader_program_cache_->UseRGBToYV12Program(
      1,
      static_cast<float>(src_subrect.width()) / dst_size.width());
  DCHECK(prepared_pass_1);
  SetTransformationsForOffScreenRendering(*packed_y_size);
  DrawQuad(src_subrect.x(), src_subrect.y(),
           ((packed_y_size->width() * 4.0f) / dst_size.width()) *
               src_subrect.width(),
           src_subrect.height(),
           src_texture_needs_y_flip_,
           packed_y_size->width(), packed_y_size->height());

  /////////////////////////////////////////
  // Pass 2: UUVV -> UUUU + VVVV
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                            texture_target_, temp_texture_u.name(), 0);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT,
                            texture_target_, temp_texture_v.name(), 0);
  DCHECK(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) ==
             GL_FRAMEBUFFER_COMPLETE_EXT);

  // Read from texture_uuvv.  The second pass uses bilinear minification to
  // achieve vertical scaling, so enable it always.
  glBindTexture(texture_target_, temp_texture_uuvv.name());
  SetTextureParameters(texture_target_, GL_LINEAR, GL_CLAMP_TO_EDGE);

  // Use the second-pass shader program and draw the scene.
  const bool prepared_pass_2 =
      shader_program_cache_->UseRGBToYV12Program(2, 1.0f);
  DCHECK(prepared_pass_2);
  SetTransformationsForOffScreenRendering(*packed_uv_size);
  DrawQuad(0.0f, 0.0f,
           packed_uv_size->width() * 2.0f,
           packed_uv_size->height() * 2.0f,
           false,
           packed_uv_size->width(), packed_uv_size->height());
  glUseProgram(0);
  glBindTexture(texture_target_, 0);

  // Before leaving, put back to drawing to a single rendering output.
  glDrawBuffers(1, kAttachments);

  *texture_y = temp_texture_y.Release();
  *texture_u = temp_texture_u.Release();
  *texture_v = temp_texture_v.Release();
  return true;
}

}  // namespace content
