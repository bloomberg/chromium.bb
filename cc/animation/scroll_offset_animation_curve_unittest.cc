// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scroll_offset_animation_curve.h"

#include "cc/animation/timing_function.h"
#include "cc/test/geometry_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(ScrollOffsetAnimationCurveTest, GetValue) {
  gfx::Vector2dF initial_value(2.f, 40.f);
  gfx::Vector2dF target_value(10.f, 20.f);
  double duration = 10.0;
  scoped_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          target_value,
          EaseInOutTimingFunction::Create().Pass()));
  curve->set_duration(duration);
  curve->set_initial_value(initial_value);

  EXPECT_EQ(AnimationCurve::ScrollOffset, curve->Type());
  EXPECT_EQ(duration, curve->Duration());

  EXPECT_VECTOR2DF_EQ(initial_value, curve->GetValue(-1.0));
  EXPECT_VECTOR2DF_EQ(initial_value, curve->GetValue(0.0));
  EXPECT_VECTOR2DF_EQ(gfx::Vector2dF(6.f, 30.f), curve->GetValue(duration/2.0));
  EXPECT_VECTOR2DF_EQ(target_value, curve->GetValue(duration));
  EXPECT_VECTOR2DF_EQ(target_value, curve->GetValue(duration+1.0));

  // Verify that GetValue takes the timing function into account.
  gfx::Vector2dF value = curve->GetValue(duration/4.0);
  EXPECT_NEAR(3.0333f, value.x(), 0.00015f);
  EXPECT_NEAR(37.4168f, value.y(), 0.00015f);
}

// Verify that a clone behaves exactly like the original.
TEST(ScrollOffsetAnimationCurveTest, Clone) {
  gfx::Vector2dF initial_value(2.f, 40.f);
  gfx::Vector2dF target_value(10.f, 20.f);
  double duration = 10.0;
  scoped_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          target_value,
          EaseInOutTimingFunction::Create().Pass()));
  curve->set_duration(duration);
  curve->set_initial_value(initial_value);

  scoped_ptr<AnimationCurve> clone(curve->Clone().Pass());

  EXPECT_EQ(AnimationCurve::ScrollOffset, clone->Type());
  EXPECT_EQ(duration, clone->Duration());

  EXPECT_VECTOR2DF_EQ(initial_value,
                      clone->ToScrollOffsetAnimationCurve()->GetValue(-1.0));
  EXPECT_VECTOR2DF_EQ(initial_value,
                      clone->ToScrollOffsetAnimationCurve()->GetValue(0.0));
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(6.f, 30.f),
      clone->ToScrollOffsetAnimationCurve()->GetValue(duration / 2.0));
  EXPECT_VECTOR2DF_EQ(
      target_value,
      clone->ToScrollOffsetAnimationCurve()->GetValue(duration));
  EXPECT_VECTOR2DF_EQ(
      target_value,
      clone->ToScrollOffsetAnimationCurve()->GetValue(duration + 1.0));

  // Verify that the timing function was cloned correctly.
  gfx::Vector2dF value =
      clone->ToScrollOffsetAnimationCurve()->GetValue(duration / 4.0);
  EXPECT_NEAR(3.0333f, value.x(), 0.00015f);
  EXPECT_NEAR(37.4168f, value.y(), 0.00015f);
}


}  // namespace
}  // namespace cc
