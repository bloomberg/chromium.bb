// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/ui_element.h"

#include <utility>

#include "base/macros.h"
#include "cc/animation/animation.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/test/geometry_test_utils.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

TEST(UiElements, AnimateSize) {
  UiElement rect;
  rect.SetSize(10, 100);
  rect.animation_player().AddAnimation(
      CreateBoundsAnimation(1, 1, gfx::SizeF(10, 100), gfx::SizeF(20, 200),
                            MicrosecondsToDelta(10000)));
  base::TimeTicks start_time = MicrosecondsToTicks(1);
  rect.Animate(start_time);
  EXPECT_FLOAT_SIZE_EQ(gfx::SizeF(10, 100), rect.size());
  rect.Animate(start_time + MicrosecondsToDelta(10000));
  EXPECT_FLOAT_SIZE_EQ(gfx::SizeF(20, 200), rect.size());
}

TEST(UiElements, AnimationAffectsInheritableTransform) {
  UiElement rect;

  cc::TransformOperations from_operations;
  from_operations.AppendTranslate(10, 100, 1000);
  cc::TransformOperations to_operations;
  to_operations.AppendTranslate(20, 200, 2000);
  rect.animation_player().AddAnimation(CreateTransformAnimation(
      2, 2, from_operations, to_operations, MicrosecondsToDelta(10000)));

  base::TimeTicks start_time = MicrosecondsToTicks(1);
  rect.Animate(start_time);
  gfx::Point3F p;
  rect.LocalTransform().TransformPoint(&p);
  EXPECT_VECTOR3DF_EQ(gfx::Vector3dF(10, 100, 1000), p);
  p = gfx::Point3F();
  rect.Animate(start_time + MicrosecondsToDelta(10000));
  rect.LocalTransform().TransformPoint(&p);
  EXPECT_VECTOR3DF_EQ(gfx::Vector3dF(20, 200, 2000), p);
}

}  // namespace vr
