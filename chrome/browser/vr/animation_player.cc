// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/animation_player.h"

#include <algorithm>

#include "base/numerics/ranges.h"
#include "base/stl_util.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_target.h"
#include "cc/animation/animation_ticker.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "chrome/browser/vr/elements/ui_element.h"

namespace vr {

namespace {

static constexpr float kTolerance = 1e-5f;

static int s_next_animation_id = 1;
static int s_next_group_id = 1;

void ReverseAnimation(base::TimeTicks monotonic_time,
                      cc::Animation* animation) {
  animation->set_direction(animation->direction() ==
                                   cc::Animation::Direction::NORMAL
                               ? cc::Animation::Direction::REVERSE
                               : cc::Animation::Direction::NORMAL);
  // Our goal here is to reverse the given animation. That is, if
  // we're 20% of the way through the animation in the forward direction, we'd
  // like to be 80% of the way of the reversed animation (so it will end
  // quickly).
  //
  // We can modify our "progress" through an animation by modifying the "time
  // offset", a value added to the current time by the animation system before
  // applying any other adjustments.
  //
  // Let our start time be s, our current time be t, and our final time (or
  // duration) be d. After reversing the animation, we would like to start
  // sampling from d - t as depicted below.
  //
  //  Forward:
  //  s    t                         d
  //  |----|-------------------------|
  //
  //  Reversed:
  //  s                         t    d
  //  |----|--------------------|----|
  //       -----time-offset----->
  //
  // Now, if we let o represent our desired offset, we need to ensure that
  //   t = d - (o + t)
  //
  // That is, sampling at the current time in either the forward or reverse
  // curves must result in the same value, otherwise we'll get jank.
  //
  // This implies that,
  //   0 = d - o - 2t
  //   o = d - 2t
  //
  // Now if there was a previous offset, we must adjust d by that offset before
  // performing this computation, so it becomes d - o_old - 2t:
  animation->set_time_offset(animation->curve()->Duration() -
                             animation->time_offset() -
                             (2 * (monotonic_time - animation->start_time())));
}

std::unique_ptr<cc::CubicBezierTimingFunction>
CreateTransitionTimingFunction() {
  return cc::CubicBezierTimingFunction::CreatePreset(
      cc::CubicBezierTimingFunction::EaseType::EASE);
}

base::TimeDelta GetStartTime(cc::Animation* animation) {
  if (animation->direction() == cc::Animation::Direction::NORMAL) {
    return base::TimeDelta();
  }
  return animation->curve()->Duration();
}

base::TimeDelta GetEndTime(cc::Animation* animation) {
  if (animation->direction() == cc::Animation::Direction::REVERSE) {
    return base::TimeDelta();
  }
  return animation->curve()->Duration();
}

bool SufficientlyEqual(float lhs, float rhs) {
  return base::IsApproximatelyEqual(lhs, rhs, kTolerance);
}

bool SufficientlyEqual(const cc::TransformOperations& lhs,
                       const cc::TransformOperations& rhs) {
  return lhs.ApproximatelyEqual(rhs, kTolerance);
}

bool SufficientlyEqual(const gfx::SizeF& lhs, const gfx::SizeF& rhs) {
  return base::IsApproximatelyEqual(lhs.width(), rhs.width(), kTolerance) &&
         base::IsApproximatelyEqual(lhs.height(), rhs.height(), kTolerance);
}

bool SufficientlyEqual(SkColor lhs, SkColor rhs) {
  return lhs == rhs;
}

template <typename T>
struct AnimationTraits {};

#define DEFINE_ANIMATION_TRAITS(value_type, name, notify_name)                \
  template <>                                                                 \
  struct AnimationTraits<value_type> {                                        \
    typedef value_type ValueType;                                             \
    typedef cc::name##AnimationCurve CurveType;                               \
    typedef cc::Keyframed##name##AnimationCurve KeyframedCurveType;           \
    typedef cc::name##Keyframe KeyframeType;                                  \
    static const CurveType* ToDerivedCurve(const cc::AnimationCurve& curve) { \
      return curve.To##name##AnimationCurve();                                \
    }                                                                         \
    static void NotifyClientValueAnimated(                                    \
        cc::AnimationTarget* animation_target,                                \
        const ValueType& target_value,                                        \
        int target_property) {                                                \
      animation_target->NotifyClient##notify_name##Animated(                  \
          target_value, target_property, nullptr);                            \
    }                                                                         \
  };

DEFINE_ANIMATION_TRAITS(float, Float, Float);
DEFINE_ANIMATION_TRAITS(cc::TransformOperations,
                        Transform,
                        TransformOperations);
DEFINE_ANIMATION_TRAITS(gfx::SizeF, Size, Size);
DEFINE_ANIMATION_TRAITS(SkColor, Color, Color);

}  // namespace

