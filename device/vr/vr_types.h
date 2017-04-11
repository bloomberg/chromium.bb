// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_VR_TYPES_H_
#define DEVICE_VR_VR_TYPES_H_

#include <array>

#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace vr {

using Mat4f = std::array<std::array<float, 4>, 4>;

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

// A floating point quaternion, in JPL format.
typedef struct Quatf {
  /// qx, qy, qz are the vector component.
  float qx;
  float qy;
  float qz;
  /// qw is the linear component.
  float qw;
} Quatf;

}  // namespace vr

#endif  // DEVICE_VR_VR_TYPES_H_
