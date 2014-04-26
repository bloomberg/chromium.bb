// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_copy_texture_chromium.h"

#include <string.h>
#include "base/basictypes.h"
#include "gpu/command_buffer/common/types.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

#define SHADER(src)            \
  "#ifdef GL_ES\n"             \
  "precision mediump float;\n" \
  "#endif\n" #src
#define SHADER_2D(src)              \
  "#define SamplerType sampler2D\n" \
  "#define TextureLookup texture2D\n" SHADER(src)
#define SHADER_EXTERNAL_OES(src)                     \
  "#extension GL_OES_EGL_image_external : require\n" \
  "#define SamplerType samplerExternalOES\n"         \
  "#define TextureLookup texture2D\n" SHADER(src)
#define SHADERS(src) \
  { SHADER_2D(src), SHADER_EXTERNAL_OES(src) }

namespace {

const GLfloat kQuadVertices[] = { -1.0f, -1.0f, 0.0f, 1.0f,
                                   1.0f, -1.0f, 0.0f, 1.0f,
                                   1.0f,  1.0f, 0.0f, 1.0f,
                                  -1.0f,  1.0f, 0.0f, 1.0f };

enum ShaderId {
  SHADER_COPY_TEXTURE,
  SHADER_COPY_TEXTURE_FLIP_Y,
  SHADER_COPY_TEXTURE_PREMULTIPLY_ALPHA,
  SHADER_COPY_TEXTURE_UNPREMULTIPLY_ALPHA,
  SHADER_COPY_TEXTURE_PREMULTIPLY_ALPHA_FLIP_Y,
  SHADER_COPY_TEXTURE_UNPREMULTIPLY_ALPHA_FLIP_Y,
  NUM_SHADERS,
};

enum SamplerId {
  SAMPLER_TEXTURE_2D,
  SAMPLER_TEXTURE_EXTERNAL_OES,
  NUM_SAMPLERS,
};

const char* vertex_shader_source =
  SHADER(
    uniform mat4 u_matrix;
    attribute vec4 a_position;
    varying vec2 v_uv;
    void main(void) {
      gl_Position = u_matrix * a_position;
      v_uv = a_position.xy * 0.5 + vec2(0.5, 0.5);
    });

const char* fragment_shader_source[NUM_SHADERS][NUM_SAMPLERS] = {
  // SHADER_COPY_TEXTURE
  SHADERS(
    uniform SamplerType u_texSampler;
    varying vec2 v_uv;
    void main(void) {
      gl_FragColor = TextureLookup(u_texSampler, v_uv.st);
    }),
  // SHADER_COPY_TEXTURE_FLIP_Y
  SHADERS(
    uniform SamplerType u_texSampler;
    varying vec2 v_uv;
    void main(void) {
      gl_FragColor = TextureLookup(u_texSampler, vec2(v_uv.s, 1.0 - v_uv.t));
    }),
  // SHADER_COPY_TEXTURE_PREMULTIPLY_ALPHA
  SHADERS(
    uniform SamplerType u_texSampler;
    varying vec2 v_uv;
    void main(void) {
      gl_FragColor = TextureLookup(u_texSampler, v_uv.st);
      gl_FragColor.rgb *= gl_FragColor.a;
    }),
  // SHADER_COPY_TEXTURE_UNPREMULTIPLY_ALPHA
  SHADERS(
    uniform SamplerType u_texSampler;
    varying vec2 v_uv;
    void main(void) {
      gl_FragColor = TextureLookup(u_texSampler, v_uv.st);
      if (gl_FragColor.a > 0.0)
        gl_FragColor.rgb /= gl_FragColor.a;
    }),
  // SHADER_COPY_TEXTURE_PREMULTIPLY_ALPHA_FLIP_Y
  SHADERS(
    uniform SamplerType u_texSampler;
    varying vec2 v_uv;
    void main(void) {
      gl_FragColor = TextureLookup(u_texSampler, vec2(v_uv.s, 1.0 - v_uv.t));
      gl_FragColor.rgb *= gl_FragColor.a;
    }),
  // SHADER_COPY_TEXTURE_UNPREMULTIPLY_ALPHA_FLIP_Y
  SHADERS(
    uniform SamplerType u_texSampler;
    varying vec2 v_uv;
    void main(void) {
      gl_FragColor = TextureLookup(u_texSampler, vec2(v_uv.s, 1.0 - v_uv.t));
      if (gl_FragColor.a > 0.0)
        gl_FragColor.rgb /= gl_FragColor.a;
    }),
};

// Returns the correct shader id to evaluate the copy operation for
// the CHROMIUM_flipy and premultiply alpha pixel store settings.
ShaderId GetShaderId(bool flip_y,
                     bool premultiply_alpha,
                     bool unpremultiply_alpha) {
  // If both pre-multiply and unpremultiply are requested, then perform no
  // alpha manipulation.
  if (premultiply_alpha && unpremultiply_alpha) {
    premultiply_alpha = false;
    unpremultiply_alpha = false;
  }

  // bit 0: Flip_y
  // bit 1: Premult
  // bit 2: Unpremult
  static ShaderId shader_ids[] = {
      SHADER_COPY_TEXTURE,
      SHADER_COPY_TEXTURE_FLIP_Y,                      // F
      SHADER_COPY_TEXTURE_PREMULTIPLY_ALPHA,           //   P
      SHADER_COPY_TEXTURE_PREMULTIPLY_ALPHA_FLIP_Y,    // F P
      SHADER_COPY_TEXTURE_UNPREMULTIPLY_ALPHA,         //     U
      SHADER_COPY_TEXTURE_UNPREMULTIPLY_ALPHA_FLIP_Y,  // F   U
      SHADER_COPY_TEXTURE,                             //   P U
      SHADER_COPY_TEXTURE_FLIP_Y,                      // F P U
  };

  unsigned index = (flip_y              ? (1 << 0) : 0) |
                   (premultiply_alpha   ? (1 << 1) : 0) |
                   (unpremultiply_alpha ? (1 << 2) : 0);
  return shader_ids[index];
}

SamplerId GetSamplerId(GLenum source_target) {
  switch (source_target) {
    case GL_TEXTURE_2D:
      return SAMPLER_TEXTURE_2D;
    case GL_TEXTURE_EXTERNAL_OES:
      return SAMPLER_TEXTURE_EXTERNAL_OES;
  }

  NOTREACHED();
  return SAMPLER_TEXTURE_2D;
}

void CompileShader(GLuint shader, const char* shader_source) {
  glShaderSource(shader, 1, &shader_source, 0);
  glCompileShader(shader);
#ifndef NDEBUG
  GLint compile_status;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
  if (GL_TRUE != compile_status)
    DLOG(ERROR) << "CopyTextureCHROMIUM: shader compilation failure.";
#endif
}

}  // namespace

