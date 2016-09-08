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

const float kWebVrVertices[32] = {
  //   x     y    u,   v
    -1.f,  1.f, 0.f, 0.f, // Left Eye
    -1.f, -1.f, 0.f, 1.f,
     0.f, -1.f, 1.f, 1.f,
     0.f,  1.f, 1.f, 0.f,

     0.f,  1.f, 0.f, 0.f, // Right Eye
     0.f, -1.f, 0.f, 1.f,
     1.f, -1.f, 1.f, 1.f,
     1.f,  1.f, 1.f, 0.f };
const int kWebVrVerticesSize = sizeof(float) * 32;

#define SHADER(Src) #Src
#define OEIE_SHADER(Src) "#extension GL_OES_EGL_image_external : require\n" #Src
#define VOID_OFFSET(x) reinterpret_cast<void*>(x)

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
    case vr_shell::ShaderID::WEBVR_VERTEX_SHADER:
      return SHADER(
          attribute vec2 a_Position;
          attribute vec2 a_TexCoordinate;
          uniform vec4 u_SrcRect;
          varying vec2 v_TexCoordinate;

          void main() {
            v_TexCoordinate = u_SrcRect.xy + (a_TexCoordinate * u_SrcRect.zw);
            gl_Position = vec4(a_Position, 0.0, 1.0);
          });
    case vr_shell::ShaderID::WEBVR_FRAGMENT_SHADER:
      return OEIE_SHADER(
          precision highp float;
          uniform samplerExternalOES u_Texture;
          varying vec2 v_TexCoordinate;

          void main() {
            gl_FragColor = texture2D(u_Texture, v_TexCoordinate);
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
  CHECK(vertex_shader_handle_) << error;

  fragment_shader_handle_ = CompileShader(
      GL_FRAGMENT_SHADER, GetShaderSource(TEXTURE_QUAD_FRAGMENT_SHADER), error);
  CHECK(fragment_shader_handle_) << error;

  program_handle_ = CreateAndLinkProgram(
      vertex_shader_handle_, fragment_shader_handle_, 4, nullptr, error);
  CHECK(program_handle_) << error;

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

WebVrRenderer::WebVrRenderer() {
  left_bounds_ = { 0.0f, 0.0f, 0.5f, 1.0f };
  right_bounds_ = { 0.5f, 0.0f, 0.5f, 1.0f };

  std::string error;
  GLuint vertex_shader_handle = CompileShader(
      GL_VERTEX_SHADER, GetShaderSource(WEBVR_VERTEX_SHADER), error);
  CHECK(vertex_shader_handle) << error;

  GLuint fragment_shader_handle = CompileShader(
      GL_FRAGMENT_SHADER, GetShaderSource(WEBVR_FRAGMENT_SHADER), error);
  CHECK(fragment_shader_handle) << error;

  program_handle_ = CreateAndLinkProgram(
      vertex_shader_handle, fragment_shader_handle, 2, nullptr, error);
  CHECK(program_handle_) << error;

  // Once the program is linked the shader objects are no longer needed
  glDeleteShader(vertex_shader_handle);
  glDeleteShader(fragment_shader_handle);

  tex_uniform_handle_ = glGetUniformLocation(program_handle_, "u_Texture");
  src_rect_uniform_handle_ = glGetUniformLocation(program_handle_, "u_SrcRect");
  position_handle_ = glGetAttribLocation(program_handle_, "a_Position");
  texcoord_handle_ = glGetAttribLocation(program_handle_, "a_TexCoordinate");

  // TODO(bajones): Figure out why this need to be restored.
  GLint old_buffer;
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &old_buffer);

  glGenBuffersARB(1, &vertex_buffer_);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER, kWebVrVerticesSize, kWebVrVertices,
      GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, old_buffer);
}

// Draw the stereo WebVR frame
void WebVrRenderer::Draw(int texture_handle) {
  // TODO(bajones): Figure out why this need to be restored.
  GLint old_buffer;
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &old_buffer);

  glUseProgram(program_handle_);

  // Bind vertex attributes
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);

  glEnableVertexAttribArray(position_handle_);
  glEnableVertexAttribArray(texcoord_handle_);

  glVertexAttribPointer(position_handle_, POSITION_ELEMENTS, GL_FLOAT, false,
      VERTEX_STRIDE, VOID_OFFSET(POSITION_OFFSET));
  glVertexAttribPointer(texcoord_handle_, TEXCOORD_ELEMENTS, GL_FLOAT, false,
      VERTEX_STRIDE, VOID_OFFSET(TEXCOORD_OFFSET));

  // Bind texture.
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_handle);
  glUniform1i(tex_uniform_handle_, 0);

  // TODO(bajones): Should be able handle both eyes in a single draw call.
  // Left eye
  glUniform4fv(src_rect_uniform_handle_, 1, (float*)(&left_bounds_));
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

  // Right eye
  glUniform4fv(src_rect_uniform_handle_, 1, (float*)(&right_bounds_));
  glDrawArrays(GL_TRIANGLE_FAN, 4, 4);

  glDisableVertexAttribArray(position_handle_);
  glDisableVertexAttribArray(texcoord_handle_);

  glBindBuffer(GL_ARRAY_BUFFER, old_buffer);
}

void WebVrRenderer::UpdateTextureBounds(int eye, const gvr::Rectf& bounds) {
  if (eye == 0) {
    left_bounds_ = bounds;
  } else if (eye == 1) {
    right_bounds_ = bounds;
  }
}

WebVrRenderer::~WebVrRenderer() {
  glDeleteBuffersARB(1, &vertex_buffer_);
  glDeleteProgram(program_handle_);
}

VrShellRenderer::VrShellRenderer()
    : textured_quad_renderer_(new TexturedQuadRenderer),
      webvr_renderer_(new WebVrRenderer) {}

VrShellRenderer::~VrShellRenderer() {}

}  // namespace vr_shell
