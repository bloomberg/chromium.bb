// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SCALE_UTILITY_H_
#define ASH_PUBLIC_CPP_SCALE_UTILITY_H_

namespace gfx {
class Transform;
}  // namespace gfx

namespace ash {

// TODO(crbug.com/756161): This method will need to be moved to
// gfx/geometry/dip_utils.h for M62 and later. This method computes the scale
// required to convert DIP coordinates to the coordinate space of the
// |transform|. It deduces the scale from the transform by applying it to a pair
// of points separated by the distance of 1, and measuring the distance between
// the transformed points.
float GetScaleFactorForTransform(const gfx::Transform& transform);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SCALE_UTILITY_H_