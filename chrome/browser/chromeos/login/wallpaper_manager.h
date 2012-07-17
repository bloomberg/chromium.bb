// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WALLPAPER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WALLPAPER_MANAGER_H_

#include <string>

#include "base/timer.h"
#include "chrome/browser/chromeos/power/resume_observer.h"
#include "chrome/browser/chromeos/system/timezone_settings.h"
#include "unicode/timezone.h"

namespace chromeos {

// This class maintains wallpapers for users who have logged into this Chrome
// OS device.
class WallpaperManager: public system::TimezoneSettings::Observer,
                        public chromeos::ResumeObserver {
 public:
  static WallpaperManager* Get();

  WallpaperManager();

  // Adds PowerManagerClient observer. It needs to be added after
  // PowerManagerClient initialized.
  void AddPowerManagerClientObserver();

  // Starts a one shot timer which calls BatchUpdateWallpaper at next midnight.
  // Cancel any previous timer if any.
  void RestartTimer();

  // Sets last selected user on user pod row.
  void SetLastSelectedUser(const std::string& last_selected_user);

  // User was deselected at login screen, reset wallpaper if needed.
  void UserDeselected();

 private:
  virtual ~WallpaperManager();

  // Change the wallpapers for users who choose DAILY wallpaper type. Updates
  // current wallpaper if it changed. This function should be called at exactly
  // at 0am if chromeos device is on.
  void BatchUpdateWallpaper();

  // Overridden from chromeos::ResumeObserver
  virtual void SystemResumed() OVERRIDE;

  // Overridden from system::TimezoneSettings::Observer.
  virtual void TimezoneChanged(const icu::TimeZone& timezone) OVERRIDE;

  // The last selected user on user pod row.
  std::string last_selected_user_;

  base::OneShotTimer<WallpaperManager> timer_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WALLPAPER_MANAGER_H_
