// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_shell_renderer.h"

#include "chrome/browser/android/vr_shell/vr_util.h"
#include "ui/gl/gl_bindings.h"

namespace vr_shell {

namespace {

const float kHalfHeight = 0.5f;
const float kHalfWidth = 0.5f;
const float kTextureQuadPosition[18] = {
    -kHalfWidth, kHalfHeight,  0.0f, -kHalfWidth, -kHalfHeight, 0.0f,
    kHalfWidth,  kHalfHeight,  0.0f, -kHalfWidth, -kHalfHeight, 0.0f,
    kHalfWidth,  -kHalfHeight, 0.0f, kHalfWidth,  kHalfHeight,  0.0f};
const int kPositionDataSize = 3;
// Number of vertices passed to glDrawArrays().
const int kVerticesNumber = 6;

const float kTexturedQuadTextureCoordinates[12] = {
    0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f};
const int kTextureCoordinateDataSize = 2;

#define SHADER(Src) #Src
#define OEIE_SHADER(Src) "#extension GL_OES_EGL_image_external : require\n" #Src

const char* GetShaderSource(ShaderID shader) {
  switch (shader) {
    case TEXTURE_QUAD_VERTEX_SHADER:
      return SHADER(uniform mat4 u_CombinedMatrix; attribute vec4 a_Position;
                    attribute vec2 a_TexCoordinate;
                    varying vec2 v_TexCoordinate; void main() {
                      v_TexCoordinate = a_TexCoordinate;
                      gl_Position = u_CombinedMatrix * a_Position;
                    });
    case TEXTURE_QUAD_FRAGMENT_SHADER:
      return OEIE_SHADER(
          precision highp float; uniform samplerExternalOES u_Texture;
          varying vec2 v_TexCoordinate; void main() {
            vec4 texture = texture2D(u_Texture, v_TexCoordinate);
            gl_FragColor = vec4(texture.r, texture.g, texture.b, 1.0);
          });
    default:
      LOG(ERROR) << "Shader source requested for unknown shader";
      return "";
  }
}

}  // namespace

TexturedQuadRenderer::TexturedQuadRenderer() {
  std::string error;
  vertex_shader_handle_ = CompileShader(
      GL_VERTEX_SHADER, GetShaderSource(TEXTURE_QUAD_VERTEX_SHADER), error);
  if (vertex_shader_handle_ == 0) {
    LOG(ERROR) << error;
    exit(1);
  }
  fragment_shader_handle_ = CompileShader(
      GL_FRAGMENT_SHADER, GetShaderSource(TEXTURE_QUAD_FRAGMENT_SHADER), error);
  if (fragment_shader_handle_ == 0) {
    LOG(ERROR) << error;
    exit(1);
  }

  program_handle_ = CreateAndLinkProgram(
      vertex_shader_handle_, fragment_shader_handle_, 4, nullptr, error);
  if (program_handle_ == 0) {
    LOG(ERROR) << error;
    exit(1);
  }
  combined_matrix_handle_ =
      glGetUniformLocation(program_handle_, "u_CombinedMatrix");
  texture_uniform_handle_ = glGetUniformLocation(program_handle_, "u_Texture");
  position_handle_ = glGetAttribLocation(program_handle_, "a_Position");
  texture_coordinate_handle_ =
      glGetAttribLocation(program_handle_, "a_TexCoordinate");
}

void TexturedQuadRenderer::Draw(int texture_data_handle,
                                const gvr::Mat4f& combined_matrix) {
  glUseProgram(program_handle_);

  // Pass in model view project matrix.
  glUniformMatrix4fv(combined_matrix_handle_, 1, false,
                     MatrixToGLArray(combined_matrix).data());

  // Pass in texture coordinate.
  glVertexAttribPointer(texture_coordinate_handle_, kTextureCoordinateDataSize,
                        GL_FLOAT, false, 0, kTexturedQuadTextureCoordinates);
  glEnableVertexAttribArray(texture_coordinate_handle_);

  glVertexAttribPointer(position_handle_, kPositionDataSize, GL_FLOAT, false, 0,
                        kTextureQuadPosition);
  glEnableVertexAttribArray(position_handle_);

  // Link texture data with texture unit.
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_data_handle);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glUniform1i(texture_uniform_handle_, 0);

  glDrawArrays(GL_TRIANGLES, 0, kVerticesNumber);

  glDisableVertexAttribArray(position_handle_);
  glDisableVertexAttribArray(texture_coordinate_handle_);
}

TexturedQuadRenderer::~TexturedQuadRenderer() {
  glDeleteShader(vertex_shader_handle_);
  glDeleteShader(fragment_shader_handle_);
}

VrShellRenderer::VrShellRenderer()
    : textured_quad_renderer_(new TexturedQuadRenderer) {}

VrShellRenderer::~VrShellRenderer() {}

}  // namespace vr_shell
