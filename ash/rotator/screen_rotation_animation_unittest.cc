// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rotator/screen_rotation_animation.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/transform.h"

namespace ash {
namespace test {

class ScreenRotationAnimationTest : public AshTestBase {
 public:
  ScreenRotationAnimationTest() {}
  ~ScreenRotationAnimationTest() override {}

  // AshTestBase:
  void SetUp() override;

 private:
  scoped_ptr<ui::ScopedAnimationDurationScaleMode> non_zero_duration_mode_;

  DISALLOW_COPY_AND_ASSIGN(ScreenRotationAnimationTest);
};

void ScreenRotationAnimationTest::SetUp() {
  AshTestBase::SetUp();
  non_zero_duration_mode_.reset(new ui::ScopedAnimationDurationScaleMode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION));
}

TEST_F(ScreenRotationAnimationTest, LayerTransformGetsSetToTargetWhenAborted) {
  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithId(9));
  ui::Layer* layer = window->layer();

  scoped_ptr<ScreenRotationAnimation> screen_rotation(
      new ScreenRotationAnimation(
          layer, 45 /* start_degrees */, 0 /* end_degrees */,
          0.5f /* initial_opacity */, 1.0f /* target_opacity */,
          gfx::Point(10, 10) /* pivot */,
          base::TimeDelta::FromSeconds(10) /* duration */, gfx::Tween::LINEAR));

  ui::LayerAnimator* animator = layer->GetAnimator();
  animator->set_preemption_strategy(
      ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
  scoped_ptr<ui::LayerAnimationSequence> animation_sequence(
      new ui::LayerAnimationSequence(screen_rotation.release()));
  animator->StartAnimation(animation_sequence.release());

  const gfx::Transform identity_transform;

  ASSERT_EQ(identity_transform, layer->GetTargetTransform());
  ASSERT_NE(identity_transform, layer->transform());

  layer->GetAnimator()->AbortAllAnimations();

  EXPECT_EQ(identity_transform, layer->transform());
}

}  // namespace test
}  // namespace ash
