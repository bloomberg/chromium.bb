// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wallpaper_manager.h"

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/skia_util.h"

using content::BrowserThread;

namespace {

const int kWallpaperUpdateIntervalSec = 24 * 60 * 60;

const char kWallpaperTypeNodeName[] = "type";
const char kWallpaperIndexNodeName[] = "index";
const char kWallpaperDateNodeName[] = "date";

const int kThumbnailWidth = 128;
const int kThumbnailHeight = 80;

// Default wallpaper index used in OOBE (first boot).
// Defined here because Chromium default index differs.
// Also see ash::WallpaperInfo kDefaultWallpapers in
// desktop_background_resources.cc
#if defined(GOOGLE_CHROME_BUILD)
const int kDefaultOOBEWallpaperIndex = 20; // IDR_AURA_WALLPAPERS_2_LANDSCAPE8
#else
const int kDefaultOOBEWallpaperIndex = 0;  // IDR_AURA_WALLPAPERS_ROMAINGUY_0
#endif

// Names of nodes with info about wallpaper.
const char kNewWallpaperDateNodeName[] = "date";
const char kNewWallpaperLayoutNodeName[] = "layout";
const char kNewWallpaperFileNodeName[] = "file";
const char kNewWallpaperTypeNodeName[] = "type";

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

WallpaperManager::WallpaperManager()
    : ALLOW_THIS_IN_INITIALIZER_LIST(wallpaper_loader_(new UserImageLoader)),
      current_user_wallpaper_type_(User::UNKNOWN),
      ALLOW_THIS_IN_INITIALIZER_LIST(current_user_wallpaper_index_(
          ash::GetInvalidWallpaperIndex())),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  RestartTimer();
  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                 content::NotificationService::AllSources());
}

// static
void WallpaperManager::RegisterPrefs(PrefService* local_state) {
  local_state->RegisterDictionaryPref(prefs::kUsersWallpaperInfo,
                                      PrefService::UNSYNCABLE_PREF);
}

void WallpaperManager::AddObservers() {
  if (!DBusThreadManager::Get()->GetPowerManagerClient()->HasObserver(this))
    DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
  system::TimezoneSettings::GetInstance()->AddObserver(this);
}

void WallpaperManager::CacheIfCustomWallpaper(const std::string& email) {
  User::WallpaperType type;
  int index;
  base::Time date;
  GetUserWallpaperProperties(email, &type, &index, &date);
  if (type == User::CUSTOMIZED) {
    std::string wallpaper_path = GetWallpaperPathForUser(email, false).value();

    // Uses WeakPtr here to make the request cancelable.
    wallpaper_loader_->Start(wallpaper_path, 0,
                             base::Bind(&WallpaperManager::CacheWallpaper,
                                        weak_factory_.GetWeakPtr(), email));
  }
}

void WallpaperManager::EnsureLoggedInUserWallpaperLoaded() {
  User::WallpaperType type;
  int index;
  base::Time last_modification_date;
  GetLoggedInUserWallpaperProperties(&type, &index, &last_modification_date);

  if (type != current_user_wallpaper_type_ ||
      index != current_user_wallpaper_index_) {
    SetUserWallpaper(UserManager::Get()->GetLoggedInUser().email());
  }
}

void WallpaperManager::FetchCustomWallpaper(const std::string& email) {
  User::WallpaperType type;
  int index;
  base::Time date;
  GetUserWallpaperProperties(email, &type, &index, &date);
  ash::WallpaperLayout layout = static_cast<ash::WallpaperLayout>(index);

  std::string wallpaper_path = GetWallpaperPathForUser(email, false).value();

  wallpaper_loader_->Start(wallpaper_path, 0,
                           base::Bind(&WallpaperManager::FetchWallpaper,
                                      base::Unretained(this), email, layout));
}

bool WallpaperManager::GetCustomWallpaperFromCache(const std::string& email,
                                                   gfx::ImageSkia* wallpaper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CustomWallpaperMap::const_iterator it = custom_wallpaper_cache_.find(email);
  if (it != custom_wallpaper_cache_.end()) {
    *wallpaper = (*it).second;
    return true;
  }
  return false;
}

FilePath WallpaperManager::GetWallpaperPathForUser(const std::string& username,
                                                   bool is_thumbnail) {
  const char* suffix = is_thumbnail ? "_thumb" : "";
  std::string filename = base::StringPrintf("%s_wallpaper%s.png",
                                            username.c_str(),
                                            suffix);
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  return user_data_dir.AppendASCII(filename);
}

