// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/animation_player.h"

#include "cc/animation/animation_target.h"
#include "cc/test/geometry_test_utils.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/transition.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/test/gfx_util.h"

namespace vr {

class TestAnimationTarget : public cc::AnimationTarget {
 public:
  TestAnimationTarget() {
    operations_.AppendTranslate(0, 0, 0);
    operations_.AppendRotate(1, 0, 0, 0);
    operations_.AppendScale(1, 1, 1);
  }

  const gfx::SizeF& size() const { return size_; }
  const cc::TransformOperations& operations() const { return operations_; }
  float opacity() const { return opacity_; }

  void NotifyClientBoundsAnimated(const gfx::SizeF& size,
                                  cc::Animation* animation) override {
    size_ = size;
  }

  void NotifyClientTransformOperationsAnimated(
      const cc::TransformOperations& operations,
      cc::Animation* animation) override {
    operations_ = operations;
  }

  void NotifyClientOpacityAnimated(float opacity,
                                   cc::Animation* animation) override {
    opacity_ = opacity;
  }

 private:
  cc::TransformOperations operations_;
  gfx::SizeF size_ = {10.0f, 10.0f};
  float opacity_ = 1.0f;
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
  EXPECT_EQ(cc::Animation::RUNNING, player.animations()[0]->run_state());
  EXPECT_EQ(id1, player.animations()[0]->id());

