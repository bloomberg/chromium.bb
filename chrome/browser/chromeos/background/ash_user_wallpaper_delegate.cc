// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/background/ash_user_wallpaper_delegate.h"

#include "ash/shell.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/wm/window_animations.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/extensions/wallpaper_manager_util.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wallpaper_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {

namespace {

bool IsNormalWallpaperChange() {
  if (chromeos::UserManager::Get()->IsUserLoggedIn() ||
      !CommandLine::ForCurrentProcess()->HasSwitch(switches::kFirstBoot) ||
      WizardController::IsZeroDelayEnabled() ||
      !CommandLine::ForCurrentProcess()->HasSwitch(switches::kLoginManager)) {
    return true;
  }

  return false;
}

class UserWallpaperDelegate : public ash::UserWallpaperDelegate {
 public:
  UserWallpaperDelegate() : boot_animation_finished_(false) {
  }

  virtual ~UserWallpaperDelegate() {
  }

  virtual int GetAnimationType() OVERRIDE {
    return ShouldShowInitialAnimation() ?
        ash::WINDOW_VISIBILITY_ANIMATION_TYPE_BRIGHTNESS_GRAYSCALE :
        static_cast<int>(views::corewm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  }

  virtual bool ShouldShowInitialAnimation() OVERRIDE {
    if (IsNormalWallpaperChange() || boot_animation_finished_)
      return false;

    // It is a first boot case now. If kDisableBootAnimation flag
    // is passed, it only disables any transition after OOBE.
    // |kDisableOobeAnimation| disables OOBE animation for slow hardware.
    bool is_registered = WizardController::IsDeviceRegistered();
    const CommandLine* command_line = CommandLine::ForCurrentProcess();
    bool disable_boot_animation = command_line->
        HasSwitch(switches::kDisableBootAnimation);
    bool disable_oobe_animation = command_line->
        HasSwitch(switches::kDisableOobeAnimation);
    if ((!is_registered && disable_oobe_animation) ||
        (is_registered && disable_boot_animation))
      return false;

    return true;
  }

  virtual void UpdateWallpaper() OVERRIDE {
    chromeos::WallpaperManager::Get()->UpdateWallpaper();
  }

  virtual void InitializeWallpaper() OVERRIDE {
    chromeos::WallpaperManager::Get()->InitializeWallpaper();
  }

  virtual void OpenSetWallpaperPage() OVERRIDE {
    wallpaper_manager_util::OpenWallpaperManager();
  }

  virtual bool CanOpenSetWallpaperPage() OVERRIDE {
    return !chromeos::UserManager::Get()->IsLoggedInAsGuest();
  }

  virtual void OnWallpaperAnimationFinished() OVERRIDE {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_WALLPAPER_ANIMATION_FINISHED,
        content::NotificationService::AllSources(),
        content::NotificationService::NoDetails());
  }

  virtual void OnWallpaperBootAnimationFinished() OVERRIDE {
    // Make sure that boot animation type is used only once.
    boot_animation_finished_ = true;
  }

 private:
  bool boot_animation_finished_;

  DISALLOW_COPY_AND_ASSIGN(UserWallpaperDelegate);
};

}  // namespace

ash::UserWallpaperDelegate* CreateUserWallpaperDelegate() {
  return new chromeos::UserWallpaperDelegate();
}

}  // namespace chromeos
