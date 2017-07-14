// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/animation_player.h"

#include "cc/animation/animation_target.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/test/gfx_util.h"

namespace vr {

class TestAnimationTarget : public cc::AnimationTarget {
 public:
  TestAnimationTarget() {}

  const gfx::SizeF& size() const { return size_; }
  const cc::TransformOperations& operations() const { return operations_; }

 private:
  void NotifyClientBoundsAnimated(const gfx::SizeF& size,
                                  cc::Animation* animation) override {
    size_ = size;
  }

  void NotifyClientTransformOperationsAnimated(
      const cc::TransformOperations& operations,
      cc::Animation* animation) override {
    operations_ = operations;
  }

  cc::TransformOperations operations_;
  gfx::SizeF size_;
};

TEST(AnimationPlayerTest, AddRemoveAnimations) {
  AnimationPlayer player;
  EXPECT_TRUE(player.animations().empty());

  player.AddAnimation(CreateBoundsAnimation(
      1, 1, gfx::SizeF(10, 100), gfx::SizeF(20, 200), UsToDelta(10000)));
  EXPECT_EQ(1ul, player.animations().size());
  EXPECT_EQ(cc::TargetProperty::BOUNDS,
            player.animations()[0]->target_property());

  cc::TransformOperations from_operations;
  from_operations.AppendTranslate(10, 100, 1000);
  cc::TransformOperations to_operations;
  to_operations.AppendTranslate(20, 200, 2000);
  player.AddAnimation(CreateTransformAnimation(
      2, 2, from_operations, to_operations, UsToDelta(10000)));

  EXPECT_EQ(2ul, player.animations().size());
  EXPECT_EQ(cc::TargetProperty::TRANSFORM,
            player.animations()[1]->target_property());

  player.AddAnimation(CreateTransformAnimation(
      3, 3, from_operations, to_operations, UsToDelta(10000)));
  EXPECT_EQ(3ul, player.animations().size());
  EXPECT_EQ(cc::TargetProperty::TRANSFORM,
            player.animations()[2]->target_property());

  player.RemoveAnimations(cc::TargetProperty::TRANSFORM);
  EXPECT_EQ(1ul, player.animations().size());
  EXPECT_EQ(cc::TargetProperty::BOUNDS,
            player.animations()[0]->target_property());

  player.RemoveAnimation(player.animations()[0]->id());
  EXPECT_TRUE(player.animations().empty());
}

TEST(AnimationPlayerTest, AnimationLifecycle) {
  TestAnimationTarget target;
  AnimationPlayer player;
  player.set_target(&target);

  player.AddAnimation(CreateBoundsAnimation(
      1, 1, gfx::SizeF(10, 100), gfx::SizeF(20, 200), UsToDelta(10000)));
  EXPECT_EQ(1ul, player.animations().size());
  EXPECT_EQ(cc::TargetProperty::BOUNDS,
            player.animations()[0]->target_property());
  EXPECT_EQ(cc::Animation::WAITING_FOR_TARGET_AVAILABILITY,
            player.animations()[0]->run_state());

  base::TimeTicks start_time = UsToTicks(1);
  player.Tick(start_time);
  EXPECT_EQ(cc::Animation::RUNNING, player.animations()[0]->run_state());

  EXPECT_SIZEF_EQ(gfx::SizeF(10, 100), target.size());

  // Tick beyond the animation
  player.Tick(start_time + UsToDelta(20000));

  EXPECT_TRUE(player.animations().empty());

  // Should have assumed the final value.
  EXPECT_SIZEF_EQ(gfx::SizeF(20, 200), target.size());
}

TEST(AnimationPlayerTest, AnimationQueue) {
  TestAnimationTarget target;
  AnimationPlayer player;
  player.set_target(&target);

  player.AddAnimation(CreateBoundsAnimation(
      1, 1, gfx::SizeF(10, 100), gfx::SizeF(20, 200), UsToDelta(10000)));
  EXPECT_EQ(1ul, player.animations().size());
  EXPECT_EQ(cc::TargetProperty::BOUNDS,
            player.animations()[0]->target_property());
  EXPECT_EQ(cc::Animation::WAITING_FOR_TARGET_AVAILABILITY,
            player.animations()[0]->run_state());

  base::TimeTicks start_time = UsToTicks(1);
  player.Tick(start_time);
  EXPECT_EQ(cc::Animation::RUNNING, player.animations()[0]->run_state());
  EXPECT_SIZEF_EQ(gfx::SizeF(10, 100), target.size());

  player.AddAnimation(CreateBoundsAnimation(
      2, 2, gfx::SizeF(10, 100), gfx::SizeF(20, 200), UsToDelta(10000)));

  cc::TransformOperations from_operations;
  from_operations.AppendTranslate(10, 100, 1000);
  cc::TransformOperations to_operations;
  to_operations.AppendTranslate(20, 200, 2000);
  player.AddAnimation(CreateTransformAnimation(
      3, 2, from_operations, to_operations, UsToDelta(10000)));

  EXPECT_EQ(3ul, player.animations().size());
  EXPECT_EQ(cc::TargetProperty::BOUNDS,
            player.animations()[1]->target_property());
  EXPECT_EQ(cc::TargetProperty::TRANSFORM,
            player.animations()[2]->target_property());
  int id1 = player.animations()[1]->id();

  player.Tick(start_time + UsToDelta(1));

  // Only the transform animation should have started (since there's no
  // conflicting animation).
  // TODO(vollick): Once we were to support groups (crbug.com/742358)
  // these two animations should start together so neither queued animation
  // should start since there's a running animation blocking one of them.
  EXPECT_EQ(cc::Animation::WAITING_FOR_TARGET_AVAILABILITY,
            player.animations()[1]->run_state());
  EXPECT_EQ(cc::Animation::RUNNING, player.animations()[2]->run_state());

  // Tick beyond the first animation. This should cause it (and the transform
  // animation) to get removed and for the second bounds animation to start.
  // TODO(vollick): this will also change when groups are supported
  // (crbug.com/742358).
  player.Tick(start_time + UsToDelta(15000));

  EXPECT_EQ(1ul, player.animations().size());
  EXPECT_EQ(cc::Animation::WAITING_FOR_TARGET_AVAILABILITY,
            player.animations()[0]->run_state());
  EXPECT_EQ(id1, player.animations()[0]->id());

  // Tick beyond all animations. There should be none remaining.
  player.Tick(start_time + UsToDelta(30000));
  EXPECT_TRUE(player.animations().empty());
}

}  // namespace vr
