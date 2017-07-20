// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/animation_player.h"

#include <algorithm>

#include "base/stl_util.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_player.h"
#include "cc/animation/animation_target.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/base/math_util.h"
#include "chrome/browser/vr/elements/ui_element.h"

namespace vr {

namespace {

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
  animation->set_time_offset(animation->curve()->Duration() -
                             (2 * (monotonic_time - animation->start_time())));
}

std::unique_ptr<cc::CubicBezierTimingFunction>
CreateTransitionTimingFunction() {
  return cc::CubicBezierTimingFunction::CreatePreset(
      cc::CubicBezierTimingFunction::EaseType::EASE);
}

}  // namespace

AnimationPlayer::AnimationPlayer() {}
AnimationPlayer::~AnimationPlayer() {}

int AnimationPlayer::GetNextAnimationId() {
  return s_next_animation_id++;
}

int AnimationPlayer::GetNextGroupId() {
  return s_next_group_id++;
}

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

void AnimationPlayer::RemoveAnimations(
    cc::TargetProperty::Type target_property) {
  base::EraseIf(
      animations_,
      [target_property](const std::unique_ptr<cc::Animation>& animation) {
        return animation->target_property() == target_property;
      });
}

void AnimationPlayer::StartAnimations(base::TimeTicks monotonic_time) {
  // TODO(vollick): support groups. crbug.com/742358
  cc::TargetProperties animated_properties;
  for (auto& animation : animations_) {
    if (animation->run_state() == cc::Animation::RUNNING ||
        animation->run_state() == cc::Animation::PAUSED) {
      animated_properties[animation->target_property()] = true;
    }
  }
  for (auto& animation : animations_) {
    if (!animated_properties[animation->target_property()] &&
        animation->run_state() ==
            cc::Animation::WAITING_FOR_TARGET_AVAILABILITY) {
      animated_properties[animation->target_property()] = true;
      animation->SetRunState(cc::Animation::RUNNING, monotonic_time);
      animation->set_start_time(monotonic_time);
    }
  }
}

void AnimationPlayer::Tick(base::TimeTicks monotonic_time) {
  DCHECK(target_);

  StartAnimations(monotonic_time);

  for (auto& animation : animations_) {
    cc::AnimationPlayer::TickAnimation(monotonic_time, animation.get(),
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
    const std::vector<cc::TargetProperty::Type>& properties) {
  transition_.target_properties.reset();
  for (auto property : properties) {
    transition_.target_properties[property] = true;
  }
}

void AnimationPlayer::TransitionOpacityTo(base::TimeTicks monotonic_time,
                                          float current,
                                          float target) {
  DCHECK(target_);

  if (!transition_.target_properties[cc::TargetProperty::OPACITY]) {
    target_->NotifyClientOpacityAnimated(target, nullptr);
    return;
  }

  cc::Animation* running_animation =
      GetRunningAnimationForProperty(cc::TargetProperty::OPACITY);

  if (running_animation &&
      target == running_animation->curve()->ToFloatAnimationCurve()->GetValue(
                    base::TimeDelta())) {
    ReverseAnimation(monotonic_time, running_animation);
    return;
  }

  RemoveAnimations(cc::TargetProperty::OPACITY);

  std::unique_ptr<cc::KeyframedFloatAnimationCurve> curve(
      cc::KeyframedFloatAnimationCurve::Create());

  curve->AddKeyframe(cc::FloatKeyframe::Create(
      base::TimeDelta(), current, CreateTransitionTimingFunction()));

  curve->AddKeyframe(cc::FloatKeyframe::Create(
      transition_.duration, target, CreateTransitionTimingFunction()));

  std::unique_ptr<cc::Animation> animation(
      cc::Animation::Create(std::move(curve), GetNextAnimationId(),
                            GetNextGroupId(), cc::TargetProperty::OPACITY));

  AddAnimation(std::move(animation));
}

void AnimationPlayer::TransitionTransformOperationsTo(
    base::TimeTicks monotonic_time,
    const cc::TransformOperations& current,
    const cc::TransformOperations& target) {
  DCHECK(target_);

  if (!transition_.target_properties[cc::TargetProperty::TRANSFORM]) {
    target_->NotifyClientTransformOperationsAnimated(target, nullptr);
    return;
  }

  cc::Animation* running_animation =
      GetRunningAnimationForProperty(cc::TargetProperty::TRANSFORM);

  if (running_animation &&
      target ==
          running_animation->curve()->ToTransformAnimationCurve()->GetValue(
              base::TimeDelta())) {
    ReverseAnimation(monotonic_time, running_animation);
    return;
  }

  RemoveAnimations(cc::TargetProperty::TRANSFORM);

  std::unique_ptr<cc::KeyframedTransformAnimationCurve> curve(
      cc::KeyframedTransformAnimationCurve::Create());

  curve->AddKeyframe(cc::TransformKeyframe::Create(
      base::TimeDelta(), current, CreateTransitionTimingFunction()));

  curve->AddKeyframe(cc::TransformKeyframe::Create(
      transition_.duration, target, CreateTransitionTimingFunction()));

  AddAnimation(cc::Animation::Create(std::move(curve), GetNextAnimationId(),
                                     GetNextGroupId(),
                                     cc::TargetProperty::TRANSFORM));
}

void AnimationPlayer::TransitionBoundsTo(base::TimeTicks monotonic_time,
                                         const gfx::SizeF& current,
                                         const gfx::SizeF& target) {
  DCHECK(target_);

  if (!transition_.target_properties[cc::TargetProperty::BOUNDS]) {
    target_->NotifyClientBoundsAnimated(target, nullptr);
    return;
  }

  cc::Animation* running_animation =
      GetRunningAnimationForProperty(cc::TargetProperty::BOUNDS);

  if (running_animation &&
      target == running_animation->curve()->ToSizeAnimationCurve()->GetValue(
                    base::TimeDelta())) {
    ReverseAnimation(monotonic_time, running_animation);
    return;
  }

  RemoveAnimations(cc::TargetProperty::BOUNDS);

  std::unique_ptr<cc::KeyframedSizeAnimationCurve> curve(
      cc::KeyframedSizeAnimationCurve::Create());

  curve->AddKeyframe(cc::SizeKeyframe::Create(
      base::TimeDelta(), current, CreateTransitionTimingFunction()));

  curve->AddKeyframe(cc::SizeKeyframe::Create(
      transition_.duration, target, CreateTransitionTimingFunction()));

  AddAnimation(cc::Animation::Create(std::move(curve), GetNextAnimationId(),
                                     GetNextGroupId(),
                                     cc::TargetProperty::BOUNDS));
}

cc::Animation* AnimationPlayer::GetRunningAnimationForProperty(
    cc::TargetProperty::Type target_property) const {
  for (auto& animation : animations_) {
    if ((animation->run_state() == cc::Animation::RUNNING ||
         animation->run_state() == cc::Animation::PAUSED) &&
        animation->target_property() == target_property) {
      return animation.get();
    }
  }
  return nullptr;
}

bool AnimationPlayer::IsAnimatingProperty(
    cc::TargetProperty::Type property) const {
  for (auto& animation : animations_) {
    if (animation->target_property() == property)
      return true;
  }
  return false;
}

}  // namespace vr
