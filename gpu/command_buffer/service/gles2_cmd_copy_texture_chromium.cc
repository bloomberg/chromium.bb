// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_copy_texture_chromium.h"

#include <string.h>
#include "base/basictypes.h"
#include "gpu/command_buffer/common/types.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

#define SHADER0(src) \
    "#ifdef GL_ES\n"\
    "precision mediump float;\n"\
    "#endif\n"\
    #src
#define SHADER(src) { false, SHADER0(src), }
#define SHADER_EXTERNAL_OES0(src) \
    "#extension GL_OES_EGL_image_external : require\n"\
    "#ifdef GL_ES\n"\
    "precision mediump float;\n"\
    "#endif\n"\
    #src
#define SHADER_EXTERNAL_OES(src) { true, SHADER_EXTERNAL_OES0(src), }

namespace {

const GLfloat kQuadVertices[] = { -1.0f, -1.0f, 0.0f, 1.0f,
                                   1.0f, -1.0f, 0.0f, 1.0f,
                                   1.0f,  1.0f, 0.0f, 1.0f,
                                  -1.0f,  1.0f, 0.0f, 1.0f };

enum ProgramId {
  PROGRAM_COPY_TEXTURE,
  PROGRAM_COPY_TEXTURE_FLIP_Y,
  PROGRAM_COPY_TEXTURE_PREMULTIPLY_ALPHA,
  PROGRAM_COPY_TEXTURE_UNPREMULTIPLY_ALPHA,
  PROGRAM_COPY_TEXTURE_PREMULTIPLY_ALPHA_FLIPY,
  PROGRAM_COPY_TEXTURE_UNPREMULTIPLY_ALPHA_FLIPY,
  PROGRAM_COPY_TEXTURE_OES,
  PROGRAM_COPY_TEXTURE_OES_FLIP_Y,
  PROGRAM_COPY_TEXTURE_OES_PREMULTIPLY_ALPHA,
  PROGRAM_COPY_TEXTURE_OES_UNPREMULTIPLY_ALPHA,
  PROGRAM_COPY_TEXTURE_OES_PREMULTIPLY_ALPHA_FLIPY,
  PROGRAM_COPY_TEXTURE_OES_UNPREMULTIPLY_ALPHA_FLIPY,
};

struct ShaderInfo {
  bool needs_egl_image_external;
  const char* source;
};

const ShaderInfo shader_infos[] = {
  // VERTEX_SHADER_POS_TEX
  SHADER(
    attribute vec4 a_position;
    varying vec2 v_uv;
    void main(void) {
      gl_Position = a_position;
      v_uv = a_position.xy * 0.5 + vec2(0.5, 0.5);
    }),
  // FRAGMENT_SHADER_TEX
  SHADER(
    uniform sampler2D u_texSampler;
    varying vec2 v_uv;
    void main(void) {
      gl_FragColor = texture2D(u_texSampler, v_uv.st);
    }),
  // FRAGMENT_SHADER_TEX_FLIP_Y
  SHADER(
    uniform sampler2D u_texSampler;
    varying vec2 v_uv;
    void main(void) {
      gl_FragColor = texture2D(u_texSampler, vec2(v_uv.s, 1.0 - v_uv.t));
    }),
  // FRAGMENT_SHADER_TEX_PREMULTIPLY_ALPHA
  SHADER(
    uniform sampler2D u_texSampler;
    varying vec2 v_uv;
    void main(void) {
      gl_FragColor = texture2D(u_texSampler, v_uv.st);
      gl_FragColor.rgb *= gl_FragColor.a;
    }),
  // FRAGMENT_SHADER_TEX_UNPREMULTIPLY_ALPHA
  SHADER(
    uniform sampler2D u_texSampler;
    varying vec2 v_uv;
    void main(void) {
      gl_FragColor = texture2D(u_texSampler, v_uv.st);
      if (gl_FragColor.a > 0.0)
        gl_FragColor.rgb /= gl_FragColor.a;
    }),
  // FRAGMENT_SHADER_TEX_PREMULTIPLY_ALPHA_FLIP_Y
  SHADER(
    uniform sampler2D u_texSampler;
    varying vec2 v_uv;
    void main(void) {
      gl_FragColor = texture2D(u_texSampler, vec2(v_uv.s, 1.0 - v_uv.t));
      gl_FragColor.rgb *= gl_FragColor.a;
    }),
  // FRAGMENT_SHADER_TEX_UNPREMULTIPLY_ALPHA_FLIP_Y
  SHADER(
    uniform sampler2D u_texSampler;
    varying vec2 v_uv;
    void main(void) {
      gl_FragColor = texture2D(u_texSampler, vec2(v_uv.s, 1.0 - v_uv.t));
      if (gl_FragColor.a > 0.0)
        gl_FragColor.rgb /= gl_FragColor.a;
    }),
  // FRAGMENT_SHADER_TEX_OES
  SHADER_EXTERNAL_OES(
    precision mediump float;
    uniform samplerExternalOES u_texSampler;
    varying vec2 v_uv;
    void main(void) {
      gl_FragColor = texture2D(u_texSampler, v_uv.st);
    }),
  // FRAGMENT_SHADER_TEX_OES_FLIP_Y
  SHADER_EXTERNAL_OES(
     precision mediump float;
     uniform samplerExternalOES u_texSampler;
     varying vec2 v_uv;
     void main(void) {
       gl_FragColor =
           texture2D(u_texSampler, vec2(v_uv.s, 1.0 - v_uv.t));
     }),
  // FRAGMENT_SHADER_TEX_OES_PREMULTIPLY_ALPHA
  SHADER_EXTERNAL_OES(
     precision mediump float;
     uniform samplerExternalOES u_texSampler;
     varying vec2 v_uv;
     void main(void) {
       gl_FragColor = texture2D(u_texSampler, v_uv.st);
       gl_FragColor.rgb *= gl_FragColor.a;
     }),
  // FRAGMENT_SHADER_TEX_OES_UNPREMULTIPLY_ALPHA
  SHADER_EXTERNAL_OES(
    precision mediump float;
    uniform samplerExternalOES u_texSampler;
    varying vec2 v_uv;
    void main(void) {
      gl_FragColor = texture2D(u_texSampler, v_uv.st);
      if (gl_FragColor.a > 0.0)
        gl_FragColor.rgb /= gl_FragColor.a;
    }),
  // FRAGMENT_SHADER_TEX_OES_PREMULTIPLY_ALPHA_FLIP_Y
  SHADER_EXTERNAL_OES(
      precision mediump float;
      uniform samplerExternalOES u_texSampler;
      varying vec2 v_uv;
      void main(void) {
        gl_FragColor =
            texture2D(u_texSampler, vec2(v_uv.s, 1.0 - v_uv.t));
        gl_FragColor.rgb *= gl_FragColor.a;
      }),
  // FRAGMENT_SHADER_TEX_OES_UNPREMULTIPLY_ALPHA_FLIP_Y
  SHADER_EXTERNAL_OES(
      precision mediump float;
      uniform samplerExternalOES u_texSampler;
      varying vec2 v_uv;
      void main(void) {
        gl_FragColor =
            texture2D(u_texSampler, vec2(v_uv.s, 1.0 - v_uv.t));
        if (gl_FragColor.a > 0.0)
          gl_FragColor.rgb /= gl_FragColor.a;
      }),
};

const int kNumShaders = arraysize(shader_infos);

// Returns the correct program to evaluate the copy operation for
// the CHROMIUM_flipy and premultiply alpha pixel store settings.
ProgramId GetProgram(
    bool flip_y,
    bool premultiply_alpha,
    bool unpremultiply_alpha,
    bool is_source_external_oes) {
  // If both pre-multiply and unpremultiply are requested, then perform no
  // alpha manipulation.
  if (premultiply_alpha && unpremultiply_alpha) {
    premultiply_alpha = false;
    unpremultiply_alpha = false;
  }

  // bit 0: Flip_y
  // bit 1: Premult
  // bit 2: Unpremult
  // bit 3: External_oes
  static ProgramId program_ids[] = {
    PROGRAM_COPY_TEXTURE,
    PROGRAM_COPY_TEXTURE_FLIP_Y,                         // F
    PROGRAM_COPY_TEXTURE_PREMULTIPLY_ALPHA,              //   P
    PROGRAM_COPY_TEXTURE_PREMULTIPLY_ALPHA_FLIPY,        // F P
    PROGRAM_COPY_TEXTURE_UNPREMULTIPLY_ALPHA,            //     U
    PROGRAM_COPY_TEXTURE_UNPREMULTIPLY_ALPHA_FLIPY,      // F   U
    PROGRAM_COPY_TEXTURE,                                //   P U
    PROGRAM_COPY_TEXTURE,                                // F P U
    PROGRAM_COPY_TEXTURE_OES,                            //       E
    PROGRAM_COPY_TEXTURE_OES_FLIP_Y,                     // F     E
    PROGRAM_COPY_TEXTURE_OES_PREMULTIPLY_ALPHA,          //   P   E
    PROGRAM_COPY_TEXTURE_OES_PREMULTIPLY_ALPHA_FLIPY,    // F P   E
    PROGRAM_COPY_TEXTURE_OES_UNPREMULTIPLY_ALPHA,        //     U E
    PROGRAM_COPY_TEXTURE_OES_UNPREMULTIPLY_ALPHA_FLIPY,  // F   U E
    PROGRAM_COPY_TEXTURE_OES,                            //   P U E
    PROGRAM_COPY_TEXTURE_OES,                            // F P U E
  };

  unsigned index = (flip_y                 ? (1 << 0) : 0) |
                   (premultiply_alpha      ? (1 << 1) : 0) |
                   (unpremultiply_alpha    ? (1 << 2) : 0) |
                   (is_source_external_oes ? (1 << 3) : 0);
  return program_ids[index];
}

}  // namespace

