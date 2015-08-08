// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/vr/vr_transform_util.h"

#include <math.h>

namespace content {

// Source:
// http://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/
VRVector4Ptr MatrixToOrientationQuaternion(
    const float m0[3], const float m1[3], const float m2[3]) {
  VRVector4Ptr out = VRVector4::New();
  float trace = m0[0] + m1[1] + m2[2];
  float root;
  if (trace > 0.0f) {
    root = sqrtf(1.0f + trace) * 2.0f;
    out->x = (m2[1] - m1[2]) / root;
    out->y = (m0[2] - m2[0]) / root;
    out->z = (m1[0] - m0[1]) / root;
    out->w = 0.25f * root;
  } else if ((m0[0] > m1[1]) && (m0[0] > m2[2])) {
    root = sqrtf(1.0f + m0[0] - m1[1] - m2[2]) * 2.0f;
    out->x = 0.25f * root;
    out->y = (m0[1] + m1[0]) / root;
    out->z = (m0[2] + m2[0]) / root;
    out->w = (m2[1] - m1[2]) / root;
  } else if (m1[1] > m2[2]) {
    root = sqrtf(1.0f + m1[1] - m0[0] - m2[2]) * 2.0f;
    out->x = (m0[1] + m1[0]) / root;
    out->y = 0.25f * root;
    out->z = (m1[2] + m2[1]) / root;
    out->w = (m0[2] - m2[0]) / root;
  } else {
    root = sqrtf(1.0f + m2[2] - m0[0] - m1[1]) * 2.0f;
    out->x = (m0[2] + m2[0]) / root;
    out->y = (m1[2] + m2[1]) / root;
    out->z = 0.25f * root;
    out->w = (m1[0] - m0[1]) / root;
  }
  return out.Pass();
}

}  // namespace content
