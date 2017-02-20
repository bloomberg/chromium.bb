// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_copy_texture_chromium.h"

#include <stddef.h>

#include <algorithm>

#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_version_info.h"

namespace {

const GLfloat kIdentityMatrix[16] = {1.0f, 0.0f, 0.0f, 0.0f,
                                     0.0f, 1.0f, 0.0f, 0.0f,
                                     0.0f, 0.0f, 1.0f, 0.0f,
                                     0.0f, 0.0f, 0.0f, 1.0f};

enum {
  SAMPLER_2D,
  SAMPLER_RECTANGLE_ARB,
  SAMPLER_EXTERNAL_OES,
  NUM_SAMPLERS
};

enum {
  S_FORMAT_ALPHA,
  S_FORMAT_LUMINANCE,
  S_FORMAT_LUMINANCE_ALPHA,
  S_FORMAT_RED,
  S_FORMAT_RGB,
  S_FORMAT_RGBA,
  S_FORMAT_RGB8,
  S_FORMAT_RGBA8,
  S_FORMAT_BGRA_EXT,
  S_FORMAT_BGRA8_EXT,
  S_FORMAT_RGB_YCBCR_420V_CHROMIUM,
  S_FORMAT_RGB_YCBCR_422_CHROMIUM,
  S_FORMAT_COMPRESSED,
  NUM_S_FORMAT
};

enum {
  D_FORMAT_RGB,
  D_FORMAT_RGBA,
  D_FORMAT_RGB8,
  D_FORMAT_RGBA8,
  D_FORMAT_BGRA_EXT,
  D_FORMAT_BGRA8_EXT,
  D_FORMAT_SRGB_EXT,
  D_FORMAT_SRGB_ALPHA_EXT,
  D_FORMAT_R8,
  D_FORMAT_R8UI,
  D_FORMAT_RG8,
  D_FORMAT_RG8UI,
  D_FORMAT_SRGB8,
  D_FORMAT_RGB565,
  D_FORMAT_RGB8UI,
  D_FORMAT_SRGB8_ALPHA8,
  D_FORMAT_RGB5_A1,
  D_FORMAT_RGBA4,
  D_FORMAT_RGBA8UI,
  D_FORMAT_RGB9_E5,
  D_FORMAT_R16F,
  D_FORMAT_R32F,
  D_FORMAT_RG16F,
  D_FORMAT_RG32F,
  D_FORMAT_RGB16F,
  D_FORMAT_RGB32F,
  D_FORMAT_RGBA16F,
  D_FORMAT_RGBA32F,
  D_FORMAT_R11F_G11F_B10F,
  NUM_D_FORMAT
};

const unsigned kNumVertexShaders = NUM_SAMPLERS;
const unsigned kNumFragmentShaders =
    4 * NUM_SAMPLERS * NUM_S_FORMAT * NUM_D_FORMAT;

typedef unsigned ShaderId;

ShaderId GetVertexShaderId(GLenum target) {
  ShaderId id = 0;
  switch (target) {
    case GL_TEXTURE_2D:
      id = SAMPLER_2D;
      break;
    case GL_TEXTURE_RECTANGLE_ARB:
      id = SAMPLER_RECTANGLE_ARB;
      break;
    case GL_TEXTURE_EXTERNAL_OES:
      id = SAMPLER_EXTERNAL_OES;
      break;
    default:
      NOTREACHED();
      break;
  }
  return id;
}

// Returns the correct fragment shader id to evaluate the copy operation for
// the premultiply alpha pixel store settings and target.
ShaderId GetFragmentShaderId(bool premultiply_alpha,
                             bool unpremultiply_alpha,
                             GLenum target,
                             GLenum source_format,
                             GLenum dest_format) {
  unsigned alphaIndex = 0;
  unsigned targetIndex = 0;
  unsigned sourceFormatIndex = 0;
  unsigned destFormatIndex = 0;

  alphaIndex = (premultiply_alpha   ? (1 << 0) : 0) |
               (unpremultiply_alpha ? (1 << 1) : 0);

  switch (target) {
    case GL_TEXTURE_2D:
      targetIndex = SAMPLER_2D;
      break;
    case GL_TEXTURE_RECTANGLE_ARB:
      targetIndex = SAMPLER_RECTANGLE_ARB;
      break;
    case GL_TEXTURE_EXTERNAL_OES:
      targetIndex = SAMPLER_EXTERNAL_OES;
      break;
    default:
      NOTREACHED();
      break;
  }

  switch (source_format) {
    case GL_ALPHA:
      sourceFormatIndex = S_FORMAT_ALPHA;
      break;
    case GL_LUMINANCE:
      sourceFormatIndex = S_FORMAT_LUMINANCE;
      break;
    case GL_LUMINANCE_ALPHA:
      sourceFormatIndex = S_FORMAT_LUMINANCE_ALPHA;
      break;
    case GL_RED:
      sourceFormatIndex = S_FORMAT_RED;
      break;
    case GL_RGB:
      sourceFormatIndex = S_FORMAT_RGB;
      break;
    case GL_RGBA:
      sourceFormatIndex = S_FORMAT_RGBA;
      break;
    case GL_RGB8:
      sourceFormatIndex = S_FORMAT_RGB8;
      break;
    case GL_RGBA8:
      sourceFormatIndex = S_FORMAT_RGBA8;
      break;
    case GL_BGRA_EXT:
      sourceFormatIndex = S_FORMAT_BGRA_EXT;
      break;
    case GL_BGRA8_EXT:
      sourceFormatIndex = S_FORMAT_BGRA8_EXT;
      break;
    case GL_RGB_YCBCR_420V_CHROMIUM:
      sourceFormatIndex = S_FORMAT_RGB_YCBCR_420V_CHROMIUM;
      break;
    case GL_RGB_YCBCR_422_CHROMIUM:
      sourceFormatIndex = S_FORMAT_RGB_YCBCR_422_CHROMIUM;
      break;
    case GL_ATC_RGB_AMD:
    case GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD:
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case GL_ETC1_RGB8_OES:
      sourceFormatIndex = S_FORMAT_COMPRESSED;
      break;
    default:
      NOTREACHED();
      break;
  }

  switch (dest_format) {
    case GL_RGB:
      destFormatIndex = D_FORMAT_RGB;
      break;
    case GL_RGBA:
      destFormatIndex = D_FORMAT_RGBA;
      break;
    case GL_RGB8:
      destFormatIndex = D_FORMAT_RGB8;
      break;
    case GL_RGBA8:
      destFormatIndex = D_FORMAT_RGBA8;
      break;
    case GL_BGRA_EXT:
      destFormatIndex = D_FORMAT_BGRA_EXT;
      break;
    case GL_BGRA8_EXT:
      destFormatIndex = D_FORMAT_BGRA8_EXT;
      break;
    case GL_SRGB_EXT:
      destFormatIndex = D_FORMAT_SRGB_EXT;
      break;
    case GL_SRGB_ALPHA_EXT:
      destFormatIndex = D_FORMAT_SRGB_ALPHA_EXT;
      break;
    case GL_R8:
      destFormatIndex = D_FORMAT_R8;
      break;
    case GL_R8UI:
      destFormatIndex = D_FORMAT_R8UI;
      break;
    case GL_RG8:
      destFormatIndex = D_FORMAT_RG8;
      break;
    case GL_RG8UI:
      destFormatIndex = D_FORMAT_RG8UI;
      break;
    case GL_SRGB8:
      destFormatIndex = D_FORMAT_SRGB8;
      break;
    case GL_RGB565:
      destFormatIndex = D_FORMAT_RGB565;
      break;
    case GL_RGB8UI:
      destFormatIndex = D_FORMAT_RGB8UI;
      break;
    case GL_SRGB8_ALPHA8:
      destFormatIndex = D_FORMAT_SRGB8_ALPHA8;
      break;
    case GL_RGB5_A1:
      destFormatIndex = D_FORMAT_RGB5_A1;
      break;
    case GL_RGBA4:
      destFormatIndex = D_FORMAT_RGBA4;
      break;
    case GL_RGBA8UI:
      destFormatIndex = D_FORMAT_RGBA8UI;
      break;
    case GL_RGB9_E5:
      destFormatIndex = D_FORMAT_RGB9_E5;
      break;
    case GL_R16F:
      destFormatIndex = D_FORMAT_R16F;
      break;
    case GL_R32F:
      destFormatIndex = D_FORMAT_R32F;
      break;
    case GL_RG16F:
      destFormatIndex = D_FORMAT_RG16F;
      break;
    case GL_RG32F:
      destFormatIndex = D_FORMAT_RG32F;
      break;
    case GL_RGB16F:
      destFormatIndex = D_FORMAT_RGB16F;
      break;
    case GL_RGB32F:
      destFormatIndex = D_FORMAT_RGB32F;
      break;
    case GL_RGBA16F:
      destFormatIndex = D_FORMAT_RGBA16F;
      break;
    case GL_RGBA32F:
      destFormatIndex = D_FORMAT_RGBA32F;
      break;
    case GL_R11F_G11F_B10F:
      destFormatIndex = D_FORMAT_R11F_G11F_B10F;
      break;
    default:
      NOTREACHED();
      break;
  }

  return alphaIndex + targetIndex * 4 + sourceFormatIndex * 4 * NUM_SAMPLERS +
         destFormatIndex * 4 * NUM_SAMPLERS * NUM_S_FORMAT;
}

const char* kShaderPrecisionPreamble =
    "#ifdef GL_ES\n"
    "precision mediump float;\n"
    "#define TexCoordPrecision mediump\n"
    "#else\n"
    "#define TexCoordPrecision\n"
    "#endif\n";

std::string GetVertexShaderSource(const gl::GLVersionInfo& gl_version_info,
                                  GLenum target) {
  std::string source;

  if (gl_version_info.is_es || gl_version_info.IsLowerThanGL(3, 2)) {
    if (gl_version_info.is_es3 && target != GL_TEXTURE_EXTERNAL_OES) {
      source += "#version 300 es\n";
      source +=
          "#define ATTRIBUTE in\n"
          "#define VARYING out\n";
    } else {
      source +=
          "#define ATTRIBUTE attribute\n"
          "#define VARYING varying\n";
    }
  } else {
    source += "#version 150\n";
    source +=
        "#define ATTRIBUTE in\n"
        "#define VARYING out\n";
  }

  // Preamble for texture precision.
  source += kShaderPrecisionPreamble;

  // Main shader source.
  source +=
      "uniform vec2 u_vertex_dest_mult;\n"
      "uniform vec2 u_vertex_dest_add;\n"
      "uniform vec2 u_vertex_source_mult;\n"
      "uniform vec2 u_vertex_source_add;\n"
      "ATTRIBUTE vec2 a_position;\n"
      "VARYING TexCoordPrecision vec2 v_uv;\n"
      "void main(void) {\n"
      "  gl_Position = vec4(0, 0, 0, 1);\n"
      "  gl_Position.xy =\n"
      "      a_position.xy * u_vertex_dest_mult + u_vertex_dest_add;\n"
      "  v_uv = a_position.xy * u_vertex_source_mult + u_vertex_source_add;\n"
      "}\n";

  return source;
}

std::string GetFragmentShaderSource(const gl::GLVersionInfo& gl_version_info,
                                    bool premultiply_alpha,
                                    bool unpremultiply_alpha,
                                    bool nv_egl_stream_consumer_external,
                                    GLenum target,
                                    GLenum source_format,
                                    GLenum dest_format) {
  std::string source;

  // Preamble for core and compatibility mode.
  if (gl_version_info.is_es || gl_version_info.IsLowerThanGL(3, 2)) {
    if (gl_version_info.is_es3 && target != GL_TEXTURE_EXTERNAL_OES) {
      source += "#version 300 es\n";
    }
    if (target == GL_TEXTURE_EXTERNAL_OES) {
      source += "#extension GL_OES_EGL_image_external : enable\n";

      if (nv_egl_stream_consumer_external) {
        source += "#extension GL_NV_EGL_stream_consumer_external : enable\n";
      }
    }
  } else {
    source += "#version 150\n";
  }

  // Preamble for texture precision.
  source += kShaderPrecisionPreamble;

  if (gpu::gles2::GLES2Util::IsSignedIntegerFormat(dest_format)) {
    source += "#define TextureType ivec4\n";
    source += "#define ZERO 0\n";
    source += "#define MAX_COLOR 255\n";
    if (gpu::gles2::GLES2Util::IsSignedIntegerFormat(source_format))
      source += "#define InnerScaleValue 1\n";
    else if (gpu::gles2::GLES2Util::IsUnsignedIntegerFormat(source_format))
      source += "#define InnerScaleValue 1u\n";
    else
      source += "#define InnerScaleValue 255.0\n";
    source += "#define OuterScaleValue 1\n";
  } else if (gpu::gles2::GLES2Util::IsUnsignedIntegerFormat(dest_format)) {
    source += "#define TextureType uvec4\n";
    source += "#define ZERO 0u\n";
    source += "#define MAX_COLOR 255u\n";
    if (gpu::gles2::GLES2Util::IsSignedIntegerFormat(source_format))
      source += "#define InnerScaleValue 1\n";
    else if (gpu::gles2::GLES2Util::IsUnsignedIntegerFormat(source_format))
      source += "#define InnerScaleValue 1u\n";
    else
      source += "#define InnerScaleValue 255.0\n";
    source += "#define OuterScaleValue 1u\n";
  } else {
    source += "#define TextureType vec4\n";
    source += "#define ZERO 0.0\n";
    source += "#define MAX_COLOR 1.0\n";
    if (gpu::gles2::GLES2Util::IsSignedIntegerFormat(source_format)) {
      source += "#define InnerScaleValue 1\n";
      source += "#define OuterScaleValue (1.0 / 255.0)\n";
    } else if (gpu::gles2::GLES2Util::IsUnsignedIntegerFormat(source_format)) {
      source += "#define InnerScaleValue 1u\n";
      source += "#define OuterScaleValue (1.0 / 255.0)\n";
    } else {
      source += "#define InnerScaleValue 1.0\n";
      source += "#define OuterScaleValue 1.0\n";
    }
  }
  if (gl_version_info.is_es2 || gl_version_info.IsLowerThanGL(3, 2) ||
      target == GL_TEXTURE_EXTERNAL_OES) {
    switch (target) {
      case GL_TEXTURE_2D:
      case GL_TEXTURE_EXTERNAL_OES:
        source += "#define TextureLookup texture2D\n";
        break;
      default:
        NOTREACHED();
        break;
    }

    source +=
        "#define VARYING varying\n"
        "#define FRAGCOLOR gl_FragColor\n";
  } else {
    source +=
        "#define VARYING in\n"
        "out TextureType frag_color;\n"
        "#define FRAGCOLOR frag_color\n"
        "#define TextureLookup texture\n";
  }

  // Preamble for sampler type.
  switch (target) {
    case GL_TEXTURE_2D:
      source += "#define SamplerType sampler2D\n";
      break;
    case GL_TEXTURE_RECTANGLE_ARB:
      source += "#define SamplerType sampler2DRect\n";
      break;
    case GL_TEXTURE_EXTERNAL_OES:
      source += "#define SamplerType samplerExternalOES\n";
      break;
    default:
      NOTREACHED();
      break;
  }

  // Main shader source.
  source +=
      "uniform SamplerType u_sampler;\n"
      "uniform mat4 u_tex_coord_transform;\n"
      "VARYING TexCoordPrecision vec2 v_uv;\n"
      "void main(void) {\n"
      "  TexCoordPrecision vec4 uv =\n"
      "      u_tex_coord_transform * vec4(v_uv, 0, 1);\n"
      "  vec4 color = TextureLookup(u_sampler, uv.st);\n"
      "  FRAGCOLOR = TextureType(color * InnerScaleValue) * OuterScaleValue;\n";

  // Post-processing to premultiply or un-premultiply alpha.
  // Check dest format has alpha channel first.
  if ((gpu::gles2::GLES2Util::GetChannelsForFormat(dest_format) & 0x0008) !=
      0) {
    if (premultiply_alpha) {
      source += "  FRAGCOLOR.rgb *= FRAGCOLOR.a;\n";
      source += "  FRAGCOLOR.rgb /= MAX_COLOR;\n";
    }
    if (unpremultiply_alpha) {
      source +=
          "  if (FRAGCOLOR.a > ZERO) {\n"
          "    FRAGCOLOR.rgb /= FRAGCOLOR.a;\n"
          "    FRAGCOLOR.rgb *= MAX_COLOR;\n"
          "  }\n";
    }
  }

  // Main function end.
  source += "}\n";

  return source;
}

GLenum getIntermediateFormat(GLenum format) {
  switch (format) {
    case GL_LUMINANCE_ALPHA:
    case GL_LUMINANCE:
    case GL_ALPHA:
      return GL_RGBA;
    case GL_SRGB_EXT:
      return GL_SRGB_ALPHA_EXT;
    case GL_RGB16F:
      return GL_RGBA16F;
    case GL_RGB9_E5:
    case GL_RGB32F:
      return GL_RGBA32F;
    case GL_SRGB8:
      return GL_SRGB8_ALPHA8;
    case GL_RGB8UI:
      return GL_RGBA8UI;
    default:
      return format;
  }
}

void CompileShader(GLuint shader, const char* shader_source) {
  glShaderSource(shader, 1, &shader_source, 0);
  glCompileShader(shader);
#if DCHECK_IS_ON()
  GLint compile_status;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
  if (GL_TRUE != compile_status) {
    char buffer[1024];
    GLsizei length = 0;
    glGetShaderInfoLog(shader, sizeof(buffer), &length, buffer);
    std::string log(buffer, length);
    DLOG(ERROR) << "CopyTextureCHROMIUM: shader compilation failure: " << log;
  }
#endif
}

void DeleteShader(GLuint shader) {
  if (shader)
    glDeleteShader(shader);
}

bool BindFramebufferTexture2D(GLenum target,
                              GLuint texture_id,
                              GLint level,
                              GLuint framebuffer) {
  DCHECK(target == GL_TEXTURE_2D || target == GL_TEXTURE_RECTANGLE_ARB);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(target, texture_id);
  // NVidia drivers require texture settings to be a certain way
  // or they won't report FRAMEBUFFER_COMPLETE.
  if (level > 0)
    glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, level);
  glTexParameterf(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target,
                            texture_id, level);

#ifndef NDEBUG
  GLenum fb_status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);
  if (GL_FRAMEBUFFER_COMPLETE != fb_status) {
    DLOG(ERROR) << "CopyTextureCHROMIUM: Incomplete framebuffer.";
    return false;
  }
