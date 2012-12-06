// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/texture_definition.h"

namespace gpu {
namespace gles2 {

TextureDefinition::LevelInfo::LevelInfo(GLenum target,
                                        GLenum internal_format,
                                        GLsizei width,
                                        GLsizei height,
                                        GLsizei depth,
                                        GLint border,
                                        GLenum format,
                                        GLenum type,
                                        bool cleared)
    : target(target),
      internal_format(internal_format),
      width(width),
      height(height),
      depth(depth),
      border(border),
      format(format),
      type(type),
      cleared(cleared) {
}

TextureDefinition::LevelInfo::LevelInfo()
    : target(0),
      internal_format(0),
      width(0),
      height(0),
      depth(0),
      border(0),
      format(0),
      type(0),
      cleared(true) {
}

TextureDefinition::TextureDefinition(GLenum target,
                                     GLuint service_id,
                                     GLenum min_filter,
                                     GLenum mag_filter,
                                     GLenum wrap_s,
                                     GLenum wrap_t,
                                     GLenum usage,
                                     bool immutable,
                                     const LevelInfos& level_infos)
    : target_(target),
      service_id_(service_id),
      min_filter_(min_filter),
      mag_filter_(mag_filter),
      wrap_s_(wrap_s),
      wrap_t_(wrap_t),
      usage_(usage),
      immutable_(immutable),
      level_infos_(level_infos) {
}

TextureDefinition::~TextureDefinition() {
  DCHECK_EQ(0U, service_id_) << "TextureDefinition leaked texture.";
}

GLuint TextureDefinition::ReleaseServiceId() {
  GLuint service_id = service_id_;
  service_id_ = 0;
  return service_id;
}

}  // namespace gles2
}  // namespace gpu