gfx::ImageSkia WallpaperManager::GetCustomWallpaperThumbnail(
    const std::string& email) {
  CustomWallpaperMap::const_iterator it =
      custom_wallpaper_thumbnail_cache_.find(email);
  if (it != custom_wallpaper_cache_.end())
    return (*it).second;
  else
    return gfx::ImageSkia();
}

void WallpaperManager::GetLoggedInUserWallpaperProperties(
    User::WallpaperType* type,
    int* index,
    base::Time* last_modification_date) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (UserManager::Get()->IsLoggedInAsStub()) {
    *type = current_user_wallpaper_type_ = User::DEFAULT;
    *index = current_user_wallpaper_index_ = ash::GetInvalidWallpaperIndex();
    return;
  }

  GetUserWallpaperProperties(UserManager::Get()->GetLoggedInUser().email(),
                             type, index, last_modification_date);
}

void WallpaperManager::InitializeWallpaper() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UserManager* user_manager = UserManager::Get();

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType))
    WizardController::SetZeroDelays();

  // Zero delays is also set in autotests.
  if (WizardController::IsZeroDelayEnabled())
    return;

  bool disable_new_oobe = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kDisableNewOobe);
  bool disable_boot_animation = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kDisableBootAnimation);

  if (!user_manager->IsUserLoggedIn()) {
    if (!disable_new_oobe) {
      if (!WizardController::IsDeviceRegistered()) {
        ash::Shell::GetInstance()->desktop_background_controller()->
            SetDefaultWallpaper(kDefaultOOBEWallpaperIndex, false);
      } else {
        bool show_users = true;
        bool result = CrosSettings::Get()->GetBoolean(
            kAccountsPrefShowUserNamesOnSignIn, &show_users);
        DCHECK(result) << "Unable to fetch setting "
                       << kAccountsPrefShowUserNamesOnSignIn;
        if (!show_users) {
          // Boot into sign in form, preload default wallpaper.
          ash::Shell::GetInstance()->desktop_background_controller()->
              SetDefaultWallpaper(kDefaultOOBEWallpaperIndex, false);
        } else if (!disable_boot_animation) {
          // Normal boot, load user wallpaper.
          // If normal boot animation is disabled wallpaper would be set
          // asynchronously once user pods are loaded.
          const chromeos::UserList& users = user_manager->GetUsers();
          if (!users.empty()) {
            SetUserWallpaper(users[0]->email());
          } else {
            ash::Shell::GetInstance()->desktop_background_controller()->
                SetDefaultWallpaper(kDefaultOOBEWallpaperIndex, false);
          }
        }
      }
    }
    return;
  }
  SetUserWallpaper(user_manager->GetLoggedInUser().email());
}

void WallpaperManager::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (type == chrome::NOTIFICATION_LOGIN_USER_CHANGED) {
    // Cancel callback for previous cache requests.
    weak_factory_.InvalidateWeakPtrs();
    custom_wallpaper_cache_.clear();
  }
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

void WallpaperManager::SaveUserWallpaperProperties(const std::string& email,
                                                   User::WallpaperType type,
                                                   int index) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  current_user_wallpaper_type_ = type;
  current_user_wallpaper_index_ = index;
  // Ephemeral users can not save data to local state. We just cache the index
  // in memory for them.
  if (UserManager::Get()->IsCurrentUserEphemeral())
    return;

  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate wallpaper_update(local_state,
      UserManager::kUserWallpapersProperties);

  base::DictionaryValue* wallpaper_properties = new base::DictionaryValue();
  wallpaper_properties->Set(kWallpaperTypeNodeName,
                            new base::FundamentalValue(type));
  wallpaper_properties->Set(kWallpaperIndexNodeName,
                            new base::FundamentalValue(index));
  wallpaper_properties->SetString(kWallpaperDateNodeName,
      base::Int64ToString(base::Time::Now().LocalMidnight().ToInternalValue()));
  wallpaper_update->SetWithoutPathExpansion(email, wallpaper_properties);
}

void WallpaperManager::SetUserWallpaperFromFile(
    const std::string& username,
    const FilePath& path,
    ash::WallpaperLayout layout,
    base::WeakPtr<WallpaperDelegate> delegate) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // For wallpapers, save the image without resizing.
  wallpaper_loader_->Start(
      path.value(), 0,
      base::Bind(&WallpaperManager::SetWallpaper,
                 base::Unretained(this), username, layout, User::CUSTOMIZED,
                 delegate));
}

