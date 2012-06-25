// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wallpaper_manager.h"

#include "ash/desktop_background/desktop_background_resources.h"
#include "base/logging.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/user_manager_impl.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"

namespace {
const int kWallpaperUpdateIntervalSec = 24 * 60 * 60;
}  // namespace

namespace chromeos {

WallpaperManager::WallpaperManager() : last_selected_user_("") {
  RestartTimer();
  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
  DBusThreadManager::Get()->GetPowerManagerClient()->RequestStatusUpdate(
      PowerManagerClient::UPDATE_INITIAL);
  system::TimezoneSettings::GetInstance()->AddObserver(this);
}

WallpaperManager::~WallpaperManager() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
  system::TimezoneSettings::GetInstance()->RemoveObserver(this);
}

void WallpaperManager::SetLastSelectedUser(std::string last_selected_user) {
  last_selected_user_ = last_selected_user;
}

void WallpaperManager::RestartTimer() {
  timer_.Stop();
  base::Time midnight = base::Time::Now().LocalMidnight();
  base::TimeDelta interval = base::Time::Now() - midnight;
  int64 remaining_seconds = kWallpaperUpdateIntervalSec - interval.InSeconds();
  if (remaining_seconds <= 0) {
    BatchUpdateWallpaper();
  } else {
    // Set up a one shot timer which will batch update wallpaper at midnight.
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromSeconds(remaining_seconds),
                 this,
                 &WallpaperManager::BatchUpdateWallpaper);
  }
}

void WallpaperManager::TimezoneChanged(const icu::TimeZone& timezone) {
  RestartTimer();
}

void WallpaperManager::SystemResumed() {
  BatchUpdateWallpaper();
}

void WallpaperManager::BatchUpdateWallpaper() {
  PrefService* local_state = g_browser_process->local_state();
  UserManager* user_manager = UserManager::Get();
  bool show_users = true;
  CrosSettings::Get()->GetBoolean(
      kAccountsPrefShowUserNamesOnSignIn, &show_users);
  if (local_state) {
    User::WallpaperType type;
    int index = 0;
    base::Time last_modification_date;
    const UserList& users = user_manager->GetUsers();
    for (UserList::const_iterator it = users.begin();
         it != users.end(); ++it) {
      std::string email = (*it)->email();
      // TODO(bshe): Move GetUserWallpaperProperties() to this class.
      static_cast<UserManagerImpl*>(user_manager)->
          GetUserWallpaperProperties(email, &type, &index,
                                     &last_modification_date);
      base::Time current_date = base::Time::Now().LocalMidnight();
      if (type == User::DAILY && current_date != last_modification_date) {
        index = ash::GetNextWallpaperIndex(index);
        // TODO(bshe): Move SaveUserWallpaperProperties() to this class.
        static_cast<UserManagerImpl*>(user_manager)->
            SaveUserWallpaperProperties(email, type, index);
      }
      // Force a wallpaper update for logged in / last selected user.
      // TODO(bshe): Notify lock screen, wallpaper picker UI to update wallpaper
      // as well.
      if (user_manager->IsUserLoggedIn() &&
          email == user_manager->GetLoggedInUser().email()) {
        user_manager->UserSelected(email);
      } else if (show_users &&
                 email == last_selected_user_) {
        user_manager->UserSelected(email);
      }
    }
  }
  RestartTimer();
}

}  // chromeos
