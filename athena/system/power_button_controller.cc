// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/system/power_button_controller.h"

#include "athena/screen/public/screen_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace athena {
namespace {

// Duration of the shutdown animation.
const int kShutdownDurationMs = 1000;

// Duration of the cancel shutdown animation.
const int kCancelShutdownDurationMs = 500;

}  // namespace

PowerButtonController::PowerButtonController()
    : brightness_is_zero_(false),
      state_(STATE_OTHER) {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
      this);
}

PowerButtonController::~PowerButtonController() {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(
      this);
}

void PowerButtonController::StartGrayscaleAndBrightnessAnimation(
    float target,
    int duration_ms,
    gfx::Tween::Type tween_type) {
  ui::LayerAnimator* animator = ScreenManager::Get()->GetScreenAnimator();
  ui::ScopedLayerAnimationSettings settings(animator);
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(duration_ms));
  settings.SetTweenType(tween_type);
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  settings.AddObserver(this);

  animator->SetBrightness(target);
  animator->SetGrayscale(target);
}

void PowerButtonController::BrightnessChanged(int level, bool user_initiated) {
  if (brightness_is_zero_)
    zero_brightness_end_time_ = base::TimeTicks::Now();
  brightness_is_zero_ = (level == 0);
}

void PowerButtonController::PowerButtonEventReceived(
    bool down,
    const base::TimeTicks& timestamp) {
  // Avoid requesting a shutdown if the power button is pressed while the screen
  // is off (http://crbug.com/128451)
  base::TimeDelta time_since_zero_brightness = brightness_is_zero_ ?
      base::TimeDelta() : (base::TimeTicks::Now() - zero_brightness_end_time_);
  const int kShortTimeMs = 10;
  if (time_since_zero_brightness.InMilliseconds() <= kShortTimeMs)
    return;

  if (state_ == STATE_SHUTDOWN_REQUESTED)
    return;

  StopObservingImplicitAnimations();
  if (down) {
    state_ = STATE_PRE_SHUTDOWN_ANIMATION;
    StartGrayscaleAndBrightnessAnimation(
        1.0f, kShutdownDurationMs, gfx::Tween::EASE_IN);
  } else {
    state_ = STATE_OTHER;
    StartGrayscaleAndBrightnessAnimation(
        0.0f, kCancelShutdownDurationMs, gfx::Tween::EASE_IN_OUT);
  }
}

void PowerButtonController::OnImplicitAnimationsCompleted() {
  if (state_ == STATE_PRE_SHUTDOWN_ANIMATION) {
    state_ = STATE_SHUTDOWN_REQUESTED;
    chromeos::DBusThreadManager::Get()
        ->GetPowerManagerClient()
        ->RequestShutdown();
  }
}

}  // namespace athena
