// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/throbber.h"

#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/timing_function.h"
#include "cc/animation/transform_operations.h"
#include "chrome/browser/vr/target_property.h"

namespace vr {

namespace {
constexpr float kStartScale = 1.0f;
constexpr float kEndScale = 2.0f;
constexpr int kCircleGrowAnimationTimeMs = 1000;
}  // namespace

Throbber::Throbber() = default;
Throbber::~Throbber() = default;

void Throbber::NotifyClientFloatAnimated(float value,
                                         int target_property_id,
                                         cc::Animation* animation) {
  if (target_property_id == CIRCLE_GROW) {
    DCHECK(!IsAnimatingProperty(TRANSFORM));
    DCHECK(!IsAnimatingProperty(OPACITY));

    SetScale(scale_before_animation_.scale.x * value,
             scale_before_animation_.scale.y * value,
             scale_before_animation_.scale.z);
    float animation_progress =
        (value - kStartScale) / (kEndScale - kStartScale);
    SetOpacity(opacity_before_animation_ * (1.0 - animation_progress));
    return;
  }
  Rect::NotifyClientFloatAnimated(value, target_property_id, animation);
}

void Throbber::SetCircleGrowAnimationEnabled(bool enabled) {
  if (!enabled) {
    if (animation_player().IsAnimatingProperty(CIRCLE_GROW)) {
      SetOpacity(opacity_before_animation_);
      SetScale(scale_before_animation_.scale.x, scale_before_animation_.scale.y,
               scale_before_animation_.scale.z);
    }
    animation_player().RemoveAnimations(CIRCLE_GROW);
    return;
  }

  if (animation_player().IsAnimatingProperty(CIRCLE_GROW))
    return;

  scale_before_animation_ = GetTargetTransform().at(kScaleIndex);
  opacity_before_animation_ = GetTargetOpacity();
  std::unique_ptr<cc::KeyframedFloatAnimationCurve> curve(
      cc::KeyframedFloatAnimationCurve::Create());

  curve->AddKeyframe(
      cc::FloatKeyframe::Create(base::TimeDelta(), kStartScale, nullptr));
  curve->AddKeyframe(cc::FloatKeyframe::Create(
      base::TimeDelta::FromMilliseconds(kCircleGrowAnimationTimeMs), kEndScale,
      nullptr));

  std::unique_ptr<cc::Animation> animation(cc::Animation::Create(
      std::move(curve), AnimationPlayer::GetNextAnimationId(),
      AnimationPlayer::GetNextGroupId(), CIRCLE_GROW));
  animation->set_iterations(-1);
  AddAnimation(std::move(animation));
}

}  // namespace vr