namespace gpu {

CopyTextureCHROMIUMResourceManager::CopyTextureCHROMIUMResourceManager()
    : initialized_(false), buffer_id_(0), framebuffer_(0), vertex_shader_(0) {
}

CopyTextureCHROMIUMResourceManager::~CopyTextureCHROMIUMResourceManager() {}

void CopyTextureCHROMIUMResourceManager::Initialize(
    const gles2::GLES2Decoder* decoder) {
  COMPILE_ASSERT(
      kVertexPositionAttrib == 0u,
      Position_attribs_must_be_0);

  // Initialize all of the GPU resources required to perform the copy.
  glGenBuffersARB(1, &buffer_id_);
  glBindBuffer(GL_ARRAY_BUFFER, buffer_id_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices,
               GL_STATIC_DRAW);

  glGenFramebuffersEXT(1, &framebuffer_);

  vertex_shader_ = glCreateShader(GL_VERTEX_SHADER);
  CompileShader(vertex_shader_, vertex_shader_source);

  decoder->RestoreBufferBindings();

  initialized_ = true;
}

void CopyTextureCHROMIUMResourceManager::Destroy() {
  if (!initialized_)
    return;

  glDeleteFramebuffersEXT(1, &framebuffer_);

  glDeleteShader(vertex_shader_);
  for (ProgramMap::const_iterator it = programs_.begin(); it != programs_.end();
       ++it) {
    const ProgramInfo& info = it->second;
    glDeleteProgram(info.program);
  }

  glDeleteBuffersARB(1, &buffer_id_);
}

void CopyTextureCHROMIUMResourceManager::DoCopyTexture(
    const gles2::GLES2Decoder* decoder,
    GLenum source_target,
    GLenum dest_target,
    GLuint source_id,
    GLuint dest_id,
    GLint level,
    GLsizei width,
    GLsizei height,
    bool flip_y,
    bool premultiply_alpha,
    bool unpremultiply_alpha) {
  // Use default transform matrix if no transform passed in.
  const static GLfloat default_matrix[16] = {1.0f, 0.0f, 0.0f, 0.0f,
                                             0.0f, 1.0f, 0.0f, 0.0f,
                                             0.0f, 0.0f, 1.0f, 0.0f,
                                             0.0f, 0.0f, 0.0f, 1.0f};
  DoCopyTextureWithTransform(decoder, source_target, dest_target, source_id,
      dest_id, level, width, height, flip_y, premultiply_alpha,
      unpremultiply_alpha, default_matrix);
}

void CopyTextureCHROMIUMResourceManager::DoCopyTextureWithTransform(
    const gles2::GLES2Decoder* decoder,
    GLenum source_target,
    GLenum dest_target,
    GLuint source_id,
    GLuint dest_id,
    GLint level,
    GLsizei width,
    GLsizei height,
    bool flip_y,
    bool premultiply_alpha,
    bool unpremultiply_alpha,
    const GLfloat transform_matrix[16]) {
  DCHECK(source_target == GL_TEXTURE_2D ||
         source_target == GL_TEXTURE_EXTERNAL_OES);
  if (!initialized_) {
    DLOG(ERROR) << "CopyTextureCHROMIUM: Uninitialized manager.";
    return;
  }

  ShaderId shader_id =
      GetShaderId(flip_y, premultiply_alpha, unpremultiply_alpha);
  SamplerId sampler_id = GetSamplerId(source_target);

  ProgramMapKey key(shader_id, sampler_id);
  ProgramInfo* info = &programs_[key];
  // Create program if necessary.
  if (!info->program) {
    GLuint shader = glCreateShader(GL_FRAGMENT_SHADER);
    CompileShader(shader, fragment_shader_source[shader_id][sampler_id]);
    info->program = glCreateProgram();
    glAttachShader(info->program, vertex_shader_);
    glAttachShader(info->program, shader);
    glBindAttribLocation(info->program, kVertexPositionAttrib, "a_position");
    glLinkProgram(info->program);
#ifndef NDEBUG
    GLint linked;
    glGetProgramiv(info->program, GL_LINK_STATUS, &linked);
    if (!linked)
      DLOG(ERROR) << "CopyTextureCHROMIUM: program link failure.";
#endif
    info->sampler_locations =
        glGetUniformLocation(info->program, "u_texSampler");
    info->matrix_handle = glGetUniformLocation(info->program, "u_matrix");
    glDeleteShader(shader);
  }
  glUseProgram(info->program);

#ifndef NDEBUG
  glValidateProgram(info->program);
  GLint validation_status;
  glGetProgramiv(info->program, GL_VALIDATE_STATUS, &validation_status);
  if (GL_TRUE != validation_status) {
    DLOG(ERROR) << "CopyTextureCHROMIUM: Invalid shader.";
    return;
  }
#endif

  glUniformMatrix4fv(info->matrix_handle, 1, GL_FALSE, transform_matrix);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, dest_id);
  // NVidia drivers require texture settings to be a certain way
  // or they won't report FRAMEBUFFER_COMPLETE.
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer_);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, dest_target,
                            dest_id, level);

#ifndef NDEBUG
  GLenum fb_status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);
  if (GL_FRAMEBUFFER_COMPLETE != fb_status) {
    DLOG(ERROR) << "CopyTextureCHROMIUM: Incomplete framebuffer.";
  } else
#endif
  {
    decoder->ClearAllAttributes();
    glEnableVertexAttribArray(kVertexPositionAttrib);

    glBindBuffer(GL_ARRAY_BUFFER, buffer_id_);
    glVertexAttribPointer(kVertexPositionAttrib, 4, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat), 0);

    glUniform1i(info->sampler_locations, 0);

    glBindTexture(source_target, source_id);
    glTexParameterf(source_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(source_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(source_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(source_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    glViewport(0, 0, width, height);
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

}  // namespace

