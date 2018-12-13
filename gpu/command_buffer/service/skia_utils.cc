// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/skia_utils.h"

#include "base/logging.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {

bool GetGrBackendTexture(GLenum target,
                         const gfx::Size& size,
                         GLenum internal_format,
                         GLenum driver_internal_format,
                         GLuint service_id,
                         viz::ResourceFormat resource_format,
                         GrBackendTexture* gr_texture) {
  if (target != GL_TEXTURE_2D && target != GL_TEXTURE_RECTANGLE_ARB) {
    LOG(ERROR) << "GetGrBackendTexture: invalid texture target.";
    return false;
  }

  GrGLTextureInfo texture_info;
  texture_info.fID = service_id;
  texture_info.fTarget = target;

  // |driver_internal_format| may be a base internal format but Skia requires a
  // sized internal format. So this may be adjusted below.
  texture_info.fFormat = driver_internal_format;
  switch (resource_format) {
    case viz::RGBA_4444:
      if (internal_format != GL_RGBA4 && internal_format != GL_RGBA) {
        LOG(ERROR)
            << "GetGrBackendTexture: color type mismatch. internal_format=0x"
            << std::hex << internal_format;
        return false;
      }
      if (texture_info.fFormat == GL_RGBA)
        texture_info.fFormat = GL_RGBA4;
      break;
    case viz::RGBA_8888:
      if (internal_format != GL_RGBA8_OES && internal_format != GL_RGBA) {
        LOG(ERROR)
            << "GetGrBackendTexture: color type mismatch. internal_format=0x"
            << std::hex << internal_format;
        return false;
      }
      if (texture_info.fFormat == GL_RGBA)
        texture_info.fFormat = GL_RGBA8_OES;
      break;
    case viz::RGBX_8888:
      if (internal_format != GL_RGB8_OES && internal_format != GL_RGB) {
        LOG(ERROR)
            << "GetGrBackendTexture: color type mismatch. internal_format=0x"
            << std::hex << internal_format;
        return false;
      }
      if (texture_info.fFormat == GL_RGB)
        texture_info.fFormat = GL_RGB8_OES;
      break;
    case viz::BGRA_8888:
      if (internal_format != GL_BGRA_EXT && internal_format != GL_BGRA8_EXT) {
        LOG(ERROR)
            << "GetGrBackendTexture: color type mismatch. internal_format=0x"
            << std::hex << internal_format;
        return false;
      }
      if (texture_info.fFormat == GL_BGRA_EXT)
        texture_info.fFormat = GL_BGRA8_EXT;
      if (texture_info.fFormat == GL_RGBA)
        texture_info.fFormat = GL_RGBA8_OES;
      break;
    default:
      LOG(ERROR) << "GetGrBackendTexture: unsupported color type.";
      return false;
  }

  *gr_texture = GrBackendTexture(size.width(), size.height(), GrMipMapped::kNo,
                                 texture_info);
  return true;
}

}  // namespace gpu
