// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/animation_player.h"

#include <algorithm>

#include "base/stl_util.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_player.h"
#include "cc/animation/animation_target.h"
#include "cc/base/math_util.h"
#include "chrome/browser/vr/elements/ui_element.h"

namespace vr {
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
    if (animation->run_state() == cc::Animation::STARTING ||
        animation->run_state() == cc::Animation::RUNNING ||
        animation->run_state() == cc::Animation::PAUSED) {
      animated_properties[animation->target_property()] = true;
    }
  }
  for (auto& animation : animations_) {
    if (!animated_properties[animation->target_property()] &&
        animation->run_state() ==
            cc::Animation::WAITING_FOR_TARGET_AVAILABILITY) {
      animated_properties[animation->target_property()] = true;
      animation->SetRunState(cc::Animation::STARTING, monotonic_time);
    }
  }
}

void AnimationPlayer::Tick(base::TimeTicks monotonic_time) {
  DCHECK(target_);

  StartAnimations(monotonic_time);

  for (auto& animation : animations_) {
    cc::AnimationPlayer::TickAnimation(monotonic_time, animation.get(),
                                       target_);
    if (animation->run_state() == cc::Animation::STARTING)
      animation->SetRunState(cc::Animation::RUNNING, monotonic_time);
  }

  // Remove finished animations.
  base::EraseIf(
      animations_,
      [monotonic_time](const std::unique_ptr<cc::Animation>& animation) {
        return !animation->is_finished() &&
               animation->IsFinishedAt(monotonic_time);
      });
}

}  // namespace vr
