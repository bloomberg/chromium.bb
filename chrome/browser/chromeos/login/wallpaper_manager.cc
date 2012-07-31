// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wallpaper_manager.h"

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/desktop_background_resources.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/user_manager_impl.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

const int kWallpaperUpdateIntervalSec = 24 * 60 * 60;

// Names of nodes with info about wallpaper.
const char kWallpaperDateNodeName[] = "date";
const char kWallpaperLayoutNodeName[] = "layout";
const char kWallpaperFileNodeName[] = "file";
const char kWallpaperTypeNodeName[] = "type";

}  // namespace

namespace chromeos {

static WallpaperManager* g_wallpaper_manager = NULL;

// WallpaperManager, public: ---------------------------------------------------

// static
WallpaperManager* WallpaperManager::Get() {
  if (!g_wallpaper_manager)
    g_wallpaper_manager = new WallpaperManager();
  return g_wallpaper_manager;
}

// static
void WallpaperManager::RegisterPrefs(PrefService* local_state) {
  local_state->RegisterDictionaryPref(prefs::kUsersWallpaperInfo,
                                      PrefService::UNSYNCABLE_PREF);
}

WallpaperManager::WallpaperManager() : last_selected_user_("") {
  system::TimezoneSettings::GetInstance()->AddObserver(this);
  RestartTimer();
}

void WallpaperManager::AddPowerManagerClientObserver() {
  if (!DBusThreadManager::Get()->GetPowerManagerClient()->HasObserver(this))
    DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
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

void WallpaperManager::SaveUserWallpaperInfo(const std::string& username,
                                             const std::string& file_name,
                                             ash::WallpaperLayout layout,
                                             User::WallpaperType type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Ephemeral users can not save data to local state. We just cache the index
  // in memory for them.
  if (UserManager::Get()->IsCurrentUserEphemeral())
    return;

  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate wallpaper_update(local_state,
                                        prefs::kUsersWallpaperInfo);

  base::DictionaryValue* wallpaper_properties = new base::DictionaryValue();
  wallpaper_properties->SetString(kWallpaperDateNodeName,
      base::Int64ToString(base::Time::Now().LocalMidnight().ToInternalValue()));
  wallpaper_properties->SetString(kWallpaperFileNodeName, file_name);
  wallpaper_properties->SetInteger(kWallpaperLayoutNodeName, layout);
  wallpaper_properties->SetInteger(kWallpaperTypeNodeName, type);
  wallpaper_update->SetWithoutPathExpansion(username, wallpaper_properties);
}

void WallpaperManager::SetLastSelectedUser(
    const std::string& last_selected_user) {
  last_selected_user_ = last_selected_user;
}

void WallpaperManager::SetWallpaperFromFilePath(const std::string& path,
                                                ash::WallpaperLayout layout) {
  image_loader_->Start(
      path, 0, false,
      base::Bind(&WallpaperManager::OnWallpaperLoaded,
                 base::Unretained(this), layout));
}

void WallpaperManager::SetWallpaperFromImageSkia(
    const gfx::ImageSkia& wallpaper,
    ash::WallpaperLayout layout) {
  ash::Shell::GetInstance()->desktop_background_controller()->
      SetCustomWallpaper(wallpaper, layout);
}

void WallpaperManager::UserDeselected() {
  if (!UserManager::Get()->IsUserLoggedIn()) {
    // This will set default login wallpaper (#fefefe).
    ash::Shell::GetInstance()->desktop_background_controller()->
        SetDefaultWallpaper(ash::GetSolidColorIndex());
  }
}

// WallpaperManager, private: --------------------------------------------------

WallpaperManager::~WallpaperManager() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
  system::TimezoneSettings::GetInstance()->RemoveObserver(this);
}

void WallpaperManager::OnCustomWallpaperLoaded(const std::string& email,
                                               ash::WallpaperLayout layout,
                                               const UserImage& user_image) {
  const SkBitmap& wallpaper = user_image.image();
  ash::Shell::GetInstance()->desktop_background_controller()->
      SetCustomWallpaper(wallpaper, layout);
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

void WallpaperManager::OnWallpaperLoaded(ash::WallpaperLayout layout,
                                         const UserImage& user_image) {
  const SkBitmap& wallpaper = user_image.image();
  SetWallpaperFromImageSkia(wallpaper, layout);
}

void WallpaperManager::SystemResumed() {
  BatchUpdateWallpaper();
}

void WallpaperManager::TimezoneChanged(const icu::TimeZone& timezone) {
  RestartTimer();
}

}  // chromeos
