// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/transformer_util.h"

#include <cmath>

#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/gfx/transform.h"

namespace ash {

// Round near zero value to zero.
void RoundNearZero(gfx::Transform* transform) {
  const float kEpsilon = 0.001f;
  SkMatrix44& matrix = transform->matrix();
  for (int x = 0; x < 4; ++x) {
    for (int y = 0; y < 4; ++y) {
      if (std::abs(SkMScalarToFloat(matrix.get(x, y))) < kEpsilon)
        matrix.set(x, y, SkFloatToMScalar(0.0f));
    }
  }
}

gfx::Transform CreateRotationTransform(display::Display::Rotation old_rotation,
                                       display::Display::Rotation new_rotation,
                                       const display::Display& display) {
  const int rotation_angle = 90 * (((new_rotation - old_rotation) + 4) % 4);
  gfx::Transform rotate;
  // The origin is (0, 0), so the translate width/height must be reduced by
  // 1 pixel.
  float one_pixel = 1.0f / display.device_scale_factor();
  switch (rotation_angle) {
    case 0:
      break;
    case 90:
      rotate.Translate(display.bounds().height() - one_pixel, 0);
      rotate.Rotate(90);
      break;
    case 180:
      rotate.Translate(display.bounds().width() - one_pixel,
                       display.bounds().height() - one_pixel);
      rotate.Rotate(180);
      break;
    case 270:
      rotate.Translate(0, display.bounds().width() - one_pixel);
      rotate.Rotate(270);
      break;
  }

  RoundNearZero(&rotate);
  return rotate;
}

}  // namespace ash
