// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/skia_utils.h"

#include "base/logging.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_version_info.h"

namespace gpu {

bool GetGrBackendTexture(const gl::GLVersionInfo* version_info,
                         GLenum target,
                         const gfx::Size& size,
                         GLuint service_id,
                         viz::ResourceFormat resource_format,
                         GrBackendTexture* gr_texture) {
  if (target != GL_TEXTURE_2D && target != GL_TEXTURE_RECTANGLE_ARB &&
      target != GL_TEXTURE_EXTERNAL_OES) {
    LOG(ERROR) << "GetGrBackendTexture: invalid texture target.";
    return false;
  }

  GrGLTextureInfo texture_info;
  texture_info.fID = service_id;
  texture_info.fTarget = target;
  texture_info.fFormat = gl::GetInternalFormat(
      version_info, viz::TextureStorageFormat(resource_format));
  *gr_texture = GrBackendTexture(size.width(), size.height(), GrMipMapped::kNo,
                                 texture_info);
  return true;
}

}  // namespace gpu
