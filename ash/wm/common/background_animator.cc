// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/common/background_animator.h"

namespace ash {
namespace {

// Duration of the background animation.
const int kBackgroundDurationMS = 1000;
}

BackgroundAnimator::BackgroundAnimator(BackgroundAnimatorDelegate* delegate,
                                       int min_alpha,
                                       int max_alpha)
    : delegate_(delegate),
      min_alpha_(min_alpha),
      max_alpha_(max_alpha),
      animation_(this),
      paints_background_(false),
      alpha_(min_alpha) {
  animation_.SetSlideDuration(kBackgroundDurationMS);
}

BackgroundAnimator::~BackgroundAnimator() {}

void BackgroundAnimator::SetDuration(int time_in_ms) {
  animation_.SetSlideDuration(time_in_ms);
}

void BackgroundAnimator::SetPaintsBackground(
    bool value,
    BackgroundAnimatorChangeType type) {
  if (paints_background_ == value)
    return;
  paints_background_ = value;
  if (type == BACKGROUND_CHANGE_IMMEDIATE && !animation_.is_animating()) {
    animation_.Reset(value ? 1.0f : 0.0f);
    AnimationProgressed(&animation_);
    return;
  }
  if (paints_background_)
    animation_.Show();
  else
    animation_.Hide();
}

void BackgroundAnimator::AnimationProgressed(const gfx::Animation* animation) {
  int alpha = animation->CurrentValueBetween(min_alpha_, max_alpha_);
  if (alpha_ == alpha)
    return;
  alpha_ = alpha;
  delegate_->UpdateBackground(alpha_);
}

}  // namespace ash