#endif
  return true;
}

void DoCopyTexImage2D(const gpu::gles2::GLES2Decoder* decoder,
                      GLenum source_target,
                      GLuint source_id,
                      GLint source_level,
                      GLenum dest_target,
                      GLuint dest_id,
                      GLint dest_level,
                      GLenum dest_internal_format,
                      GLsizei width,
                      GLsizei height,
                      GLuint framebuffer) {
  DCHECK_EQ(static_cast<GLenum>(GL_TEXTURE_2D), source_target);
  GLenum dest_binding_target =
      gpu::gles2::GLES2Util::GLFaceTargetToTextureTarget(dest_target);
  DCHECK(dest_binding_target == GL_TEXTURE_2D ||
         dest_binding_target == GL_TEXTURE_CUBE_MAP);
  DCHECK(source_level == 0 || decoder->GetFeatureInfo()->IsES3Capable());
  if (BindFramebufferTexture2D(source_target, source_id, source_level,
                               framebuffer)) {
    glBindTexture(dest_binding_target, dest_id);
    glTexParameterf(dest_binding_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(dest_binding_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(dest_binding_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(dest_binding_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glCopyTexImage2D(dest_target, dest_level, dest_internal_format, 0 /* x */,
                     0 /* y */, width, height, 0 /* border */);
  }

  decoder->RestoreTextureState(source_id);
  decoder->RestoreTextureState(dest_id);
  decoder->RestoreTextureUnitBindings(0);
  decoder->RestoreActiveTexture();
  decoder->RestoreFramebufferBindings();
}

void DoCopyTexSubImage2D(const gpu::gles2::GLES2Decoder* decoder,
                         GLenum source_target,
                         GLuint source_id,
                         GLint source_level,
                         GLenum dest_target,
                         GLuint dest_id,
                         GLint dest_level,
                         GLint xoffset,
                         GLint yoffset,
                         GLint source_x,
                         GLint source_y,
                         GLsizei source_width,
                         GLsizei source_height,
                         GLuint framebuffer) {
  DCHECK(source_target == GL_TEXTURE_2D ||
         source_target == GL_TEXTURE_RECTANGLE_ARB);
  GLenum dest_binding_target =
      gpu::gles2::GLES2Util::GLFaceTargetToTextureTarget(dest_target);
  DCHECK(dest_binding_target == GL_TEXTURE_2D ||
         dest_binding_target == GL_TEXTURE_CUBE_MAP);
  DCHECK(source_level == 0 || decoder->GetFeatureInfo()->IsES3Capable());
  if (BindFramebufferTexture2D(source_target, source_id, source_level,
                               framebuffer)) {
    glBindTexture(dest_binding_target, dest_id);
    glTexParameterf(dest_binding_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(dest_binding_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(dest_binding_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(dest_binding_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glCopyTexSubImage2D(dest_target, dest_level, xoffset, yoffset, source_x,
                        source_y, source_width, source_height);
  }

  decoder->RestoreTextureState(source_id);
  decoder->RestoreTextureState(dest_id);
  decoder->RestoreTextureUnitBindings(0);
  decoder->RestoreActiveTexture();
  decoder->RestoreFramebufferBindings();
}

}  // namespace

namespace gpu {
namespace gles2 {

CopyTextureCHROMIUMResourceManager::CopyTextureCHROMIUMResourceManager()
    : initialized_(false),
      nv_egl_stream_consumer_external_(false),
      vertex_shaders_(kNumVertexShaders, 0u),
      fragment_shaders_(kNumFragmentShaders, 0u),
      vertex_array_object_id_(0u),
      buffer_id_(0u),
      framebuffer_(0u) {}

CopyTextureCHROMIUMResourceManager::~CopyTextureCHROMIUMResourceManager() {
  // |buffer_id_| and |framebuffer_| can be not-null because when GPU context is
  // lost, this class can be deleted without releasing resources like
  // GLES2DecoderImpl.
}

void CopyTextureCHROMIUMResourceManager::Initialize(
    const gles2::GLES2Decoder* decoder,
    const gles2::FeatureInfo::FeatureFlags& feature_flags) {
  static_assert(
      kVertexPositionAttrib == 0u,
      "kVertexPositionAttrib must be 0");
  DCHECK(!buffer_id_);
  DCHECK(!vertex_array_object_id_);
  DCHECK(!framebuffer_);
  DCHECK(programs_.empty());

  nv_egl_stream_consumer_external_ =
      feature_flags.nv_egl_stream_consumer_external;

  if (feature_flags.native_vertex_array_object) {
    glGenVertexArraysOES(1, &vertex_array_object_id_);
    glBindVertexArrayOES(vertex_array_object_id_);
  }

  // Initialize all of the GPU resources required to perform the copy.
  glGenBuffersARB(1, &buffer_id_);
  glBindBuffer(GL_ARRAY_BUFFER, buffer_id_);
  const GLfloat kQuadVertices[] = {-1.0f, -1.0f,
                                    1.0f, -1.0f,
                                    1.0f,  1.0f,
                                   -1.0f,  1.0f};
  glBufferData(
      GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);

  glGenFramebuffersEXT(1, &framebuffer_);

  if (vertex_array_object_id_) {
    glEnableVertexAttribArray(kVertexPositionAttrib);
    glVertexAttribPointer(kVertexPositionAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
    decoder->RestoreAllAttributes();
  }

  decoder->RestoreBufferBindings();

  initialized_ = true;
}

void CopyTextureCHROMIUMResourceManager::Destroy() {
  if (!initialized_)
    return;

  if (vertex_array_object_id_) {
    glDeleteVertexArraysOES(1, &vertex_array_object_id_);
    vertex_array_object_id_ = 0;
  }

  glDeleteFramebuffersEXT(1, &framebuffer_);
  framebuffer_ = 0;

  std::for_each(
      vertex_shaders_.begin(), vertex_shaders_.end(), DeleteShader);
  std::for_each(
      fragment_shaders_.begin(), fragment_shaders_.end(), DeleteShader);

  for (ProgramMap::const_iterator it = programs_.begin(); it != programs_.end();
       ++it) {
    const ProgramInfo& info = it->second;
    glDeleteProgram(info.program);
  }

  glDeleteBuffersARB(1, &buffer_id_);
  buffer_id_ = 0;
}

void CopyTextureCHROMIUMResourceManager::DoCopyTexture(
    const gles2::GLES2Decoder* decoder,
    GLenum source_target,
    GLuint source_id,
    GLint source_level,
    GLenum source_internal_format,
    GLenum dest_target,
    GLuint dest_id,
    GLint dest_level,
    GLenum dest_internal_format,
    GLsizei width,
    GLsizei height,
    bool flip_y,
    bool premultiply_alpha,
    bool unpremultiply_alpha,
    CopyTextureMethod method) {
  bool premultiply_alpha_change = premultiply_alpha ^ unpremultiply_alpha;
  GLenum dest_binding_target =
      gpu::gles2::GLES2Util::GLFaceTargetToTextureTarget(dest_target);

  // GL_TEXTURE_RECTANGLE_ARB on FBO is supported by OpenGL, not GLES2,
  // so restrict this to GL_TEXTURE_2D and GL_TEXTURE_CUBE_MAP.
  if (source_target == GL_TEXTURE_2D &&
      (dest_binding_target == GL_TEXTURE_2D ||
       dest_binding_target == GL_TEXTURE_CUBE_MAP) &&
      !flip_y && !premultiply_alpha_change && method == DIRECT_COPY) {
    DoCopyTexImage2D(decoder, source_target, source_id, source_level,
                     dest_target, dest_id, dest_level, dest_internal_format,
                     width, height, framebuffer_);
    return;
  }

  // Draw to level 0 of an intermediate GL_TEXTURE_2D texture.
  GLuint dest_texture = dest_id;
  GLuint intermediate_texture = 0;
  GLint original_dest_level = dest_level;
  GLenum original_dest_target = dest_target;
  GLenum original_internal_format = dest_internal_format;
  if (method == DRAW_AND_COPY) {
    GLenum adjusted_internal_format =
        getIntermediateFormat(dest_internal_format);
    dest_target = GL_TEXTURE_2D;
    glGenTextures(1, &intermediate_texture);
    glBindTexture(dest_target, intermediate_texture);
    GLenum format = TextureManager::ExtractFormatFromStorageFormat(
        adjusted_internal_format);
    GLenum type =
        TextureManager::ExtractTypeFromStorageFormat(adjusted_internal_format);

    glTexImage2D(dest_target, 0, adjusted_internal_format, width, height, 0,
                 format, type, nullptr);
    dest_texture = intermediate_texture;
    dest_level = 0;
    dest_internal_format = adjusted_internal_format;
  }
  // Use kIdentityMatrix if no transform passed in.
  DoCopyTextureWithTransform(
      decoder, source_target, source_id, source_level, source_internal_format,
      dest_target, dest_texture, dest_level, dest_internal_format, width,
      height, flip_y, premultiply_alpha, unpremultiply_alpha, kIdentityMatrix);

  if (method == DRAW_AND_COPY) {
    source_level = 0;
    DoCopyTexImage2D(decoder, dest_target, intermediate_texture, source_level,
                     original_dest_target, dest_id, original_dest_level,
                     original_internal_format, width, height, framebuffer_);
    glDeleteTextures(1, &intermediate_texture);
  }
}

void CopyTextureCHROMIUMResourceManager::DoCopySubTexture(
    const gles2::GLES2Decoder* decoder,
    GLenum source_target,
    GLuint source_id,
    GLint source_level,
    GLenum source_internal_format,
    GLenum dest_target,
    GLuint dest_id,
    GLint dest_level,
    GLenum dest_internal_format,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLsizei dest_width,
    GLsizei dest_height,
    GLsizei source_width,
    GLsizei source_height,
    bool flip_y,
    bool premultiply_alpha,
    bool unpremultiply_alpha,
    CopyTextureMethod method) {
  bool premultiply_alpha_change = premultiply_alpha ^ unpremultiply_alpha;
  GLenum dest_binding_target =
      gpu::gles2::GLES2Util::GLFaceTargetToTextureTarget(dest_target);

  // GL_TEXTURE_RECTANGLE_ARB on FBO is supported by OpenGL, not GLES2,
  // so restrict this to GL_TEXTURE_2D and GL_TEXTURE_CUBE_MAP.
  if (source_target == GL_TEXTURE_2D &&
      (dest_binding_target == GL_TEXTURE_2D ||
       dest_binding_target == GL_TEXTURE_CUBE_MAP) &&
      !flip_y && !premultiply_alpha_change && method == DIRECT_COPY) {
    DoCopyTexSubImage2D(decoder, source_target, source_id, source_level,
                        dest_target, dest_id, dest_level, xoffset, yoffset, x,
                        y, width, height, framebuffer_);
    return;
  }

  // Draw to level 0 of an intermediate GL_TEXTURE_2D texture.
  GLint dest_xoffset = xoffset;
  GLint dest_yoffset = yoffset;
  GLuint dest_texture = dest_id;
  GLint original_dest_level = dest_level;
  GLenum original_dest_target = dest_target;
  GLuint intermediate_texture = 0;
  if (method == DRAW_AND_COPY) {
    GLenum adjusted_internal_format =
        getIntermediateFormat(dest_internal_format);
    dest_target = GL_TEXTURE_2D;
    glGenTextures(1, &intermediate_texture);
    glBindTexture(dest_target, intermediate_texture);
    GLenum format = TextureManager::ExtractFormatFromStorageFormat(
        adjusted_internal_format);
    GLenum type =
        TextureManager::ExtractTypeFromStorageFormat(adjusted_internal_format);

    glTexImage2D(dest_target, 0, adjusted_internal_format, width, height, 0,
                 format, type, nullptr);
    dest_texture = intermediate_texture;
    dest_level = 0;
    dest_internal_format = adjusted_internal_format;
    dest_xoffset = 0;
    dest_yoffset = 0;
    dest_width = width;
    dest_height = height;
  }

  DoCopySubTextureWithTransform(
      decoder, source_target, source_id, source_level, source_internal_format,
      dest_target, dest_texture, dest_level, dest_internal_format, dest_xoffset,
      dest_yoffset, x, y, width, height, dest_width, dest_height, source_width,
      source_height, flip_y, premultiply_alpha, unpremultiply_alpha,
      kIdentityMatrix);

  if (method == DRAW_AND_COPY) {
    source_level = 0;
    DoCopyTexSubImage2D(decoder, dest_target, intermediate_texture,
                        source_level, original_dest_target, dest_id,
                        original_dest_level, xoffset, yoffset, 0, 0, width,
                        height, framebuffer_);
    glDeleteTextures(1, &intermediate_texture);
  }
}

void CopyTextureCHROMIUMResourceManager::DoCopySubTextureWithTransform(
    const gles2::GLES2Decoder* decoder,
    GLenum source_target,
    GLuint source_id,
    GLint source_level,
    GLenum source_internal_format,
    GLenum dest_target,
    GLuint dest_id,
    GLint dest_level,
    GLenum dest_internal_format,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLsizei dest_width,
    GLsizei dest_height,
    GLsizei source_width,
    GLsizei source_height,
    bool flip_y,
    bool premultiply_alpha,
    bool unpremultiply_alpha,
    const GLfloat transform_matrix[16]) {
  DoCopyTextureInternal(
      decoder, source_target, source_id, source_level, source_internal_format,
      dest_target, dest_id, dest_level, dest_internal_format, xoffset, yoffset,
      x, y, width, height, dest_width, dest_height, source_width, source_height,
      flip_y, premultiply_alpha, unpremultiply_alpha, transform_matrix);
}

void CopyTextureCHROMIUMResourceManager::DoCopyTextureWithTransform(
    const gles2::GLES2Decoder* decoder,
    GLenum source_target,
    GLuint source_id,
    GLint source_level,
    GLenum source_format,
    GLenum dest_target,
    GLuint dest_id,
    GLint dest_level,
    GLenum dest_format,
    GLsizei width,
    GLsizei height,
    bool flip_y,
    bool premultiply_alpha,
    bool unpremultiply_alpha,
    const GLfloat transform_matrix[16]) {
  GLsizei dest_width = width;
  GLsizei dest_height = height;
  DoCopyTextureInternal(decoder, source_target, source_id, source_level,
                        source_format, dest_target, dest_id, dest_level,
                        dest_format, 0, 0, 0, 0, width, height, dest_width,
                        dest_height, width, height, flip_y, premultiply_alpha,
                        unpremultiply_alpha, transform_matrix);
}

void CopyTextureCHROMIUMResourceManager::DoCopyTextureInternal(
    const gles2::GLES2Decoder* decoder,
    GLenum source_target,
    GLuint source_id,
    GLint source_level,
    GLenum source_format,
    GLenum dest_target,
    GLuint dest_id,
    GLint dest_level,
    GLenum dest_format,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLsizei dest_width,
    GLsizei dest_height,
    GLsizei source_width,
    GLsizei source_height,
    bool flip_y,
    bool premultiply_alpha,
    bool unpremultiply_alpha,
    const GLfloat transform_matrix[16]) {
  DCHECK(source_target == GL_TEXTURE_2D ||
         source_target == GL_TEXTURE_RECTANGLE_ARB ||
         source_target == GL_TEXTURE_EXTERNAL_OES);
  DCHECK(dest_target == GL_TEXTURE_2D ||
         dest_target == GL_TEXTURE_RECTANGLE_ARB);
  DCHECK_GE(source_level, 0);
  DCHECK_GE(dest_level, 0);
  DCHECK_GE(xoffset, 0);
  DCHECK_LE(xoffset + width, dest_width);
  DCHECK_GE(yoffset, 0);
  DCHECK_LE(yoffset + height, dest_height);
  if (dest_width == 0 || dest_height == 0 || source_width == 0 ||
      source_height == 0) {
    return;
  }

  if (!initialized_) {
    DLOG(ERROR) << "CopyTextureCHROMIUM: Uninitialized manager.";
    return;
  }
  const gl::GLVersionInfo& gl_version_info =
      decoder->GetFeatureInfo()->gl_version_info();

  if (vertex_array_object_id_) {
    glBindVertexArrayOES(vertex_array_object_id_);
  } else {
    if (!gl_version_info.is_desktop_core_profile) {
      decoder->ClearAllAttributes();
    }
    glEnableVertexAttribArray(kVertexPositionAttrib);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_id_);
    glVertexAttribPointer(kVertexPositionAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
  }

  ShaderId vertex_shader_id = GetVertexShaderId(source_target);
  DCHECK_LT(static_cast<size_t>(vertex_shader_id), vertex_shaders_.size());
  ShaderId fragment_shader_id = GetFragmentShaderId(
      premultiply_alpha, unpremultiply_alpha, source_target,
      source_format, dest_format);
  DCHECK_LT(static_cast<size_t>(fragment_shader_id), fragment_shaders_.size());

  ProgramMapKey key(fragment_shader_id);
  ProgramInfo* info = &programs_[key];
  // Create program if necessary.
  if (!info->program) {
    info->program = glCreateProgram();
    GLuint* vertex_shader = &vertex_shaders_[vertex_shader_id];
    if (!*vertex_shader) {
      *vertex_shader = glCreateShader(GL_VERTEX_SHADER);
      std::string source =
          GetVertexShaderSource(gl_version_info, source_target);
      CompileShader(*vertex_shader, source.c_str());
    }
    glAttachShader(info->program, *vertex_shader);
    GLuint* fragment_shader = &fragment_shaders_[fragment_shader_id];
    if (!*fragment_shader) {
      *fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
      std::string source = GetFragmentShaderSource(
          gl_version_info, premultiply_alpha, unpremultiply_alpha,
          nv_egl_stream_consumer_external_, source_target, source_format,
          dest_format);
      CompileShader(*fragment_shader, source.c_str());
    }
    glAttachShader(info->program, *fragment_shader);
    glBindAttribLocation(info->program, kVertexPositionAttrib, "a_position");
    glLinkProgram(info->program);
#ifndef NDEBUG
    GLint linked;
    glGetProgramiv(info->program, GL_LINK_STATUS, &linked);
    if (!linked)
      DLOG(ERROR) << "CopyTextureCHROMIUM: program link failure.";
#endif
    info->vertex_dest_mult_handle =
        glGetUniformLocation(info->program, "u_vertex_dest_mult");
    info->vertex_dest_add_handle =
        glGetUniformLocation(info->program, "u_vertex_dest_add");
    info->vertex_source_mult_handle =
        glGetUniformLocation(info->program, "u_vertex_source_mult");
    info->vertex_source_add_handle =
        glGetUniformLocation(info->program, "u_vertex_source_add");

    info->tex_coord_transform_handle =
        glGetUniformLocation(info->program, "u_tex_coord_transform");
    info->sampler_handle = glGetUniformLocation(info->program, "u_sampler");
  }
  glUseProgram(info->program);

  glUniformMatrix4fv(info->tex_coord_transform_handle, 1, GL_FALSE,
                     transform_matrix);

  // Note: For simplicity, the calculations in this comment block use a single
  // dimension. All calculations trivially extend to the x-y plane.
  // The target subrange in the destination texture has coordinates
  // [xoffset, xoffset + width]. The full destination texture has range
  // [0, dest_width].
  //
  // We want to find A and B such that:
  //   A * X + B = Y
  //   C * Y + D = Z
  //
  // where X = [-1, 1], Z = [xoffset, xoffset + width]
  // and C, D satisfy the relationship C * [-1, 1] + D = [0, dest_width].
  //
  // Math shows:
  //  C = D = dest_width / 2
  //  Y = [(xoffset * 2 / dest_width) - 1,
  //       (xoffset + width) * 2 / dest_width) - 1]
  //  A = width / dest_width
  //  B = (xoffset * 2 + width - dest_width) / dest_width
  glUniform2f(info->vertex_dest_mult_handle, width * 1.f / dest_width,
              height * 1.f / dest_height);
  glUniform2f(info->vertex_dest_add_handle,
              (xoffset * 2.f + width - dest_width) / dest_width,
              (yoffset * 2.f + height - dest_height) / dest_height);

  // Note: For simplicity, the calculations in this comment block use a single
  // dimension. All calculations trivially extend to the x-y plane.
  // The target subrange in the source texture has coordinates [x, x + width].
  // The full source texture has range [0, source_width]. We need to transform
  // the subrange into texture space ([0, M]), assuming that [0, source_width]
  // gets mapped to [0, M]. If source_target == GL_TEXTURE_RECTANGLE_ARB, M =
  // source_width. Otherwise, M = 1.
  //
  // We want to find A and B such that:
  //   A * X + B = Y
  //   C * Y + D = Z
  //
  // where X = [-1, 1], Z = [x, x + width]
  // and C, D satisfy the relationship C * [0, M] + D = [0, source_width].
  //
  // Math shows:
  //   D = 0
  //   C = source_width / M
  //   Y = [x * M / source_width, (x + width) * M / source_width]
  //   B = (x + w/2) * M / source_width
  //   A = (w/2) * M / source_width
  //
  // When flip_y is true, we run the same calcluation, but with Z = [x + width,
  // x]. (I'm intentionally keeping the x-plane notation, although the
  // calculation only gets applied to the y-plane).
  //
  // Math shows:
  //   D = 0
  //   C = source_width / M
  //   Y = [(x + width) * M / source_width, x * M / source_width]
  //   B = (x + w/2) * M / source_width
  //   A = (-w/2) * M / source_width
  //
  // So everything is the same but the sign of A is flipped.
  GLfloat m_x = source_target == GL_TEXTURE_RECTANGLE_ARB ? source_width : 1;
  GLfloat m_y = source_target == GL_TEXTURE_RECTANGLE_ARB ? source_height : 1;
  GLfloat sign_a = flip_y ? -1 : 1;
  glUniform2f(info->vertex_source_mult_handle, width / 2.f * m_x / source_width,
              height / 2.f * m_y / source_height * sign_a);
  glUniform2f(info->vertex_source_add_handle,
              (x + width / 2.f) * m_x / source_width,
              (y + height / 2.f) * m_y / source_height);

  DCHECK(dest_level == 0 || decoder->GetFeatureInfo()->IsES3Capable());
  if (BindFramebufferTexture2D(dest_target, dest_id, dest_level,
                               framebuffer_)) {
#ifndef NDEBUG
    // glValidateProgram of MACOSX validates FBO unlike other platforms, so
    // glValidateProgram must be called after FBO binding. crbug.com/463439
    glValidateProgram(info->program);
    GLint validation_status;
    glGetProgramiv(info->program, GL_VALIDATE_STATUS, &validation_status);
    if (GL_TRUE != validation_status) {
      DLOG(ERROR) << "CopyTextureCHROMIUM: Invalid shader.";
      return;
    }
#endif

    glUniform1i(info->sampler_handle, 0);

    glBindTexture(source_target, source_id);
    DCHECK(source_level == 0 || decoder->GetFeatureInfo()->IsES3Capable());
    if (source_level > 0)
      glTexParameteri(source_target, GL_TEXTURE_BASE_LEVEL, source_level);
    glTexParameterf(source_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(source_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(source_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(source_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    bool need_scissor =
        xoffset || yoffset || width != dest_width || height != dest_height;
    if (need_scissor) {
      glEnable(GL_SCISSOR_TEST);
      glScissor(xoffset, yoffset, width, height);
    } else {
      glDisable(GL_SCISSOR_TEST);
    }
    glViewport(0, 0, dest_width, dest_height);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
  }

  decoder->RestoreAllAttributes();
  decoder->RestoreTextureState(source_id);
  decoder->RestoreTextureState(dest_id);
  decoder->RestoreTextureUnitBindings(0);
  decoder->RestoreActiveTexture();
  decoder->RestoreProgramBindings();
  decoder->RestoreBufferBindings();
  decoder->RestoreFramebufferBindings();
  decoder->RestoreGlobalState();
}

}  // namespace gles2
}  // namespace gpu