void WallpaperManager::SetInitialUserWallpaper(const std::string& username) {
  current_user_wallpaper_type_ = User::DEFAULT;
  if (username == kGuestUser)
    current_user_wallpaper_index_ = ash::GetGuestWallpaperIndex();
  else
    current_user_wallpaper_index_ = ash::GetDefaultWallpaperIndex();
  SaveUserWallpaperProperties(username,
                              current_user_wallpaper_type_,
                              current_user_wallpaper_index_);

  // Some browser tests do not have shell instance. And it is not necessary to
  // create a wallpaper for these tests. Add HasInstance check to prevent tests
  // crash and speed up the tests by avoid loading wallpaper.
  if (ash::Shell::HasInstance()) {
    ash::Shell::GetInstance()->desktop_background_controller()->
        SetDefaultWallpaper(current_user_wallpaper_index_, false);
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
  wallpaper_properties->SetString(kNewWallpaperDateNodeName,
      base::Int64ToString(base::Time::Now().LocalMidnight().ToInternalValue()));
  wallpaper_properties->SetString(kNewWallpaperFileNodeName, file_name);
  wallpaper_properties->SetInteger(kNewWallpaperLayoutNodeName, layout);
  wallpaper_properties->SetInteger(kNewWallpaperTypeNodeName, type);
  wallpaper_update->SetWithoutPathExpansion(username, wallpaper_properties);
}

void WallpaperManager::SetLastSelectedUser(
    const std::string& last_selected_user) {
  last_selected_user_ = last_selected_user;
}

void WallpaperManager::SetUserWallpaper(const std::string& email) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (email == kGuestUser) {
    ash::Shell::GetInstance()->desktop_background_controller()->
      SetDefaultWallpaper(ash::GetGuestWallpaperIndex(), false);
  }

  if (!UserManager::Get()->IsKnownUser(email))
    return;

  User::WallpaperType type;
  int index;
  base::Time date;
  GetUserWallpaperProperties(email, &type, &index, &date);
  if (type == User::DAILY && date != base::Time::Now().LocalMidnight()) {
    index = ash::GetNextWallpaperIndex(index);
    SaveUserWallpaperProperties(email, User::DAILY, index);
  } else if (type == User::CUSTOMIZED) {
    gfx::ImageSkia custom_wallpaper;
    if (GetCustomWallpaperFromCache(email, &custom_wallpaper)) {
      // In customized mode, we use index pref to save the user selected
      // wallpaper layout. See function SaveWallpaperToLocalState().
      ash::WallpaperLayout layout = static_cast<ash::WallpaperLayout>(index);
      ash::Shell::GetInstance()->desktop_background_controller()->
          SetCustomWallpaper(custom_wallpaper, layout);
    } else {
      FetchCustomWallpaper(email);
    }
    return;
  }
  ash::Shell::GetInstance()->desktop_background_controller()->
      SetDefaultWallpaper(index, false);
  SetLastSelectedUser(email);
}

void WallpaperManager::SetWallpaperFromImageSkia(
    const gfx::ImageSkia& wallpaper,
    ash::WallpaperLayout layout) {
  ash::Shell::GetInstance()->desktop_background_controller()->
      SetCustomWallpaper(wallpaper, layout);
}

void WallpaperManager::OnUserSelected(const std::string& email) {
  SetUserWallpaper(email);
}

// WallpaperManager, private: --------------------------------------------------

WallpaperManager::~WallpaperManager() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
  system::TimezoneSettings::GetInstance()->RemoveObserver(this);
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
      GetUserWallpaperProperties(email, &type, &index, &last_modification_date);
      base::Time current_date = base::Time::Now().LocalMidnight();
      if (type == User::DAILY && current_date != last_modification_date) {
        index = ash::GetNextWallpaperIndex(index);
        SaveUserWallpaperProperties(email, type, index);
      }
      // Force a wallpaper update for logged in / last selected user.
      // TODO(bshe): Notify lock screen, wallpaper picker UI to update wallpaper
      // as well.
      if (user_manager->IsUserLoggedIn() &&
          email == user_manager->GetLoggedInUser().email()) {
        SetUserWallpaper(email);
      } else if (show_users &&
                 email == last_selected_user_) {
        SetUserWallpaper(email);
      }
    }
  }
  RestartTimer();
}

