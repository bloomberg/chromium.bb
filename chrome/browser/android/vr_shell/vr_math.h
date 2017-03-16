// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_MATH_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_MATH_H_

#include <array>

#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

namespace vr_shell {

// 2D rectangles. Unlike gvr::Rectf and gvr::Recti, these have width and height
// rather than right and top.
typedef struct Recti {
  int x;
  int y;
  int width;
  int height;
} Recti;

typedef struct Rectf {
  float x;
  float y;
  float width;
  float height;
} Rectf;

typedef struct RotationAxisAngle {
  float x;
  float y;
  float z;
  float angle;
} RotationAxisAngle;

typedef struct Colorf {
  float r;
  float g;
  float b;
  float a;
} Colorf;

void SetIdentityM(gvr::Mat4f& mat);
void TranslateM(gvr::Mat4f& tmat, gvr::Mat4f& mat, float x, float y, float z);
void ScaleM(gvr::Mat4f& tmat, const gvr::Mat4f& mat, float x, float y, float z);

// Util functions that are copied from the treasure_hunt NDK demo in
// third_party/gvr-andoir-sdk/ folder.
gvr::Vec3f MatrixVectorMul(const gvr::Mat4f& m, const gvr::Vec3f& v);
gvr::Vec3f MatrixVectorRotate(const gvr::Mat4f& m, const gvr::Vec3f& v);
gvr::Mat4f MatrixMul(const gvr::Mat4f& matrix1, const gvr::Mat4f& matrix2);
gvr::Mat4f PerspectiveMatrixFromView(const gvr::Rectf& fov,
                                     float z_near,
                                     float z_far);

// Provides the direction the head is looking towards as a 3x1 unit vector.
gvr::Vec3f GetForwardVector(const gvr::Mat4f& matrix);

gvr::Vec3f GetTranslation(const gvr::Mat4f& matrix);

gvr::Mat4f QuatToMatrix(const gvr::Quatf& quat);

float VectorLength(const gvr::Vec3f& vec);
gvr::Vec3f VectorSubtract(const gvr::Vec3f& a, const gvr::Vec3f& b);
float VectorDot(const gvr::Vec3f& a, const gvr::Vec3f& b);

// Normalize a vector, and return its original length.
float NormalizeVector(gvr::Vec3f& vec);

void NormalizeQuat(gvr::Quatf& quat);

gvr::Quatf QuatFromAxisAngle(const gvr::Vec3f& axis, float angle);

gvr::Vec3f GetRayPoint(const gvr::Vec3f& rayOrigin,
                       const gvr::Vec3f& rayVector,
                       float scale);

float Distance(const gvr::Vec3f& vec1, const gvr::Vec3f& vec2);

// Angle between the vectors' projections to the XZ plane.
bool XZAngle(const gvr::Vec3f& vec1, const gvr::Vec3f& vec2, float* angle);

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_MATH_H_