  // Tick beyond all animations. There should be none remaining.
  player.Tick(start_time + UsToDelta(30000));
  EXPECT_TRUE(player.animations().empty());
}

TEST(AnimationPlayerTest, OpacityTransitions) {
  TestAnimationTarget target;
  AnimationPlayer player;
  player.set_target(&target);
  Transition transition;
  transition.target_properties[cc::TargetProperty::OPACITY] = true;
  transition.duration = UsToDelta(10000);
  player.set_transition(transition);

  base::TimeTicks start_time = UsToTicks(1000000);
  player.Tick(start_time);

  float from = 1.0f;
  float to = 0.5f;
  player.TransitionOpacityTo(start_time, from, to);

  EXPECT_EQ(from, target.opacity());
  player.Tick(start_time);

  player.Tick(start_time + UsToDelta(5000));
  EXPECT_GT(from, target.opacity());
  EXPECT_LT(to, target.opacity());

  player.Tick(start_time + UsToDelta(10000));
  EXPECT_EQ(to, target.opacity());
}

TEST(AnimationPlayerTest, ReversedOpacityTransitions) {
  TestAnimationTarget target;
  AnimationPlayer player;
  player.set_target(&target);
  Transition transition;
  transition.target_properties[cc::TargetProperty::OPACITY] = true;
  transition.duration = UsToDelta(10000);
  player.set_transition(transition);

  base::TimeTicks start_time = UsToTicks(1000000);
  player.Tick(start_time);

  float from = 1.0f;
  float to = 0.5f;
  player.TransitionOpacityTo(start_time, from, to);

  EXPECT_EQ(from, target.opacity());
  player.Tick(start_time);

  player.Tick(start_time + UsToDelta(1000));
  float value_before_reversing = target.opacity();
  EXPECT_GT(from, value_before_reversing);
  EXPECT_LT(to, value_before_reversing);

  player.TransitionOpacityTo(start_time + UsToDelta(1000), target.opacity(),
                             from);
  player.Tick(start_time + UsToDelta(1000));
  EXPECT_FLOAT_EQ(value_before_reversing, target.opacity());

  player.Tick(start_time + UsToDelta(2000));
  EXPECT_EQ(from, target.opacity());
}

TEST(AnimationPlayerTest, TransformTransitions) {
  TestAnimationTarget target;
  AnimationPlayer player;
  player.set_target(&target);
  Transition transition;
  transition.target_properties[cc::TargetProperty::TRANSFORM] = true;
  transition.duration = UsToDelta(10000);
  player.set_transition(transition);
  base::TimeTicks start_time = UsToTicks(1000000);
  player.Tick(start_time);

  cc::TransformOperations from = target.operations();

  cc::TransformOperations to;
  to.AppendTranslate(8, 0, 0);
  to.AppendRotate(1, 0, 0, 0);
  to.AppendScale(1, 1, 1);

  player.TransitionTransformOperationsTo(start_time, from, to);

  EXPECT_EQ(from, target.operations());
  player.Tick(start_time);

  player.Tick(start_time + UsToDelta(5000));
  EXPECT_LT(from.at(0).translate.x, target.operations().at(0).translate.x);
  EXPECT_GT(to.at(0).translate.x, target.operations().at(0).translate.x);

  player.Tick(start_time + UsToDelta(10000));
  EXPECT_EQ(to, target.operations());
}

TEST(AnimationPlayerTest, ReversedTransformTransitions) {
  TestAnimationTarget target;
  AnimationPlayer player;
  player.set_target(&target);
  Transition transition;
  transition.target_properties[cc::TargetProperty::TRANSFORM] = true;
  transition.duration = UsToDelta(10000);
  player.set_transition(transition);
  base::TimeTicks start_time = UsToTicks(1000000);
  player.Tick(start_time);

  cc::TransformOperations from = target.operations();

  cc::TransformOperations to;
  to.AppendTranslate(8, 0, 0);
  to.AppendRotate(1, 0, 0, 0);
  to.AppendScale(1, 1, 1);

  player.TransitionTransformOperationsTo(start_time, from, to);

  EXPECT_EQ(from, target.operations());
  player.Tick(start_time);

  player.Tick(start_time + UsToDelta(1000));
  cc::TransformOperations value_before_reversing = target.operations();
  EXPECT_LT(from.at(0).translate.x, target.operations().at(0).translate.x);
  EXPECT_GT(to.at(0).translate.x, target.operations().at(0).translate.x);

  player.TransitionTransformOperationsTo(start_time + UsToDelta(1000),
                                         target.operations(), from);
  player.Tick(start_time + UsToDelta(1000));
  EXPECT_EQ(value_before_reversing, target.operations());

  player.Tick(start_time + UsToDelta(2000));
  EXPECT_EQ(from, target.operations());
}

TEST(AnimationPlayerTest, BoundsTransitions) {
  TestAnimationTarget target;
  AnimationPlayer player;
  player.set_target(&target);
  Transition transition;
  transition.target_properties[cc::TargetProperty::BOUNDS] = true;
  transition.duration = UsToDelta(10000);
  player.set_transition(transition);
  base::TimeTicks start_time = UsToTicks(1000000);
  player.Tick(start_time);

  gfx::SizeF from = target.size();
  gfx::SizeF to(20.0f, 20.0f);

  player.TransitionBoundsTo(start_time, from, to);

  EXPECT_FLOAT_SIZE_EQ(from, target.size());
  player.Tick(start_time);

  player.Tick(start_time + UsToDelta(5000));
  EXPECT_LT(from.width(), target.size().width());
  EXPECT_GT(to.width(), target.size().width());
  EXPECT_LT(from.height(), target.size().height());
  EXPECT_GT(to.height(), target.size().height());

  player.Tick(start_time + UsToDelta(10000));
  EXPECT_FLOAT_SIZE_EQ(to, target.size());
}

TEST(AnimationPlayerTest, ReversedBoundsTransitions) {
  TestAnimationTarget target;
  AnimationPlayer player;
  player.set_target(&target);
  Transition transition;
  transition.target_properties[cc::TargetProperty::BOUNDS] = true;
  transition.duration = UsToDelta(10000);
  player.set_transition(transition);
  base::TimeTicks start_time = UsToTicks(1000000);
  player.Tick(start_time);

  gfx::SizeF from = target.size();
  gfx::SizeF to(20.0f, 20.0f);

  player.TransitionBoundsTo(start_time, from, to);

  EXPECT_FLOAT_SIZE_EQ(from, target.size());
  player.Tick(start_time);

  player.Tick(start_time + UsToDelta(1000));
  gfx::SizeF value_before_reversing = target.size();
  EXPECT_LT(from.width(), target.size().width());
  EXPECT_GT(to.width(), target.size().width());
  EXPECT_LT(from.height(), target.size().height());
  EXPECT_GT(to.height(), target.size().height());

  player.TransitionBoundsTo(start_time + UsToDelta(1000), target.size(), from);
  player.Tick(start_time + UsToDelta(1000));
  EXPECT_FLOAT_SIZE_EQ(value_before_reversing, target.size());

  player.Tick(start_time + UsToDelta(2000));
  EXPECT_FLOAT_SIZE_EQ(from, target.size());
}

}  // namespace vr