namespace gpu {

void CopyTextureCHROMIUMResourceManager::Initialize(
    const gles2::GLES2Decoder* decoder) {
  COMPILE_ASSERT(
      kVertexPositionAttrib == 0u,
      Position_attribs_must_be_0);

  const char* extensions =
      reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
  bool have_egl_image_external = extensions &&
      strstr(extensions, "GL_OES_EGL_image_external");

  // Initialize all of the GPU resources required to perform the copy.
  glGenBuffersARB(1, &buffer_id_);
  glBindBuffer(GL_ARRAY_BUFFER, buffer_id_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices,
               GL_STATIC_DRAW);

  glGenFramebuffersEXT(1, &framebuffer_);

  // TODO(gman): Init these on demand.
  GLuint shaders[kNumShaders];
  for (int shader = 0; shader < kNumShaders; ++shader) {
    shaders[shader] = glCreateShader(
        shader == 0 ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
    const ShaderInfo& info = shader_infos[shader];
    if (info.needs_egl_image_external && !have_egl_image_external) {
      continue;
    }
    const char* shader_source = shader_infos[shader].source;
    glShaderSource(shaders[shader], 1, &shader_source, 0);
    glCompileShader(shaders[shader]);
#ifndef NDEBUG
    GLint compile_status;
    glGetShaderiv(shaders[shader], GL_COMPILE_STATUS, &compile_status);
    if (GL_TRUE != compile_status)
      DLOG(ERROR) << "CopyTextureCHROMIUM: shader compilation failure.";
#endif
  }

  // TODO(gman): Init these on demand.
  for (int program = 0; program < kNumPrograms; ++program) {
    const ShaderInfo& info = shader_infos[program + 1];
    if (info.needs_egl_image_external && !have_egl_image_external) {
      continue;
    }
    programs_[program] = glCreateProgram();
    glAttachShader(programs_[program], shaders[0]);
    glAttachShader(programs_[program], shaders[program + 1]);

    glBindAttribLocation(programs_[program], kVertexPositionAttrib,
                         "a_position");

    glLinkProgram(programs_[program]);
#ifndef NDEBUG
    GLint linked;
    glGetProgramiv(programs_[program], GL_LINK_STATUS, &linked);
    if (!linked)
      DLOG(ERROR) << "CopyTextureCHROMIUM: program link failure.";
#endif

    sampler_locations_[program] = glGetUniformLocation(programs_[program],
                                                      "u_texSampler");
  }

  for (int shader = 0; shader < kNumShaders; ++shader)
    glDeleteShader(shaders[shader]);

  decoder->RestoreBufferBindings();

  initialized_ = true;
}

void CopyTextureCHROMIUMResourceManager::Destroy() {
  if (!initialized_)
    return;

  glDeleteFramebuffersEXT(1, &framebuffer_);

  for (int program = 0; program < kNumPrograms; ++program)
    glDeleteProgram(programs_[program]);

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
  DCHECK(source_target == GL_TEXTURE_2D ||
         source_target == GL_TEXTURE_EXTERNAL_OES);
  if (!initialized_) {
    DLOG(ERROR) << "CopyTextureCHROMIUM: Uninitialized manager.";
    return;
  }

  GLuint program = GetProgram(
      flip_y, premultiply_alpha, unpremultiply_alpha,
      source_target == GL_TEXTURE_EXTERNAL_OES);
  glUseProgram(programs_[program]);

#ifndef NDEBUG
  glValidateProgram(programs_[program]);
  GLint validation_status;
  glGetProgramiv(programs_[program], GL_VALIDATE_STATUS, &validation_status);
  if (GL_TRUE != validation_status) {
    DLOG(ERROR) << "CopyTextureCHROMIUM: Invalid shader.";
    return;
  }
#endif

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
    glEnableVertexAttribArray(kVertexPositionAttrib);

    glBindBuffer(GL_ARRAY_BUFFER, buffer_id_);
    glVertexAttribPointer(kVertexPositionAttrib, 4, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat), 0);

    glUniform1i(sampler_locations_[program], 0);

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

  decoder->RestoreAttribute(kVertexPositionAttrib);
  decoder->RestoreTextureUnitBindings(0);
  decoder->RestoreTextureState(source_id);
  decoder->RestoreTextureState(dest_id);
  decoder->RestoreActiveTexture();
  decoder->RestoreProgramBindings();
  decoder->RestoreBufferBindings();
  decoder->RestoreFramebufferBindings();
  decoder->RestoreGlobalState();
}

}  // namespace

