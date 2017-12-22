// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/scoped_render_pass_texture.h"

#include "base/bits.h"
#include "base/logging.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"

namespace viz {

ScopedRenderPassTexture::ScopedRenderPassTexture(
    gpu::gles2::GLES2Interface* gl,
    const gfx::Size& size,
    ResourceTextureHint hint,
    ResourceFormat format,
    const gfx::ColorSpace& color_space,
    bool use_texture_usage_hint,
    bool use_texture_storage,
    bool use_texture_npot)
    : gl_(gl), size_(size), hint_(hint), color_space_(color_space) {
  DCHECK(gl_);
  gl_->GenTextures(1, &gl_id_);

  // Create and set texture properties.
  gl_->BindTexture(GL_TEXTURE_2D, gl_id_);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  if (use_texture_usage_hint && (hint_ & ResourceTextureHint::kFramebuffer)) {
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_USAGE_ANGLE,
                       GL_FRAMEBUFFER_ATTACHMENT_ANGLE);
  }

  if (use_texture_storage) {
    GLint levels = 1;
    if (use_texture_npot && (hint_ & ResourceTextureHint::kMipmap))
      levels += base::bits::Log2Floor(std::max(size_.width(), size_.height()));

    gl_->TexStorage2DEXT(GL_TEXTURE_2D, levels, TextureStorageFormat(format),
                         size_.width(), size_.height());
  } else {
    gl_->TexImage2D(GL_TEXTURE_2D, 0, GLInternalFormat(format), size_.width(),
                    size_.height(), 0, GLDataFormat(format), GLDataType(format),
                    nullptr);
  }
}

ScopedRenderPassTexture::ScopedRenderPassTexture() = default;

ScopedRenderPassTexture::ScopedRenderPassTexture(
    ScopedRenderPassTexture&& other) {
  gl_ = other.gl_;
  size_ = other.size_;
  hint_ = other.hint_;
  color_space_ = other.color_space_;
  gl_id_ = other.gl_id_;

  // When being moved, other will no longer hold this gl_id_.
  other.gl_id_ = 0;
}

ScopedRenderPassTexture::~ScopedRenderPassTexture() {
  Free();
}

ScopedRenderPassTexture& ScopedRenderPassTexture::operator=(
    ScopedRenderPassTexture&& other) {
  if (this != &other) {
    Free();
    gl_ = other.gl_;
    size_ = other.size_;
    hint_ = other.hint_;
    color_space_ = other.color_space_;
    gl_id_ = other.gl_id_;

    // When being moved, other will no longer hold this gl_id_.
    other.gl_id_ = 0;
  }
  return *this;
}

void ScopedRenderPassTexture::Free() {
  if (!gl_id_)
    return;
  gl_->DeleteTextures(1, &gl_id_);
  gl_id_ = 0;
}

}  // namespace viz