void WallpaperManager::CacheWallpaper(const std::string& email,
                                      const UserImage& wallpaper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(custom_wallpaper_cache_.find(email) == custom_wallpaper_cache_.end());

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&WallpaperManager::CacheThumbnail,
                 base::Unretained(this), email, wallpaper.image()));

  custom_wallpaper_cache_.insert(std::make_pair(email, wallpaper.image()));
}

void WallpaperManager::CacheThumbnail(const std::string& email,
                                      const gfx::ImageSkia& wallpaper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  gfx::ImageSkia thumbnail =
      skia::ImageOperations::Resize(wallpaper,
                                    skia::ImageOperations::RESIZE_LANCZOS3,
                                    kThumbnailWidth, kThumbnailHeight);
  custom_wallpaper_thumbnail_cache_[email] = thumbnail;
}

void WallpaperManager::FetchWallpaper(const std::string& email,
                                      ash::WallpaperLayout layout,
                                      const UserImage& wallpaper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&WallpaperManager::CacheThumbnail,
                 base::Unretained(this), email, wallpaper.image()));

  ash::Shell::GetInstance()->desktop_background_controller()->
      SetCustomWallpaper(wallpaper.image(), layout);
}

void WallpaperManager::GetUserWallpaperProperties(const std::string& email,
    User::WallpaperType* type,
    int* index,
    base::Time* last_modification_date) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Default to the values cached in memory.
  *type = current_user_wallpaper_type_;
  *index = current_user_wallpaper_index_;

  // Override with values found in local store, if any.
  if (!email.empty()) {
    const DictionaryValue* user_wallpapers = g_browser_process->local_state()->
        GetDictionary(UserManager::kUserWallpapersProperties);
    const base::DictionaryValue* wallpaper_properties;
    if (user_wallpapers->GetDictionaryWithoutPathExpansion(
        email,
        &wallpaper_properties)) {
      *type = User::UNKNOWN;
      *index = ash::GetInvalidWallpaperIndex();
      wallpaper_properties->GetInteger(kWallpaperTypeNodeName,
                                       reinterpret_cast<int*>(type));
      wallpaper_properties->GetInteger(kWallpaperIndexNodeName, index);
      std::string date_string;
      int64 val;
      if (!(wallpaper_properties->GetString(kWallpaperDateNodeName,
                                            &date_string) &&
            base::StringToInt64(date_string, &val)))
        val = 0;
      *last_modification_date = base::Time::FromInternalValue(val);
    }
  }
}

void WallpaperManager::GenerateUserWallpaperThumbnail(
    const std::string& email,
    User::WallpaperType type,
    base::WeakPtr<WallpaperDelegate> delegate,
    const gfx::ImageSkia& wallpaper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  gfx::ImageSkia thumbnail =
      skia::ImageOperations::Resize(wallpaper,
                                    skia::ImageOperations::RESIZE_LANCZOS3,
                                    kThumbnailWidth, kThumbnailHeight);

  custom_wallpaper_thumbnail_cache_[email] = thumbnail;

  // Notify thumbnail is ready.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&WallpaperManager::OnThumbnailUpdated,
                 base::Unretained(this), delegate));
}

void WallpaperManager::OnThumbnailUpdated(
    base::WeakPtr<WallpaperDelegate> delegate) {
  if (delegate)
    delegate->SetCustomWallpaperThumbnail();
}

void WallpaperManager::SaveWallpaper(const std::string& path,
                                     const UserImage& wallpaper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::vector<unsigned char> image_data = wallpaper.raw_image();
  int write_bytes = file_util::WriteFile(FilePath(path),
      reinterpret_cast<char*>(&*image_data.begin()),
      image_data.size());
  DCHECK(write_bytes == static_cast<int>(image_data.size()));
}

void WallpaperManager::SetWallpaper(const std::string& username,
                                    ash::WallpaperLayout layout,
                                    User::WallpaperType type,
                                    base::WeakPtr<WallpaperDelegate> delegate,
                                    const UserImage& wallpaper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::string wallpaper_path =
      GetWallpaperPathForUser(username, false).value();

  // TODO(bshe): Ephemeral user should not save custom wallpaper to disk.
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&WallpaperManager::SaveWallpaper,
                 base::Unretained(this), wallpaper_path, wallpaper));

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&WallpaperManager::GenerateUserWallpaperThumbnail,
                 base::Unretained(this), username, type, delegate,
                 wallpaper.image()));

  ash::Shell::GetInstance()->desktop_background_controller()->
      SetCustomWallpaper(wallpaper.image(), layout);
  SaveUserWallpaperProperties(username, type, layout);
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
