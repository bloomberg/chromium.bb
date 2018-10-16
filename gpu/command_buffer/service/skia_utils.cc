// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/skia_utils.h"

#include "gpu/command_buffer/service/error_state.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/command_buffer/service/wrapped_sk_image.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gl/gl_gl_api_implementation.h"

namespace gpu {

namespace {

bool GetGrBackendTextureValidated(const gl::GLVersionInfo& version_info,
                                  const char* function_name,
                                  gles2::ErrorState* error_state,
                                  const gles2::Texture& texture,
                                  GLint sk_color_type,
                                  GrBackendTexture* gr_texture) {
  if (texture.target() != GL_TEXTURE_2D &&
      texture.target() != GL_TEXTURE_RECTANGLE_ARB) {
    if (error_state) {
      ERRORSTATE_SET_GL_ERROR(error_state, GL_INVALID_OPERATION, function_name,
                              "invalid texture target");
    }
    return false;
  }

  int width;
  int height;
  int depth;
  if (!texture.GetLevelSize(texture.target(), 0, &width, &height, &depth)) {
    if (error_state) {
      ERRORSTATE_SET_GL_ERROR(error_state, GL_INVALID_OPERATION, function_name,
                              "missing texture size info");
    }
    return false;
  }
  GLenum type;
  GLenum internal_format;
  if (!texture.GetLevelType(texture.target(), 0, &type, &internal_format)) {
    if (error_state) {
      ERRORSTATE_SET_GL_ERROR(error_state, GL_INVALID_OPERATION, function_name,
                              "missing texture type info");
    }
    return false;
  }

  GrGLTextureInfo texture_info;
  texture_info.fID = texture.service_id();
  texture_info.fTarget = texture.target();

  // GetInternalFormat may return a base internal format but Skia requires a
  // sized internal format. So this may be adjusted below.
  texture_info.fFormat = GetInternalFormat(&version_info, internal_format);
  switch (sk_color_type) {
    case kARGB_4444_SkColorType:
      if (internal_format != GL_RGBA4 && internal_format != GL_RGBA) {
        if (error_state) {
          ERRORSTATE_SET_GL_ERROR(error_state, GL_INVALID_OPERATION,
                                  function_name, "color type mismatch");
        }
        return false;
      }
      if (texture_info.fFormat == GL_RGBA)
        texture_info.fFormat = GL_RGBA4;
      break;
    case kRGBA_8888_SkColorType:
      if (internal_format != GL_RGBA8_OES && internal_format != GL_RGBA) {
        if (error_state) {
          ERRORSTATE_SET_GL_ERROR(error_state, GL_INVALID_OPERATION,
                                  function_name, "color type mismatch");
        }
        return false;
      }
      if (texture_info.fFormat == GL_RGBA)
        texture_info.fFormat = GL_RGBA8_OES;
      break;
    case kBGRA_8888_SkColorType:
      if (internal_format != GL_BGRA_EXT && internal_format != GL_BGRA8_EXT) {
        if (error_state) {
          ERRORSTATE_SET_GL_ERROR(error_state, GL_INVALID_OPERATION,
                                  function_name, "color type mismatch");
        }
        return false;
      }
      if (texture_info.fFormat == GL_BGRA_EXT)
        texture_info.fFormat = GL_BGRA8_EXT;
      if (texture_info.fFormat == GL_RGBA)
        texture_info.fFormat = GL_RGBA8_OES;
      break;
    default:
      if (error_state) {
        ERRORSTATE_SET_GL_ERROR(error_state, GL_INVALID_OPERATION,
                                function_name, "unsupported color type");
      }
      return false;
  }

  *gr_texture = GrBackendTexture(width, height, GrMipMapped::kNo, texture_info);
  return true;
}

}  // namespace

bool GetGrBackendTexture(const gl::GLVersionInfo& version_info,
                         const char* function_name,
                         gles2::ErrorState* error_state,
                         const TextureBase& texture,
                         GLint sk_color_type,
                         GrBackendTexture* gr_texture) {
  switch (texture.GetType()) {
    case TextureBase::Type::kNone:
      NOTREACHED();
      return false;
    case TextureBase::Type::kValidated:
      return GetGrBackendTextureValidated(
          version_info, function_name, error_state,
          static_cast<const gles2::Texture&>(texture), sk_color_type,
          gr_texture);
    case TextureBase::Type::kPassthrough:
      NOTIMPLEMENTED();
      return false;
    case TextureBase::Type::kSkImage:
      return static_cast<const raster::WrappedSkImage&>(texture)
          .GetGrBackendTexture(gr_texture);
    default:
      NOTREACHED();
  }
  return false;
}

}  // namespace gpu
