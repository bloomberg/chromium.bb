// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_math.h"

#include <cmath>

#include "base/logging.h"

namespace vr {

namespace {
Mat4f CopyMat(const Mat4f& mat) {
  Mat4f ret = mat;
  return ret;
}
}

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

void SetIdentityM(Mat4f* mat) {
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      (*mat)[i][j] = i == j ? 1 : 0;
    }
  }
}

// Left multiply a translation matrix.
void TranslateM(const Mat4f& mat,
                const gfx::Vector3dF& translation,
                Mat4f* out) {
  if (out != &mat) {
    for (int i = 0; i < 4; ++i) {
      for (int j = 0; j < 4; ++j) {
        (*out)[i][j] = mat[i][j];
      }
    }
  }
  (*out)[0][3] += translation.x();
  (*out)[1][3] += translation.y();
  (*out)[2][3] += translation.z();
}

// Left multiply a scale matrix.
void ScaleM(const Mat4f& mat, const gfx::Vector3dF& scale, Mat4f* out) {
  if (out != &mat) {
    for (int i = 0; i < 4; ++i) {
      for (int j = 0; j < 3; ++j) {
        (*out)[i][j] = mat[i][j];
      }
    }
  }
  // Multiply all rows including translation components.
  for (int j = 0; j < 4; ++j) {
    (*out)[0][j] *= scale.x();
    (*out)[1][j] *= scale.y();
    (*out)[2][j] *= scale.z();
  }
}

gfx::Vector3dF MatrixVectorMul(const Mat4f& m, const gfx::Vector3dF& v) {
  return gfx::Vector3dF(
      m[0][0] * v.x() + m[0][1] * v.y() + m[0][2] * v.z() + m[0][3],
      m[1][0] * v.x() + m[1][1] * v.y() + m[1][2] * v.z() + m[1][3],
      m[2][0] * v.x() + m[2][1] * v.y() + m[2][2] * v.z() + m[2][3]);
}

// Rotation only, ignore translation components.
gfx::Vector3dF MatrixVectorRotate(const Mat4f& m, const gfx::Vector3dF& v) {
  return gfx::Vector3dF(m[0][0] * v.x() + m[0][1] * v.y() + m[0][2] * v.z(),
                        m[1][0] * v.x() + m[1][1] * v.y() + m[1][2] * v.z(),
                        m[2][0] * v.x() + m[2][1] * v.y() + m[2][2] * v.z());
}

void MatrixMul(const Mat4f& matrix1, const Mat4f& matrix2, Mat4f* out) {
  const Mat4f& mat1 = (out == &matrix1) ? CopyMat(matrix1) : matrix1;
  const Mat4f& mat2 = (out == &matrix2) ? CopyMat(matrix2) : matrix2;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      (*out)[i][j] = 0.0f;
      for (int k = 0; k < 4; ++k) {
        (*out)[i][j] += mat1[i][k] * mat2[k][j];
      }
    }
  }
}

void PerspectiveMatrixFromView(const gfx::RectF& fov,
                               float z_near,
                               float z_far,
                               Mat4f* out) {
  const float x_left = -std::tan(fov.x() * M_PI / 180.0f) * z_near;
  const float x_right = std::tan(fov.right() * M_PI / 180.0f) * z_near;
  const float y_bottom = -std::tan(fov.bottom() * M_PI / 180.0f) * z_near;
  const float y_top = std::tan(fov.y() * M_PI / 180.0f) * z_near;

  DCHECK(x_left < x_right && y_bottom < y_top && z_near < z_far &&
         z_near > 0.0f && z_far > 0.0f);
  const float X = (2 * z_near) / (x_right - x_left);
  const float Y = (2 * z_near) / (y_top - y_bottom);
  const float A = (x_right + x_left) / (x_right - x_left);
  const float B = (y_top + y_bottom) / (y_top - y_bottom);
  const float C = (z_near + z_far) / (z_near - z_far);
  const float D = (2 * z_near * z_far) / (z_near - z_far);

  for (int i = 0; i < 4; ++i) {
    (*out)[i].fill(0.0f);
  }
  (*out)[0][0] = X;
  (*out)[0][2] = A;
  (*out)[1][1] = Y;
  (*out)[1][2] = B;
  (*out)[2][2] = C;
  (*out)[2][3] = D;
  (*out)[3][2] = -1;
}

gfx::Vector3dF GetForwardVector(const Mat4f& matrix) {
  // Same as multiplying the inverse of the rotation component of the matrix by
  // (0, 0, -1, 0).
  return gfx::Vector3dF(-matrix[2][0], -matrix[2][1], -matrix[2][2]);
}

gfx::Vector3dF GetTranslation(const Mat4f& matrix) {
  return gfx::Vector3dF(matrix[0][3], matrix[1][3], matrix[2][3]);
}

float NormalizeVector(gfx::Vector3dF* vec) {
  float len = vec->Length();
  if (len == 0)
    return 0;
  vec->Scale(1.0f / len);
  return len;
}

void NormalizeQuat(Quatf* quat) {
  float len = sqrt(quat->qx * quat->qx + quat->qy * quat->qy +
                   quat->qz * quat->qz + quat->qw * quat->qw);
  quat->qx /= len;
  quat->qy /= len;
  quat->qz /= len;
  quat->qw /= len;
}

Quatf QuatFromAxisAngle(const RotationAxisAngle& axis_angle) {
  // Rotation angle is the product of |angle| and the magnitude of |axis|.
  gfx::Vector3dF normal(axis_angle.x, axis_angle.y, axis_angle.z);
  float length = NormalizeVector(&normal);
  float angle = axis_angle.angle * length;

  Quatf res;
  float s = sin(angle / 2);
  res.qx = normal.x() * s;
  res.qy = normal.y() * s;
  res.qz = normal.z() * s;
  res.qw = cos(angle / 2);
  return res;
}

Quatf QuatMultiply(const Quatf& a, const Quatf& b) {
  Quatf res;
  res.qw = a.qw * b.qw - a.qx * b.qx - a.qy * b.qy - a.qz * b.qz;
  res.qx = a.qw * b.qx + a.qx * b.qw + a.qy * b.qz - a.qz * b.qy;
  res.qy = a.qw * b.qy - a.qx * b.qz + a.qy * b.qw + a.qz * b.qx;
  res.qz = a.qw * b.qz + a.qx * b.qy - a.qy * b.qx + a.qz * b.qw;
  return res;
}

void QuatToMatrix(const Quatf& quat, Mat4f* out) {
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

  *out = {{{{m11, m12, m13, 0.0f}},
           {{m21, m22, m23, 0.0f}},
           {{m31, m32, m33, 0.0f}},
           {{0.0f, 0.0f, 0.0f, 1.0f}}}};
}

gfx::Point3F GetRayPoint(const gfx::Point3F& rayOrigin,
                         const gfx::Vector3dF& rayVector,
                         float scale) {
  return rayOrigin + gfx::ScaleVector3d(rayVector, scale);
}

float Distance(const gfx::Point3F& p1, const gfx::Point3F& p2) {
  return std::sqrt(p1.SquaredDistanceTo(p2));
}

bool XZAngle(const gfx::Vector3dF& vec1,
             const gfx::Vector3dF& vec2,
             float* angle) {
  float len1 = vec1.Length();
  float len2 = vec2.Length();
  if (len1 == 0 || len2 == 0)
    return false;
  float cross_p = vec1.x() * vec2.z() - vec1.z() * vec2.x();
  *angle = asin(cross_p / (len1 * len2));
  return true;
}

}  // namespace vr
