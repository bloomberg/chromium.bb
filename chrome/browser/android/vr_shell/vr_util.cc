// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_util.h"

#include <array>
#include <cmath>

#include "third_party/gvr-android-sdk/src/ndk-beta/include/vr/gvr/capi/include/gvr_types.h"

namespace vr_shell {

// Internal matrix layout:
//
//   m[0][0], m[0][1], m[0][2], m[0][3],
//   m[1][0], m[1][1], m[1][2], m[1][3],
//   m[2][0], m[2][1], m[2][2], m[2][3],
//   m[3][0], m[3][1], m[3][2], m[3][3],
//
// The translation component is in the right column m[i][3].
//
// The bottom row m[3][i] is (0, 0, 0, 1) for non-perspective transforms.
//
// These matrices are intended to be used to premultiply column vectors
// for transforms, so successive transforms need to be left-multiplied.

void SetIdentityM(gvr::Mat4f& mat) {
  float* m = (float*)mat.m;
  for (int i = 0; i < 16; i++) {
    m[i] = 0;
  }
  for (int i = 0; i < 16; i += 5) {
    m[i] = 1.0f;
  }
}

// Left multiply a translation matrix.
void TranslateM(gvr::Mat4f& tmat, gvr::Mat4f& mat, float x, float y, float z) {
  if (&tmat != &mat) {
    for (int i = 0; i < 4; ++i) {
      for (int j = 0; j < 4; ++j) {
        tmat.m[i][j] = mat.m[i][j];
      }
    }
  }
  tmat.m[0][3] += x;
  tmat.m[1][3] += y;
  tmat.m[2][3] += z;
}

// Right multiply a translation matrix.
void TranslateMRight(gvr::Mat4f& tmat,
                     gvr::Mat4f& mat,
                     float x,
                     float y,
                     float z) {
  if (&tmat != &mat) {
    for (int i = 0; i < 4; ++i) {
      for (int j = 0; j < 3; ++j) {
        tmat.m[i][j] = mat.m[i][j];
      }
    }
  }

  for (int i = 0; i < 4; i++) {
    tmat.m[i][3] =
        mat.m[i][0] * x + mat.m[i][1] * y + mat.m[i][2] * z + mat.m[i][3];
  }
}

// Left multiply a scale matrix.
void ScaleM(gvr::Mat4f& tmat, const gvr::Mat4f& mat,
            float x, float y, float z) {
  if (&tmat != &mat) {
    for (int i = 0; i < 4; ++i) {
      for (int j = 0; j < 3; ++j) {
        tmat.m[i][j] = mat.m[i][j];
      }
    }
  }
  // Multiply all rows including translation components.
  for (int j = 0; j < 4; ++j) {
    tmat.m[0][j] *= x;
    tmat.m[1][j] *= y;
    tmat.m[2][j] *= z;
  }
}

// Right multiply a scale matrix.
void ScaleMRight(gvr::Mat4f& tmat, const gvr::Mat4f& mat,
                 float x, float y, float z) {
  if (&tmat != &mat) {
    for (int i = 0; i < 4; ++i) {
      for (int j = 0; j < 3; ++j) {
        tmat.m[i][j] = mat.m[i][j];
      }
    }
  }
  // Multiply columns, don't change translation components.
  for (int i = 0; i < 3; ++i) {
    tmat.m[i][0] *= x;
    tmat.m[i][1] *= y;
    tmat.m[i][2] *= z;
  }
}

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

std::array<float, 4> MatrixVectorMul(const gvr::Mat4f& matrix,
                                     const std::array<float, 4>& vec) {
  std::array<float, 4> result;
  for (int i = 0; i < 4; ++i) {
    result[i] = 0;
    for (int k = 0; k < 4; ++k) {
      result[i] += matrix.m[i][k] * vec[k];
    }
  }
  return result;
}

std::array<float, 3> MatrixVectorMul(const gvr::Mat4f& matrix,
                                     const std::array<float, 3>& vec) {
  // Use homogeneous coordinates for the multiplication.
  std::array<float, 4> vec_h = {{vec[0], vec[1], vec[2], 1.0f}};
  std::array<float, 4> result;
  for (int i = 0; i < 4; ++i) {
    result[i] = 0;
    for (int k = 0; k < 4; ++k) {
      result[i] += matrix.m[i][k] * vec_h[k];
    }
  }
  // Convert back from homogeneous coordinates.
  float rw = 1.0f / result[3];
  return {{rw * result[0], rw * result[1], rw * result[2]}};
}

gvr::Vec3f MatrixVectorMul(const gvr::Mat4f& m, const gvr::Vec3f& v) {
  gvr::Vec3f res;
  res.x = m.m[0][0] * v.x + m.m[0][1] * v.y + m.m[0][2] * v.z + m.m[0][3];
  res.y = m.m[1][0] * v.x + m.m[1][1] * v.y + m.m[1][2] * v.z + m.m[1][3];
  res.z = m.m[2][0] * v.x + m.m[2][1] * v.y + m.m[2][2] * v.z + m.m[2][3];
  return res;
}

// Rotation only, ignore translation components.
gvr::Vec3f MatrixVectorRotate(const gvr::Mat4f& m, const gvr::Vec3f& v) {
  gvr::Vec3f res;
  res.x = m.m[0][0] * v.x + m.m[0][1] * v.y + m.m[0][2] * v.z;
  res.y = m.m[1][0] * v.x + m.m[1][1] * v.y + m.m[1][2] * v.z;
  res.z = m.m[2][0] * v.x + m.m[2][1] * v.y + m.m[2][2] * v.z;
  return res;
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

gvr::Vec3f getForwardVector(const gvr::Mat4f& matrix) {
  gvr::Vec3f forward;
  float* fp = &forward.x;
  // Same as multiplying the inverse of the rotation component of the matrix by
  // (0, 0, -1, 0).
  for (int i = 0; i < 3; ++i) {
    fp[i] = -matrix.m[2][i];
  }
  return forward;
}

/**
 * Provides the relative translation of the head as a 3x1 vector.
 *
 */
gvr::Vec3f getTranslation(const gvr::Mat4f& matrix) {
  gvr::Vec3f translation;
  float* tp = &translation.x;
  // Same as multiplying the matrix by (0, 0, 0, 1).
  for (int i = 0; i < 3; ++i) {
    tp[i] = matrix.m[i][3];
  }
  return translation;
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

float VectorLength(const gvr::Vec3f& vec) {
  return sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
}

void NormalizeVector(gvr::Vec3f& vec) {
  float len = VectorLength(vec);
  vec.x /= len;
  vec.y /= len;
  vec.z /= len;
}

float VectorDot(const gvr::Vec3f& a, const gvr::Vec3f& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

void NormalizeQuat(gvr::Quatf& quat) {
  float len = sqrt(quat.qx * quat.qx + quat.qy * quat.qy + quat.qz * quat.qz +
                   quat.qw * quat.qw);
  quat.qx /= len;
  quat.qy /= len;
  quat.qz /= len;
  quat.qw /= len;
}

gvr::Quatf QuatFromAxisAngle(float x, float y, float z, float angle) {
  gvr::Quatf res;
  float s = sin(angle / 2);
  res.qx = x * s;
  res.qy = y * s;
  res.qz = z * s;
  res.qw = cos(angle / 2);
  return res;
}

gvr::Quatf QuatMultiply(const gvr::Quatf& a, const gvr::Quatf& b) {
  gvr::Quatf res;
  res.qw = a.qw * b.qw - a.qx * b.qx - a.qy * b.qy - a.qz * b.qz;
  res.qx = a.qw * b.qx + a.qx * b.qw + a.qy * b.qz - a.qz * b.qy;
  res.qy = a.qw * b.qy - a.qx * b.qz + a.qy * b.qw + a.qz * b.qx;
  res.qz = a.qw * b.qz + a.qx * b.qy - a.qy * b.qx + a.qz * b.qw;
  return res;
}

gvr::Mat4f QuatToMatrix(const gvr::Quatf& quat) {
  const float x2 = quat.qx * quat.qx;
  const float y2 = quat.qy * quat.qy;
  const float z2 = quat.qz * quat.qz;
  const float xy = quat.qx * quat.qy;
  const float xz = quat.qx * quat.qz;
  const float xw = quat.qx * quat.qw;
  const float yz = quat.qy * quat.qz;
  const float yw = quat.qy * quat.qw;
  const float zw = quat.qz * quat.qw;

  const float m11 = 1.0f - 2.0f * y2 - 2.0f * z2;
  const float m12 = 2.0f * (xy - zw);
  const float m13 = 2.0f * (xz + yw);
  const float m21 = 2.0f * (xy + zw);
  const float m22 = 1.0f - 2.0f * x2 - 2.0f * z2;
  const float m23 = 2.0f * (yz - xw);
  const float m31 = 2.0f * (xz - yw);
  const float m32 = 2.0f * (yz + xw);
  const float m33 = 1.0f - 2.0f * x2 - 2.0f * y2;

  float ret[16] = {m11, m12, m13, 0.0f, m21,  m22,  m23,  0.0f,
                   m31, m32, m33, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

  return *((gvr::Mat4f*)&ret);
}

}  // namespace vr_shell
