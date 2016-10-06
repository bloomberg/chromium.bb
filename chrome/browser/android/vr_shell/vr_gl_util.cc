// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_gl_util.h"

namespace vr_shell {

// This code is adapted from the GVR Treasure Hunt demo source.
std::array<float, 16> MatrixToGLArray(const gvr::Mat4f& matrix) {
  // Note that this performs a *transpose* to a column-major matrix array, as
  // expected by GL. The input matrix has translation components at [i][3] for
  // use with row vectors and premultiplied transforms. In the output, the
  // translation elements are at the end at positions 3*4+i.
  std::array<float, 16> result;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      result[j * 4 + i] = matrix.m[i][j];
    }
  }
  return result;
}

// This code is adapted from the GVR Treasure Hunt demo source.
gvr::Rectf ModulateRect(const gvr::Rectf& rect, float width, float height) {
  gvr::Rectf result = {rect.left * width, rect.right * width,
                       rect.bottom * height, rect.top * height};
  return result;
}

// This code is adapted from the GVR Treasure Hunt demo source.
gvr::Recti CalculatePixelSpaceRect(const gvr::Sizei& texture_size,
                                   const gvr::Rectf& texture_rect) {
  float width = static_cast<float>(texture_size.width);
  float height = static_cast<float>(texture_size.height);
  gvr::Rectf rect = ModulateRect(texture_rect, width, height);
  gvr::Recti result = {
      static_cast<int>(rect.left), static_cast<int>(rect.right),
      static_cast<int>(rect.bottom), static_cast<int>(rect.top)};
  return result;
}

GLuint CompileShader(GLenum shader_type,
                     const GLchar* shader_source,
                     std::string& error) {
  GLuint shader_handle = glCreateShader(shader_type);
  if (shader_handle != 0) {
    // Pass in the shader source.
    int len = strlen(shader_source);
    glShaderSource(shader_handle, 1, &shader_source, &len);
    // Compile the shader.
    glCompileShader(shader_handle);
    // Get the compilation status.
    GLint status;
    glGetShaderiv(shader_handle, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
      GLint info_log_length;
      glGetShaderiv(shader_handle, GL_INFO_LOG_LENGTH, &info_log_length);
      GLchar* str_info_log = new GLchar[info_log_length + 1];
      glGetShaderInfoLog(shader_handle, info_log_length, nullptr, str_info_log);
      error = "Error compiling shader: ";
      error += str_info_log;
      delete[] str_info_log;
      glDeleteShader(shader_handle);
      shader_handle = 0;
    }
  }

  return shader_handle;
}

GLuint CreateAndLinkProgram(GLuint vertext_shader_handle,
                            GLuint fragment_shader_handle,
                            std::string& error) {
  GLuint program_handle = glCreateProgram();

  if (program_handle != 0) {
    // Bind the vertex shader to the program.
    glAttachShader(program_handle, vertext_shader_handle);

    // Bind the fragment shader to the program.
    glAttachShader(program_handle, fragment_shader_handle);

    // Link the two shaders together into a program.
    glLinkProgram(program_handle);

    // Get the link status.
    GLint link_status;
    glGetProgramiv(program_handle, GL_LINK_STATUS, &link_status);

    // If the link failed, delete the program.
    if (link_status == GL_FALSE) {
      GLint info_log_length;
      glGetProgramiv(program_handle, GL_INFO_LOG_LENGTH, &info_log_length);

      GLchar* str_info_log = new GLchar[info_log_length + 1];
      glGetProgramInfoLog(program_handle, info_log_length, nullptr,
                          str_info_log);
      error = "Error compiling program: ";
      error += str_info_log;
      delete[] str_info_log;
      glDeleteProgram(program_handle);
      program_handle = 0;
    }
  }

  return program_handle;
}

}  // namespace vr_shell
