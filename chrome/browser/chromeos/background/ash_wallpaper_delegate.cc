// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/background/ash_wallpaper_delegate.h"

#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_delegate.h"
#include "ash/wm/window_animation_types.h"
#include "ash/wm/window_animations.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/login_state.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {

namespace {

bool IsNormalWallpaperChange() {
  if (chromeos::LoginState::Get()->IsUserLoggedIn() ||
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kFirstExecAfterBoot) ||
      WizardController::IsZeroDelayEnabled() ||
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kLoginManager)) {
    return true;
  }

  return false;
}

class WallpaperDelegate : public ash::WallpaperDelegate {
 public:
  WallpaperDelegate()
      : boot_animation_finished_(false),
        animation_duration_override_in_ms_(0) {}

  ~WallpaperDelegate() override {}

  int GetAnimationType() override {
    return ShouldShowInitialAnimation()
               ? ash::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_BRIGHTNESS_GRAYSCALE
               : static_cast<int>(::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  }

  int GetAnimationDurationOverride() override {
    return animation_duration_override_in_ms_;
  }

  void SetAnimationDurationOverride(int animation_duration_in_ms) override {
    animation_duration_override_in_ms_ = animation_duration_in_ms;
  }

  bool ShouldShowInitialAnimation() override {
    if (IsNormalWallpaperChange() || boot_animation_finished_)
      return false;
    return true;
  }

  void InitializeWallpaper() override {
    chromeos::WallpaperManager::Get()->InitializeWallpaper();
  }

  void OnWallpaperAnimationFinished() override {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_WALLPAPER_ANIMATION_FINISHED,
        content::NotificationService::AllSources(),
        content::NotificationService::NoDetails());
  }

  void OnWallpaperBootAnimationFinished() override {
    // Make sure that boot animation type is used only once.
    boot_animation_finished_ = true;
  }

 private:
  bool boot_animation_finished_;

  // The animation duration to show a new wallpaper if an animation is required.
  int animation_duration_override_in_ms_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperDelegate);
};

}  // namespace

ash::WallpaperDelegate* CreateWallpaperDelegate() {
  return new chromeos::WallpaperDelegate();
}

}  // namespace chromeos