int AnimationPlayer::GetNextAnimationId() {
  return s_next_animation_id++;
}

int AnimationPlayer::GetNextGroupId() {
  return s_next_group_id++;
}

AnimationPlayer::AnimationPlayer() {}
AnimationPlayer::~AnimationPlayer() {}

void AnimationPlayer::AddAnimation(std::unique_ptr<cc::Animation> animation) {
  animations_.push_back(std::move(animation));
}

void AnimationPlayer::RemoveAnimation(int animation_id) {
  base::EraseIf(
      animations_,
      [animation_id](const std::unique_ptr<cc::Animation>& animation) {
        return animation->id() == animation_id;
      });
}

void AnimationPlayer::RemoveAnimations(int target_property) {
  base::EraseIf(
      animations_,
      [target_property](const std::unique_ptr<cc::Animation>& animation) {
        return animation->target_property_id() == target_property;
      });
}

void AnimationPlayer::Tick(base::TimeTicks monotonic_time) {
  DCHECK(target_);

  StartAnimations(monotonic_time);

  for (auto& animation : animations_) {
    cc::AnimationTicker::TickAnimation(monotonic_time, animation.get(),
                                       target_);
  }

  // Remove finished animations.
  base::EraseIf(
      animations_,
      [monotonic_time](const std::unique_ptr<cc::Animation>& animation) {
        return !animation->is_finished() &&
               animation->IsFinishedAt(monotonic_time);
      });

  StartAnimations(monotonic_time);
}

void AnimationPlayer::SetTransitionedProperties(
    const std::set<int>& properties) {
  transition_.target_properties = properties;
}

void AnimationPlayer::TransitionFloatTo(base::TimeTicks monotonic_time,
                                        int target_property,
                                        float current,
                                        float target) {
  TransitionValueTo<float>(monotonic_time, target_property, current, target);
}

void AnimationPlayer::TransitionTransformOperationsTo(
    base::TimeTicks monotonic_time,
    int target_property,
    const cc::TransformOperations& current,
    const cc::TransformOperations& target) {
  TransitionValueTo<cc::TransformOperations>(monotonic_time, target_property,
                                             current, target);
}

void AnimationPlayer::TransitionSizeTo(base::TimeTicks monotonic_time,
                                       int target_property,
                                       const gfx::SizeF& current,
                                       const gfx::SizeF& target) {
  TransitionValueTo<gfx::SizeF>(monotonic_time, target_property, current,
                                target);
}

void AnimationPlayer::TransitionColorTo(base::TimeTicks monotonic_time,
                                        int target_property,
                                        SkColor current,
                                        SkColor target) {
  TransitionValueTo<SkColor>(monotonic_time, target_property, current, target);
}

bool AnimationPlayer::IsAnimatingProperty(int property) const {
  for (auto& animation : animations_) {
    if (animation->target_property_id() == property)
      return true;
  }
  return false;
}

float AnimationPlayer::GetTargetFloatValue(int target_property,
                                           float default_value) const {
  return GetTargetValue<float>(target_property, default_value);
}

cc::TransformOperations AnimationPlayer::GetTargetTransformOperationsValue(
    int target_property,
    const cc::TransformOperations& default_value) const {
  return GetTargetValue<cc::TransformOperations>(target_property,
                                                 default_value);
}

