// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_math.h"

#include <cmath>

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
  float* m = reinterpret_cast<float*>(mat.m);
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
void ScaleM(gvr::Mat4f& tmat,
            const gvr::Mat4f& mat,
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
  // Multiply all rows including translation components.
  for (int j = 0; j < 4; ++j) {
    tmat.m[0][j] *= x;
    tmat.m[1][j] *= y;
    tmat.m[2][j] *= z;
  }
}

// Right multiply a scale matrix.
void ScaleMRight(gvr::Mat4f& tmat,
                 const gvr::Mat4f& mat,
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
  // Multiply columns, don't change translation components.
  for (int i = 0; i < 3; ++i) {
    tmat.m[i][0] *= x;
    tmat.m[i][1] *= y;
    tmat.m[i][2] *= z;
  }
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

gvr::Vec3f GetForwardVector(const gvr::Mat4f& matrix) {
  // Same as multiplying the inverse of the rotation component of the matrix by
  // (0, 0, -1, 0).
  return {-matrix.m[2][0], -matrix.m[2][1], -matrix.m[2][2]};
}

gvr::Vec3f GetTranslation(const gvr::Mat4f& matrix) {
  return {matrix.m[0][3], matrix.m[1][3], matrix.m[2][3]};
}

float VectorLength(const gvr::Vec3f& vec) {
  return sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
}

float NormalizeVector(gvr::Vec3f& vec) {
  float len = VectorLength(vec);
  vec.x /= len;
  vec.y /= len;
  vec.z /= len;
  return len;
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

gvr::Quatf QuatFromAxisAngle(const gvr::Vec3f& axis, float angle) {
  // Rotation angle is the product of |angle| and the magnitude of |axis|.
  gvr::Vec3f normal = axis;
  float length = NormalizeVector(normal);
  angle *= length;

  gvr::Quatf res;
  float s = sin(angle / 2);
  res.qx = normal.x * s;
  res.qy = normal.y * s;
  res.qz = normal.z * s;
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

  return {{{m11, m12, m13, 0.0f},
           {m21, m22, m23, 0.0f},
           {m31, m32, m33, 0.0f},
           {0.0f, 0.0f, 0.0f, 1.0f}}};
}

gvr::Vec3f GetRayPoint(const gvr::Vec3f& rayOrigin,
                       const gvr::Vec3f& rayVector,
                       float scale) {
  gvr::Vec3f v;
  v.x = rayOrigin.x + scale * rayVector.x;
  v.y = rayOrigin.y + scale * rayVector.y;
  v.z = rayOrigin.z + scale * rayVector.z;
  return v;
}

}  // namespace vr_shell
