// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/animation_player.h"

#include "cc/animation/animation_target.h"
#include "cc/test/geometry_test_utils.h"
#include "chrome/browser/vr/target_property.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/transition.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/test/gfx_util.h"

namespace vr {

static constexpr float kNoise = 1e-6f;

class TestAnimationTarget : public cc::AnimationTarget {
 public:
  TestAnimationTarget() {
    layout_offset_.AppendTranslate(0, 0, 0);
    operations_.AppendTranslate(0, 0, 0);
    operations_.AppendRotate(1, 0, 0, 0);
    operations_.AppendScale(1, 1, 1);
  }

  const gfx::SizeF& size() const { return size_; }
  const cc::TransformOperations& operations() const { return operations_; }
  const cc::TransformOperations& layout_offset() const {
    return layout_offset_;
  }
  float opacity() const { return opacity_; }
  SkColor background_color() const { return background_color_; }

  void NotifyClientSizeAnimated(const gfx::SizeF& size,
                                int target_property_id,
                                cc::Animation* animation) override {
    size_ = size;
  }

  void NotifyClientTransformOperationsAnimated(
      const cc::TransformOperations& operations,
      int target_property_id,
      cc::Animation* animation) override {
    if (target_property_id == LAYOUT_OFFSET) {
      layout_offset_ = operations;
    } else {
      operations_ = operations;
    }
  }

  void NotifyClientFloatAnimated(float opacity,
                                 int target_property_id,
                                 cc::Animation* animation) override {
    opacity_ = opacity;
  }

  void NotifyClientColorAnimated(SkColor color,
                                 int target_property_id,
                                 cc::Animation* animation) override {
    background_color_ = color;
  }