gfx::SizeF AnimationPlayer::GetTargetSizeValue(
    int target_property,
    const gfx::SizeF& default_value) const {
  return GetTargetValue<gfx::SizeF>(target_property, default_value);
}

SkColor AnimationPlayer::GetTargetColorValue(int target_property,
                                             SkColor default_value) const {
  return GetTargetValue<SkColor>(target_property, default_value);
}

void AnimationPlayer::StartAnimations(base::TimeTicks monotonic_time) {
  // TODO(vollick): support groups. crbug.com/742358
  cc::TargetProperties animated_properties;
  for (auto& animation : animations_) {
    if (animation->run_state() == cc::Animation::RUNNING ||
        animation->run_state() == cc::Animation::PAUSED) {
      animated_properties[animation->target_property_id()] = true;
    }
  }
  for (auto& animation : animations_) {
    if (!animated_properties[animation->target_property_id()] &&
        animation->run_state() ==
            cc::Animation::WAITING_FOR_TARGET_AVAILABILITY) {
      animated_properties[animation->target_property_id()] = true;
      animation->SetRunState(cc::Animation::RUNNING, monotonic_time);
      animation->set_start_time(monotonic_time);
    }
  }
}

template <typename ValueType>
void AnimationPlayer::TransitionValueTo(base::TimeTicks monotonic_time,
                                        int target_property,
                                        const ValueType& current,
                                        const ValueType& target) {
  DCHECK(target_);

  if (transition_.target_properties.find(target_property) ==
      transition_.target_properties.end()) {
    AnimationTraits<ValueType>::NotifyClientValueAnimated(target_, target,
                                                          target_property);
    return;
  }

  cc::Animation* running_animation =
      GetRunningAnimationForProperty(target_property);

  if (running_animation) {
    const auto* curve =
        AnimationTraits<ValueType>::ToDerivedCurve(*running_animation->curve());
    if (SufficientlyEqual(target,
                          curve->GetValue(GetEndTime(running_animation)))) {
      return;
    }
    if (SufficientlyEqual(target,
                          curve->GetValue(GetStartTime(running_animation)))) {
      ReverseAnimation(monotonic_time, running_animation);
      return;
    }
  } else if (SufficientlyEqual(target, current)) {
    return;
  }

  RemoveAnimations(target_property);

  std::unique_ptr<typename AnimationTraits<ValueType>::KeyframedCurveType>
      curve(AnimationTraits<ValueType>::KeyframedCurveType::Create());

  curve->AddKeyframe(AnimationTraits<ValueType>::KeyframeType::Create(
      base::TimeDelta(), current, CreateTransitionTimingFunction()));

  curve->AddKeyframe(AnimationTraits<ValueType>::KeyframeType::Create(
      transition_.duration, target, CreateTransitionTimingFunction()));

  AddAnimation(cc::Animation::Create(std::move(curve), GetNextAnimationId(),
                                     GetNextGroupId(), target_property));
}

cc::Animation* AnimationPlayer::GetRunningAnimationForProperty(
    int target_property) const {
  for (auto& animation : animations_) {
    if ((animation->run_state() == cc::Animation::RUNNING ||
         animation->run_state() == cc::Animation::PAUSED) &&
        animation->target_property_id() == target_property) {
      return animation.get();
    }
  }
  return nullptr;
}

cc::Animation* AnimationPlayer::GetAnimationForProperty(
    int target_property) const {
  for (auto& animation : animations_) {
    if (animation->target_property_id() == target_property) {
      return animation.get();
    }
  }
  return nullptr;
}

template <typename ValueType>
ValueType AnimationPlayer::GetTargetValue(
    int target_property,
    const ValueType& default_value) const {
  cc::Animation* running_animation = GetAnimationForProperty(target_property);
  if (!running_animation) {
    return default_value;
  }
  const auto* curve =
      AnimationTraits<ValueType>::ToDerivedCurve(*running_animation->curve());
  return curve->GetValue(GetEndTime(running_animation));
}

}  // namespace vr
