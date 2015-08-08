// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include "content/browser/vr/vr_transform_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

const float kPi = 3.14159265f;

void EXPECT_QUATERNION(
    float x, float y, float z, float w,
    const VRVector4Ptr& quat) {
  EXPECT_NEAR(x, quat->x, 0.0001f);
  EXPECT_NEAR(y, quat->y, 0.0001f);
  EXPECT_NEAR(z, quat->z, 0.0001f);
  EXPECT_NEAR(w, quat->w, 0.0001f);
}

TEST(VRTransformUtilTest, IdentityMatrixToQuaternion) {
  float matrix[9] = {
    1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f
  };

  VRVector4Ptr orientation = MatrixToOrientationQuaternion(
      &matrix[0], &matrix[3], &matrix[6]);

  EXPECT_QUATERNION(0.0f, 0.0f, 0.0f, 1.0f, orientation);
}

TEST(VRTransformUtilTest, 90DegXMatrixToQuaternion) {
  float rad = kPi * 0.5f;
  float matrix[9] = {
    1.0f, 0.0f, 0.0f,
    0.0f, cosf(rad), -sinf(rad),
    0.0f, sinf(rad), cosf(rad)
  };

  VRVector4Ptr orientation = MatrixToOrientationQuaternion(
      &matrix[0], &matrix[3], &matrix[6]);

  EXPECT_QUATERNION(0.7071f, 0.0f, 0.0f, 0.7071f, orientation);
}

TEST(VRTransformUtilTest, 180DegYMatrixToQuaternion) {
  float matrix[9] = {
    cosf(kPi), 0.0f, sinf(kPi),
    0.0f, 1.0f, 0.0f,
    -sinf(kPi), 0.0f, cosf(kPi)
  };

  VRVector4Ptr orientation = MatrixToOrientationQuaternion(
      &matrix[0], &matrix[3], &matrix[6]);

  EXPECT_QUATERNION(0.0f, 1.0f, 0.0f, 0.0f, orientation);
}

}  // namespace content