 private:
  cc::TransformOperations layout_offset_;
  cc::TransformOperations operations_;
  gfx::SizeF size_ = {10.0f, 10.0f};
  float opacity_ = 1.0f;
  SkColor background_color_ = SK_ColorRED;
};

TEST(AnimationPlayerTest, AddRemoveAnimations) {
  AnimationPlayer player;
  EXPECT_TRUE(player.animations().empty());

  player.AddAnimation(CreateBoundsAnimation(1, 1, gfx::SizeF(10, 100),
                                            gfx::SizeF(20, 200),
                                            MicrosecondsToDelta(10000)));
  EXPECT_EQ(1ul, player.animations().size());
  EXPECT_EQ(BOUNDS, player.animations()[0]->target_property_id());

  cc::TransformOperations from_operations;
  from_operations.AppendTranslate(10, 100, 1000);
  cc::TransformOperations to_operations;
  to_operations.AppendTranslate(20, 200, 2000);
  player.AddAnimation(CreateTransformAnimation(
      2, 2, from_operations, to_operations, MicrosecondsToDelta(10000)));

  EXPECT_EQ(2ul, player.animations().size());
  EXPECT_EQ(TRANSFORM, player.animations()[1]->target_property_id());

  player.AddAnimation(CreateTransformAnimation(
      3, 3, from_operations, to_operations, MicrosecondsToDelta(10000)));
  EXPECT_EQ(3ul, player.animations().size());
  EXPECT_EQ(TRANSFORM, player.animations()[2]->target_property_id());

  player.RemoveAnimations(TRANSFORM);
  EXPECT_EQ(1ul, player.animations().size());
  EXPECT_EQ(BOUNDS, player.animations()[0]->target_property_id());

  player.RemoveAnimation(player.animations()[0]->id());
  EXPECT_TRUE(player.animations().empty());
}

TEST(AnimationPlayerTest, AnimationLifecycle) {
  TestAnimationTarget target;
  AnimationPlayer player;
  player.set_target(&target);

  player.AddAnimation(CreateBoundsAnimation(1, 1, gfx::SizeF(10, 100),
                                            gfx::SizeF(20, 200),
                                            MicrosecondsToDelta(10000)));
  EXPECT_EQ(1ul, player.animations().size());
  EXPECT_EQ(BOUNDS, player.animations()[0]->target_property_id());
  EXPECT_EQ(cc::Animation::WAITING_FOR_TARGET_AVAILABILITY,
            player.animations()[0]->run_state());

  base::TimeTicks start_time = MicrosecondsToTicks(1);
  player.Tick(start_time);
  EXPECT_EQ(cc::Animation::RUNNING, player.animations()[0]->run_state());

  EXPECT_SIZEF_EQ(gfx::SizeF(10, 100), target.size());

  // Tick beyond the animation
  player.Tick(start_time + MicrosecondsToDelta(20000));

  EXPECT_TRUE(player.animations().empty());

  // Should have assumed the final value.
  EXPECT_SIZEF_EQ(gfx::SizeF(20, 200), target.size());
}

TEST(AnimationPlayerTest, AnimationQueue) {
  TestAnimationTarget target;
  AnimationPlayer player;
  player.set_target(&target);

  player.AddAnimation(CreateBoundsAnimation(1, 1, gfx::SizeF(10, 100),
                                            gfx::SizeF(20, 200),
                                            MicrosecondsToDelta(10000)));
  EXPECT_EQ(1ul, player.animations().size());
  EXPECT_EQ(BOUNDS, player.animations()[0]->target_property_id());
  EXPECT_EQ(cc::Animation::WAITING_FOR_TARGET_AVAILABILITY,
            player.animations()[0]->run_state());

  base::TimeTicks start_time = MicrosecondsToTicks(1);
  player.Tick(start_time);
  EXPECT_EQ(cc::Animation::RUNNING, player.animations()[0]->run_state());
  EXPECT_SIZEF_EQ(gfx::SizeF(10, 100), target.size());

  player.AddAnimation(CreateBoundsAnimation(2, 2, gfx::SizeF(10, 100),
                                            gfx::SizeF(20, 200),
                                            MicrosecondsToDelta(10000)));

  cc::TransformOperations from_operations;
  from_operations.AppendTranslate(10, 100, 1000);
  cc::TransformOperations to_operations;
  to_operations.AppendTranslate(20, 200, 2000);
  player.AddAnimation(CreateTransformAnimation(
      3, 2, from_operations, to_operations, MicrosecondsToDelta(10000)));

  EXPECT_EQ(3ul, player.animations().size());
  EXPECT_EQ(BOUNDS, player.animations()[1]->target_property_id());
  EXPECT_EQ(TRANSFORM, player.animations()[2]->target_property_id());
  int id1 = player.animations()[1]->id();

  player.Tick(start_time + MicrosecondsToDelta(1));

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
  player.Tick(start_time + MicrosecondsToDelta(15000));

  EXPECT_EQ(1ul, player.animations().size());
  EXPECT_EQ(cc::Animation::RUNNING, player.animations()[0]->run_state());
  EXPECT_EQ(id1, player.animations()[0]->id());

  // Tick beyond all animations. There should be none remaining.
  player.Tick(start_time + MicrosecondsToDelta(30000));
  EXPECT_TRUE(player.animations().empty());
}

TEST(AnimationPlayerTest, OpacityTransitions) {
  TestAnimationTarget target;
  AnimationPlayer player;
  player.set_target(&target);
  Transition transition;
  transition.target_properties = {OPACITY};
  transition.duration = MicrosecondsToDelta(10000);
  player.set_transition(transition);

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  player.Tick(start_time);

  float from = 1.0f;
  float to = 0.5f;
  player.TransitionFloatTo(start_time, OPACITY, from, to);

  EXPECT_EQ(from, target.opacity());
  player.Tick(start_time);

  // Scheduling a redundant, approximately equal transition should be ignored.
  int animation_id = player.animations().front()->id();
  float nearby = to + kNoise;
  player.TransitionFloatTo(start_time, OPACITY, from, nearby);
  EXPECT_EQ(animation_id, player.animations().front()->id());

  player.Tick(start_time + MicrosecondsToDelta(5000));
  EXPECT_GT(from, target.opacity());
  EXPECT_LT(to, target.opacity());

  player.Tick(start_time + MicrosecondsToDelta(10000));
  EXPECT_EQ(to, target.opacity());
}

TEST(AnimationPlayerTest, ReversedOpacityTransitions) {
  TestAnimationTarget target;
  AnimationPlayer player;
  player.set_target(&target);
  Transition transition;
  transition.target_properties = {OPACITY};
  transition.duration = MicrosecondsToDelta(10000);
  player.set_transition(transition);

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  player.Tick(start_time);

  float from = 1.0f;
  float to = 0.5f;
  player.TransitionFloatTo(start_time, OPACITY, from, to);

  EXPECT_EQ(from, target.opacity());
  player.Tick(start_time);

  player.Tick(start_time + MicrosecondsToDelta(1000));
  float value_before_reversing = target.opacity();
  EXPECT_GT(from, value_before_reversing);
  EXPECT_LT(to, value_before_reversing);

  player.TransitionFloatTo(start_time + MicrosecondsToDelta(1000), OPACITY,
                           target.opacity(), from);
  player.Tick(start_time + MicrosecondsToDelta(1000));
  EXPECT_FLOAT_EQ(value_before_reversing, target.opacity());

  player.Tick(start_time + MicrosecondsToDelta(2000));
  EXPECT_EQ(from, target.opacity());
}

TEST(AnimationPlayerTest, LayoutOffsetTransitions) {
  // In this test, we do expect exact equality.
  float tolerance = 0.0f;
  TestAnimationTarget target;
  AnimationPlayer player;
  player.set_target(&target);
  Transition transition;
  transition.target_properties = {LAYOUT_OFFSET};
  transition.duration = MicrosecondsToDelta(10000);
  player.set_transition(transition);
  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  player.Tick(start_time);

  cc::TransformOperations from = target.layout_offset();

  cc::TransformOperations to;
  to.AppendTranslate(8, 0, 0);

  player.TransitionTransformOperationsTo(start_time, LAYOUT_OFFSET, from, to);

  EXPECT_TRUE(from.ApproximatelyEqual(target.layout_offset(), tolerance));
  player.Tick(start_time);

  // Scheduling a redundant, approximately equal transition should be ignored.
  int animation_id = player.animations().front()->id();
  cc::TransformOperations nearby = to;
  nearby.at(0).translate.x += kNoise;
  player.TransitionTransformOperationsTo(start_time, LAYOUT_OFFSET, from,
                                         nearby);
  EXPECT_EQ(animation_id, player.animations().front()->id());

  player.Tick(start_time + MicrosecondsToDelta(5000));
  EXPECT_LT(from.at(0).translate.x, target.layout_offset().at(0).translate.x);
  EXPECT_GT(to.at(0).translate.x, target.layout_offset().at(0).translate.x);

  player.Tick(start_time + MicrosecondsToDelta(10000));
  EXPECT_TRUE(to.ApproximatelyEqual(target.layout_offset(), tolerance));
}

TEST(AnimationPlayerTest, TransformTransitions) {
  // In this test, we do expect exact equality.
  float tolerance = 0.0f;
  TestAnimationTarget target;
  AnimationPlayer player;
  player.set_target(&target);
  Transition transition;
  transition.target_properties = {TRANSFORM};
  transition.duration = MicrosecondsToDelta(10000);
  player.set_transition(transition);
  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  player.Tick(start_time);

  cc::TransformOperations from = target.operations();

  cc::TransformOperations to;
  to.AppendTranslate(8, 0, 0);
  to.AppendRotate(1, 0, 0, 0);
  to.AppendScale(1, 1, 1);

  player.TransitionTransformOperationsTo(start_time, TRANSFORM, from, to);

  EXPECT_TRUE(from.ApproximatelyEqual(target.operations(), tolerance));
  player.Tick(start_time);

  // Scheduling a redundant, approximately equal transition should be ignored.
  int animation_id = player.animations().front()->id();
  cc::TransformOperations nearby = to;
  nearby.at(0).translate.x += kNoise;
  player.TransitionTransformOperationsTo(start_time, TRANSFORM, from, nearby);
  EXPECT_EQ(animation_id, player.animations().front()->id());

  player.Tick(start_time + MicrosecondsToDelta(5000));
  EXPECT_LT(from.at(0).translate.x, target.operations().at(0).translate.x);
  EXPECT_GT(to.at(0).translate.x, target.operations().at(0).translate.x);

  player.Tick(start_time + MicrosecondsToDelta(10000));
  EXPECT_TRUE(to.ApproximatelyEqual(target.operations(), tolerance));
}

TEST(AnimationPlayerTest, ReversedTransformTransitions) {
  // In this test, we do expect exact equality.
  float tolerance = 0.0f;
  TestAnimationTarget target;
  AnimationPlayer player;
  player.set_target(&target);
  Transition transition;
  transition.target_properties = {TRANSFORM};
  transition.duration = MicrosecondsToDelta(10000);
  player.set_transition(transition);
  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  player.Tick(start_time);

  cc::TransformOperations from = target.operations();

  cc::TransformOperations to;
  to.AppendTranslate(8, 0, 0);
  to.AppendRotate(1, 0, 0, 0);
  to.AppendScale(1, 1, 1);

  player.TransitionTransformOperationsTo(start_time, TRANSFORM, from, to);

  EXPECT_TRUE(from.ApproximatelyEqual(target.operations(), tolerance));
  player.Tick(start_time);

  player.Tick(start_time + MicrosecondsToDelta(1000));
  cc::TransformOperations value_before_reversing = target.operations();
  EXPECT_LT(from.at(0).translate.x, target.operations().at(0).translate.x);
  EXPECT_GT(to.at(0).translate.x, target.operations().at(0).translate.x);

  player.TransitionTransformOperationsTo(start_time + MicrosecondsToDelta(1000),
                                         TRANSFORM, target.operations(), from);
  player.Tick(start_time + MicrosecondsToDelta(1000));
  EXPECT_TRUE(value_before_reversing.ApproximatelyEqual(target.operations(),
                                                        tolerance));

  player.Tick(start_time + MicrosecondsToDelta(2000));
  EXPECT_TRUE(from.ApproximatelyEqual(target.operations(), tolerance));
}

TEST(AnimationPlayerTest, BoundsTransitions) {
  TestAnimationTarget target;
  AnimationPlayer player;
  player.set_target(&target);
  Transition transition;
  transition.target_properties = {BOUNDS};
  transition.duration = MicrosecondsToDelta(10000);
  player.set_transition(transition);
  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  player.Tick(start_time);

  gfx::SizeF from = target.size();
  gfx::SizeF to(20.0f, 20.0f);

  player.TransitionSizeTo(start_time, BOUNDS, from, to);

  EXPECT_FLOAT_SIZE_EQ(from, target.size());
  player.Tick(start_time);

  // Scheduling a redundant, approximately equal transition should be ignored.
  int animation_id = player.animations().front()->id();
  gfx::SizeF nearby = to;
  nearby.set_width(to.width() + kNoise);
  player.TransitionSizeTo(start_time, BOUNDS, from, nearby);
  EXPECT_EQ(animation_id, player.animations().front()->id());

  player.Tick(start_time + MicrosecondsToDelta(5000));
  EXPECT_LT(from.width(), target.size().width());
  EXPECT_GT(to.width(), target.size().width());
  EXPECT_LT(from.height(), target.size().height());
  EXPECT_GT(to.height(), target.size().height());

  player.Tick(start_time + MicrosecondsToDelta(10000));
  EXPECT_FLOAT_SIZE_EQ(to, target.size());
}

TEST(AnimationPlayerTest, ReversedBoundsTransitions) {
  TestAnimationTarget target;
  AnimationPlayer player;
  player.set_target(&target);
  Transition transition;
  transition.target_properties = {BOUNDS};
  transition.duration = MicrosecondsToDelta(10000);
  player.set_transition(transition);
  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  player.Tick(start_time);

  gfx::SizeF from = target.size();
  gfx::SizeF to(20.0f, 20.0f);

  player.TransitionSizeTo(start_time, BOUNDS, from, to);

  EXPECT_FLOAT_SIZE_EQ(from, target.size());
  player.Tick(start_time);

  player.Tick(start_time + MicrosecondsToDelta(1000));
  gfx::SizeF value_before_reversing = target.size();
  EXPECT_LT(from.width(), target.size().width());
  EXPECT_GT(to.width(), target.size().width());
  EXPECT_LT(from.height(), target.size().height());
  EXPECT_GT(to.height(), target.size().height());

  player.TransitionSizeTo(start_time + MicrosecondsToDelta(1000), BOUNDS,
                          target.size(), from);
  player.Tick(start_time + MicrosecondsToDelta(1000));
  EXPECT_FLOAT_SIZE_EQ(value_before_reversing, target.size());

  player.Tick(start_time + MicrosecondsToDelta(2000));
  EXPECT_FLOAT_SIZE_EQ(from, target.size());
}

TEST(AnimationPlayerTest, BackgroundColorTransitions) {
  TestAnimationTarget target;
  AnimationPlayer player;
  player.set_target(&target);
  Transition transition;
  transition.target_properties = {BACKGROUND_COLOR};
  transition.duration = MicrosecondsToDelta(10000);
  player.set_transition(transition);
  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  player.Tick(start_time);

  SkColor from = SK_ColorRED;
  SkColor to = SK_ColorGREEN;

  player.TransitionColorTo(start_time, BACKGROUND_COLOR, from, to);

  EXPECT_EQ(from, target.background_color());
  player.Tick(start_time);

  player.Tick(start_time + MicrosecondsToDelta(5000));
  EXPECT_GT(SkColorGetR(from), SkColorGetR(target.background_color()));
  EXPECT_LT(SkColorGetR(to), SkColorGetR(target.background_color()));
  EXPECT_LT(SkColorGetG(from), SkColorGetG(target.background_color()));
  EXPECT_GT(SkColorGetG(to), SkColorGetG(target.background_color()));
  EXPECT_EQ(0u, SkColorGetB(target.background_color()));
  EXPECT_EQ(255u, SkColorGetA(target.background_color()));

  player.Tick(start_time + MicrosecondsToDelta(10000));
  EXPECT_EQ(to, target.background_color());
}

TEST(AnimationPlayerTest, ReversedBackgroundColorTransitions) {
  TestAnimationTarget target;
  AnimationPlayer player;
  player.set_target(&target);
  Transition transition;
  transition.target_properties = {BACKGROUND_COLOR};
  transition.duration = MicrosecondsToDelta(10000);
  player.set_transition(transition);
  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  player.Tick(start_time);

  SkColor from = SK_ColorRED;
  SkColor to = SK_ColorGREEN;

  player.TransitionColorTo(start_time, BACKGROUND_COLOR, from, to);

  EXPECT_EQ(from, target.background_color());
  player.Tick(start_time);

  player.Tick(start_time + MicrosecondsToDelta(1000));
  SkColor value_before_reversing = target.background_color();
  EXPECT_GT(SkColorGetR(from), SkColorGetR(target.background_color()));
  EXPECT_LT(SkColorGetR(to), SkColorGetR(target.background_color()));
  EXPECT_LT(SkColorGetG(from), SkColorGetG(target.background_color()));
  EXPECT_GT(SkColorGetG(to), SkColorGetG(target.background_color()));
  EXPECT_EQ(0u, SkColorGetB(target.background_color()));
  EXPECT_EQ(255u, SkColorGetA(target.background_color()));

  player.TransitionColorTo(start_time + MicrosecondsToDelta(1000),
                           BACKGROUND_COLOR, target.background_color(), from);
  player.Tick(start_time + MicrosecondsToDelta(1000));
  EXPECT_EQ(value_before_reversing, target.background_color());

  player.Tick(start_time + MicrosecondsToDelta(2000));
  EXPECT_EQ(from, target.background_color());
}

TEST(AnimationPlayerTest, DoubleReversedTransitions) {
  TestAnimationTarget target;
  AnimationPlayer player;
  player.set_target(&target);
  Transition transition;
  transition.target_properties = {OPACITY};
  transition.duration = MicrosecondsToDelta(10000);
  player.set_transition(transition);

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  player.Tick(start_time);

  float from = 1.0f;
  float to = 0.5f;
  player.TransitionFloatTo(start_time, OPACITY, from, to);

  EXPECT_EQ(from, target.opacity());
  player.Tick(start_time);

  player.Tick(start_time + MicrosecondsToDelta(1000));
  float value_before_reversing = target.opacity();
  EXPECT_GT(from, value_before_reversing);
  EXPECT_LT(to, value_before_reversing);

  player.TransitionFloatTo(start_time + MicrosecondsToDelta(1000), OPACITY,
                           target.opacity(), from);
  player.Tick(start_time + MicrosecondsToDelta(1000));
  EXPECT_FLOAT_EQ(value_before_reversing, target.opacity());

  player.Tick(start_time + MicrosecondsToDelta(1500));
  value_before_reversing = target.opacity();
  // If the code for reversing transitions does not account for an existing time
  // offset, then reversing a second time will give incorrect values.
  player.TransitionFloatTo(start_time + MicrosecondsToDelta(1500), OPACITY,
                           target.opacity(), to);
  player.Tick(start_time + MicrosecondsToDelta(1500));
  EXPECT_FLOAT_EQ(value_before_reversing, target.opacity());
}

TEST(AnimationPlayerTest, RedundantTransition) {
  TestAnimationTarget target;
  AnimationPlayer player;
  player.set_target(&target);
  Transition transition;
  transition.target_properties = {OPACITY};
  transition.duration = MicrosecondsToDelta(10000);
  player.set_transition(transition);

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  player.Tick(start_time);

  float from = 1.0f;
  float to = 0.5f;
  player.TransitionFloatTo(start_time, OPACITY, from, to);

  EXPECT_EQ(from, target.opacity());
  player.Tick(start_time);

  player.Tick(start_time + MicrosecondsToDelta(1000));
  float value_before_redundant_transition = target.opacity();

  // While an existing transition is in progress to the same value, we should
  // not start a new transition.
  player.TransitionFloatTo(start_time, OPACITY, target.opacity(), to);

  EXPECT_EQ(1lu, player.animations().size());
  EXPECT_EQ(value_before_redundant_transition, target.opacity());
}

TEST(AnimationPlayerTest, TransitionToSameValue) {
  TestAnimationTarget target;
  AnimationPlayer player;
  player.set_target(&target);
  Transition transition;
  transition.target_properties = {OPACITY};
  transition.duration = MicrosecondsToDelta(10000);
  player.set_transition(transition);

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  player.Tick(start_time);

  // Transitioning to the same value should be a no-op.
  float from = 1.0f;
  float to = 1.0f;
  player.TransitionFloatTo(start_time, OPACITY, from, to);
  EXPECT_EQ(from, target.opacity());
  EXPECT_TRUE(player.animations().empty());
}

TEST(AnimationPlayerTest, CorrectTargetValue) {
  TestAnimationTarget target;
  AnimationPlayer player;
  player.set_target(&target);
  base::TimeDelta duration = MicrosecondsToDelta(10000);

  float from_opacity = 1.0f;
  float to_opacity = 0.5f;
  gfx::SizeF from_bounds = gfx::SizeF(10, 200);
  gfx::SizeF to_bounds = gfx::SizeF(20, 200);
  SkColor from_color = SK_ColorRED;
  SkColor to_color = SK_ColorGREEN;
  cc::TransformOperations from_transform;
  from_transform.AppendTranslate(10, 100, 1000);
  cc::TransformOperations to_transform;
  to_transform.AppendTranslate(20, 200, 2000);

  // Verify the default value is returned if there's no running animations.
  EXPECT_EQ(from_opacity, player.GetTargetFloatValue(OPACITY, from_opacity));
  EXPECT_SIZEF_EQ(from_bounds, player.GetTargetSizeValue(BOUNDS, from_bounds));
  EXPECT_EQ(from_color,
            player.GetTargetColorValue(BACKGROUND_COLOR, from_color));
  EXPECT_TRUE(from_transform.ApproximatelyEqual(
      player.GetTargetTransformOperationsValue(TRANSFORM, from_transform),
      kEpsilon));

  // Add animations.
  player.AddAnimation(
      CreateOpacityAnimation(2, 1, from_opacity, to_opacity, duration));
  player.AddAnimation(
      CreateBoundsAnimation(1, 1, from_bounds, to_bounds, duration));
  player.AddAnimation(
      CreateBackgroundColorAnimation(3, 1, from_color, to_color, duration));
  player.AddAnimation(
      CreateTransformAnimation(4, 1, from_transform, to_transform, duration));

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  player.Tick(start_time);

  // Verify target value.
  EXPECT_EQ(to_opacity, player.GetTargetFloatValue(OPACITY, from_opacity));
  EXPECT_SIZEF_EQ(to_bounds, player.GetTargetSizeValue(BOUNDS, from_bounds));
  EXPECT_EQ(to_color, player.GetTargetColorValue(BACKGROUND_COLOR, from_color));
  EXPECT_TRUE(to_transform.ApproximatelyEqual(
      player.GetTargetTransformOperationsValue(TRANSFORM, from_transform),
      kEpsilon));
}

}  // namespace vr
