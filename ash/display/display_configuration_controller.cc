// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_configuration_controller.h"

#include "ash/display/display_animator.h"
#include "ash/display/display_layout.h"
#include "ash/display/display_manager.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/screen_util.h"
#include "base/time/time.h"

#if defined(OS_CHROMEOS)
#include "ash/display/display_animator_chromeos.h"
#include "base/sys_info.h"
#endif

namespace {

// Specifies how long the display change should have been disabled
// after each display change operations.
// |kCycleDisplayThrottleTimeoutMs| is set to be longer to avoid
// changing the settings while the system is still configurating
// displays. It will be overriden by |kAfterDisplayChangeThrottleTimeoutMs|
// when the display change happens, so the actual timeout is much shorter.
const int64_t kAfterDisplayChangeThrottleTimeoutMs = 500;
const int64_t kCycleDisplayThrottleTimeoutMs = 4000;
const int64_t kSetPrimaryDisplayThrottleTimeoutMs = 500;

}  // namespace

namespace ash {

class DisplayConfigurationController::DisplayChangeLimiter {
 public:
  DisplayChangeLimiter() : throttle_timeout_(base::Time::Now()) {}

  void SetThrottleTimeout(int64_t throttle_ms) {
    throttle_timeout_ =
        base::Time::Now() + base::TimeDelta::FromMilliseconds(throttle_ms);
  }

  bool IsThrottled() const { return base::Time::Now() < throttle_timeout_; }

 private:
  base::Time throttle_timeout_;

  DISALLOW_COPY_AND_ASSIGN(DisplayChangeLimiter);
};

DisplayConfigurationController::DisplayConfigurationController(
    DisplayManager* display_manager,
    WindowTreeHostManager* window_tree_host_manager)
    : display_manager_(display_manager),
      window_tree_host_manager_(window_tree_host_manager),
      weak_ptr_factory_(this) {
  window_tree_host_manager_->AddObserver(this);
#if defined(OS_CHROMEOS)
  if (base::SysInfo::IsRunningOnChromeOS())
    limiter_.reset(new DisplayChangeLimiter);
  display_animator_.reset(new DisplayAnimatorChromeOS());
#endif
}

DisplayConfigurationController::~DisplayConfigurationController() {
  window_tree_host_manager_->RemoveObserver(this);
}

void DisplayConfigurationController::SetDisplayLayout(
    const DisplayLayout& layout,
    bool user_action) {
  if (user_action && display_animator_) {
    display_animator_->StartFadeOutAnimation(
        base::Bind(&DisplayConfigurationController::SetDisplayLayoutImpl,
                   weak_ptr_factory_.GetWeakPtr(), layout));
  } else {
    SetDisplayLayoutImpl(layout);
  }
}

void DisplayConfigurationController::SetMirrorMode(bool mirror,
                                                   bool user_action) {
  if (display_manager_->num_connected_displays() <= 1 ||
      display_manager_->IsInMirrorMode() == mirror || IsLimited()) {
    return;
  }
  if (user_action && display_animator_) {
    display_animator_->StartFadeOutAnimation(
        base::Bind(&DisplayConfigurationController::SetMirrorModeImpl,
                   weak_ptr_factory_.GetWeakPtr(), mirror));
  } else {
    SetMirrorModeImpl(mirror);
  }
}

void DisplayConfigurationController::SetDisplayRotation(
    int64_t display_id,
    gfx::Display::Rotation rotation,
    gfx::Display::RotationSource source,
    bool user_action) {
  ash::ScreenRotationAnimator screen_rotation_animator(display_id);
  if (user_action && screen_rotation_animator.CanAnimate())
    screen_rotation_animator.Rotate(rotation, source);
  else
    display_manager_->SetDisplayRotation(display_id, rotation, source);
}

void DisplayConfigurationController::SetPrimaryDisplayId(int64_t display_id,
                                                         bool user_action) {
  if (display_manager_->GetNumDisplays() <= 1 || IsLimited())
    return;

  if (user_action && display_animator_) {
    display_animator_->StartFadeOutAnimation(
        base::Bind(&DisplayConfigurationController::SetPrimaryDisplayIdImpl,
                   weak_ptr_factory_.GetWeakPtr(), display_id));
  } else {
    SetPrimaryDisplayIdImpl(display_id);
  }
}

void DisplayConfigurationController::OnDisplayConfigurationChanged() {
  // TODO(oshima): Stop all animations.
  if (limiter_)
    limiter_->SetThrottleTimeout(kAfterDisplayChangeThrottleTimeoutMs);
}

// Protected

void DisplayConfigurationController::ResetAnimatorForTest() {
  if (!display_animator_)
    return;
  display_animator_.reset();
}

// Private

bool DisplayConfigurationController::IsLimited() {
  return limiter_ && limiter_->IsThrottled();
}

void DisplayConfigurationController::SetDisplayLayoutImpl(
    const DisplayLayout& layout) {
  // TODO(oshima/stevenjb): Add support for 3+ displays.
  display_manager_->SetLayoutForCurrentDisplays(layout);
  if (display_animator_)
    display_animator_->StartFadeInAnimation();
}

void DisplayConfigurationController::SetMirrorModeImpl(bool mirror) {
  display_manager_->SetMirrorMode(mirror);
  if (limiter_)
    limiter_->SetThrottleTimeout(kCycleDisplayThrottleTimeoutMs);
  if (display_animator_)
    display_animator_->StartFadeInAnimation();
}

void DisplayConfigurationController::SetPrimaryDisplayIdImpl(
    int64_t display_id) {
  window_tree_host_manager_->SetPrimaryDisplayId(display_id);
  if (limiter_)
    limiter_->SetThrottleTimeout(kSetPrimaryDisplayThrottleTimeoutMs);
  if (display_animator_)
    display_animator_->StartFadeInAnimation();
}

}  // namespace ash
