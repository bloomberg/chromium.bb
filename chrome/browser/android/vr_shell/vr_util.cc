// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <cmath>

#include "chrome/browser/android/vr_shell/vr_util.h"
#include "third_party/gvr-android-sdk/src/ndk-beta/include/vr/gvr/capi/include/gvr_types.h"

namespace vr_shell {

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

gvr::Mat4f MatrixTranspose(const gvr::Mat4f& mat) {
  gvr::Mat4f result;
  for (int i = 0; i < 4; ++i) {
    for (int k = 0; k < 4; ++k) {
      result.m[i][k] = mat.m[k][i];
    }
  }
  return result;
}

gvr::Mat4f MatrixMul(const gvr::Mat4f& matrix1, const gvr::Mat4f& matrix2) {
  gvr::Mat4f result;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      result.m[i][j] = 0.0f;
      for (int k = 0; k < 4; ++k) {
        result.m[i][j] += matrix1.m[i][k] * matrix2.m[k][j];
      }
    }
  }
  return result;
}

gvr::Mat4f PerspectiveMatrixFromView(const gvr::Rectf& fov,
                                     float z_near,
                                     float z_far) {
  gvr::Mat4f result;
  const float x_left = -std::tan(fov.left * M_PI / 180.0f) * z_near;
  const float x_right = std::tan(fov.right * M_PI / 180.0f) * z_near;
  const float y_bottom = -std::tan(fov.bottom * M_PI / 180.0f) * z_near;
  const float y_top = std::tan(fov.top * M_PI / 180.0f) * z_near;

  assert(x_left < x_right && y_bottom < y_top && z_near < z_far &&
         z_near > 0.0f && z_far > 0.0f);
  const float X = (2 * z_near) / (x_right - x_left);
  const float Y = (2 * z_near) / (y_top - y_bottom);
  const float A = (x_right + x_left) / (x_right - x_left);
  const float B = (y_top + y_bottom) / (y_top - y_bottom);
  const float C = (z_near + z_far) / (z_near - z_far);
  const float D = (2 * z_near * z_far) / (z_near - z_far);

  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      result.m[i][j] = 0.0f;
    }
  }
  result.m[0][0] = X;
  result.m[0][2] = A;
  result.m[1][1] = Y;
  result.m[1][2] = B;
  result.m[2][2] = C;
  result.m[2][3] = D;
  result.m[3][2] = -1;

  return result;
}

gvr::Rectf ModulateRect(const gvr::Rectf& rect, float width, float height) {
  gvr::Rectf result = {rect.left * width, rect.right * width,
                       rect.bottom * height, rect.top * height};
  return result;
}

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
      glGetShaderInfoLog(shader_handle, info_log_length, NULL, str_info_log);
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
                            int num_attributes,
                            const GLchar** attributes,
                            std::string& error) {
  GLuint program_handle = glCreateProgram();

  if (program_handle != 0) {
    // Bind the vertex shader to the program.
    glAttachShader(program_handle, vertext_shader_handle);

    // Bind the fragment shader to the program.
    glAttachShader(program_handle, fragment_shader_handle);

    // Bind attributes. This is optional, no need to supply them if
    // using glGetAttribLocation to look them up. Useful for a single
    // vertex array object (VAO) that is used with multiple shaders.
    if (attributes != nullptr) {
      for (int i = 0; i < num_attributes; i++) {
        glBindAttribLocation(program_handle, i, attributes[i]);
      }
    }

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
      glGetProgramInfoLog(program_handle, info_log_length, NULL, str_info_log);
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
