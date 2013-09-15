// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/loading_animation.h"

#include "base/logging.h"
#include "ui/gfx/animation/tween.h"

namespace autofill {

// Duration of one cycle of the animation.
static const int kDurationMs = 1800;

// The frame rate of the animation.
static const int kHertz = 60;

LoadingAnimation::LoadingAnimation(gfx::AnimationDelegate* delegate)
    : LinearAnimation(kDurationMs, kHertz, delegate),
      first_cycle_(true) {}

LoadingAnimation::~LoadingAnimation() {}

void LoadingAnimation::Step(base::TimeTicks time_now) {
  LinearAnimation::Step(time_now);

  if (!is_animating()) {
    first_cycle_ = false;
    Start();
  }
}

double LoadingAnimation::GetCurrentValueForDot(size_t dot_i) {
  double base_value = gfx::LinearAnimation::GetCurrentValue();

  const double kSecondDotDelayMs = 100.0;
  const double kThirdDotDelayMs = 300.0;
  if (dot_i == 1)
    base_value -= kSecondDotDelayMs / kDurationMs;
  else if (dot_i == 2)
    base_value -= kThirdDotDelayMs / kDurationMs;

  if (base_value < 0.0)
    base_value = first_cycle_ ? 0.0 : base_value + 1.0;

  double value = gfx::Tween::CalculateValue(gfx::Tween::EASE_OUT, base_value);

  static AnimationFrame animation_frames[] = {
    { 0.0, 0.0 },
    { 0.55, 0.0 },
    { 0.6, -1.0 },
    { 0.8, 0.3 },
    { 0.9, -0.2 },
    { 0.95, 0.1 },
    { 1.0, 0.0 },
  };

  for (size_t i = 0; i < arraysize(animation_frames); ++i) {
    if (value > animation_frames[i].value)
      continue;
    else if (i == 0)
      return animation_frames[i].position;

    return animation_frames[i - 1].position +
        (animation_frames[i].position - animation_frames[i - 1].position) *
            (value - animation_frames[i - 1].value) /
                (animation_frames[i].value - animation_frames[i - 1].value);
  }

  NOTREACHED();
  return 0.0;
}

void LoadingAnimation::Reset() {
  Stop();
  first_cycle_ = true;
}

}  // namespace autofill
