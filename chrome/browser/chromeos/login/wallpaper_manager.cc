// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wallpaper_manager.h"

#include <vector>

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/desktop_background_resources.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/worker_pool.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/simple_jpeg_encoder.h"
#include "chrome/browser/chromeos/login/user.h"
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
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skia_util.h"

using content::BrowserThread;

namespace {

const int kWallpaperUpdateIntervalSec = 24 * 60 * 60;

// A dictionary pref that maps usernames to file paths to their wallpapers.
// Deprecated. Will remove this const char after done migration.
const char kUserWallpapers[] = "UserWallpapers";

// Names of nodes with info about wallpaper in |kUserWallpapers| dictionary.
const char kWallpaperTypeNodeName[] = "type";
const char kWallpaperIndexNodeName[] = "index";
const char kWallpaperDateNodeName[] = "date";

const int kThumbnailWidth = 128;
const int kThumbnailHeight = 80;

const int kCacheWallpaperDelayMs = 500;

// Default wallpaper index used in OOBE (first boot).
// Defined here because Chromium default index differs.
// Also see ash::WallpaperInfo kDefaultWallpapers in
// desktop_background_resources.cc
#if defined(GOOGLE_CHROME_BUILD)
const int kDefaultOOBEWallpaperIndex = 1; // IDR_AURA_WALLPAPERS_2_LANDSCAPE8
#else
const int kDefaultOOBEWallpaperIndex = 0;  // IDR_AURA_WALLPAPERS_5_GRADIENT5
#endif

// A dictionary pref that maps usernames to wallpaper properties.
const char kUserWallpapersProperties[] = "UserWallpapersProperties";

// Names of nodes with info about wallpaper in |kUserWallpapersProperties|
// dictionary.
const char kNewWallpaperDateNodeName[] = "date";
const char kNewWallpaperLayoutNodeName[] = "layout";
const char kNewWallpaperFileNodeName[] = "file";
const char kNewWallpaperTypeNodeName[] = "type";

// File path suffix of the original custom wallpaper.
const char kOriginalCustomWallpaperSuffix[] = "_wallpaper";

gfx::ImageSkia GetWallpaperThumbnail(const gfx::ImageSkia& wallpaper) {
  gfx::ImageSkia thumbnail = gfx::ImageSkiaOperations::CreateResizedImage(
      wallpaper,
      skia::ImageOperations::RESIZE_LANCZOS3,
      gfx::Size(kThumbnailWidth, kThumbnailHeight));

  thumbnail.MakeThreadSafe();
  return thumbnail;
}

// For our scaling ratios we need to round positive numbers.
int RoundPositive(double x) {
  return static_cast<int>(floor(x + 0.5));
}

}  // namespace

namespace chromeos {

const char kSmallWallpaperSuffix[] = "_small";
const char kLargeWallpaperSuffix[] = "_large";

static WallpaperManager* g_wallpaper_manager = NULL;

// WallpaperManager, public: ---------------------------------------------------

// static
WallpaperManager* WallpaperManager::Get() {
  if (!g_wallpaper_manager)
    g_wallpaper_manager = new WallpaperManager();
  return g_wallpaper_manager;
}

WallpaperManager::WallpaperManager()
    : loaded_wallpapers_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(current_default_wallpaper_index_(
          ash::GetInvalidWallpaperIndex())),
      ALLOW_THIS_IN_INITIALIZER_LIST(wallpaper_loader_(
          new UserImageLoader(ImageDecoder::ROBUST_JPEG_CODEC))),
      current_user_wallpaper_type_(User::UNKNOWN),
      ALLOW_THIS_IN_INITIALIZER_LIST(current_user_wallpaper_index_(
          ash::GetInvalidWallpaperIndex())),
      should_cache_wallpaper_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  RestartTimer();
  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_WEBUI_VISIBLE,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_WALLPAPER_ANIMATION_FINISHED,
                 content::NotificationService::AllSources());
}

// static
void WallpaperManager::RegisterPrefs(PrefService* local_state) {
  local_state->RegisterDictionaryPref(prefs::kUsersWallpaperInfo,
                                      PrefService::UNSYNCABLE_PREF);
  local_state->RegisterDictionaryPref(kUserWallpapers,
                                      PrefService::UNSYNCABLE_PREF);
  local_state->RegisterDictionaryPref(kUserWallpapersProperties,
                                      PrefService::UNSYNCABLE_PREF);
}

void WallpaperManager::AddObservers() {
  if (!DBusThreadManager::Get()->GetPowerManagerClient()->HasObserver(this))
    DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
  system::TimezoneSettings::GetInstance()->AddObserver(this);
}

void WallpaperManager::EnsureLoggedInUserWallpaperLoaded() {
  WallpaperInfo info;
  if (GetLoggedInUserWallpaperInfo(&info)) {
    // TODO(sschmitz): We need an index for default wallpapers for the new UI.
    RecordUma(info.type, -1);
    if (info == current_user_wallpaper_info_)
      return;
  }
  SetUserWallpaper(UserManager::Get()->GetLoggedInUser()->email());
}

void WallpaperManager::ClearWallpaperCache() {
  // Cancel callback for previous cache requests.
  weak_factory_.InvalidateWeakPtrs();
  wallpaper_cache_.clear();
}

bool WallpaperManager::GetWallpaperFromCache(const std::string& email,
                                             gfx::ImageSkia* wallpaper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CustomWallpaperMap::const_iterator it = wallpaper_cache_.find(email);
  if (it != wallpaper_cache_.end()) {
    *wallpaper = (*it).second;
    return true;
  }
  return false;
}

FilePath WallpaperManager::GetOriginalWallpaperPathForUser(
    const std::string& username) {
  std::string filename = username + kOriginalCustomWallpaperSuffix;
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  return user_data_dir.AppendASCII(filename);
}

FilePath WallpaperManager::GetWallpaperPathForUser(const std::string& username,
                                                   bool is_small) {
  const char* suffix = is_small ?
      kSmallWallpaperSuffix : kLargeWallpaperSuffix;

  std::string filename = base::StringPrintf("%s_wallpaper%s",
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
  if (it != custom_wallpaper_thumbnail_cache_.end())
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

  GetUserWallpaperProperties(UserManager::Get()->GetLoggedInUser()->email(),
                             type, index, last_modification_date);
}

bool WallpaperManager::GetLoggedInUserWallpaperInfo(WallpaperInfo* info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (UserManager::Get()->IsLoggedInAsStub()) {
    info->file = current_user_wallpaper_info_.file = "";
    info->layout = current_user_wallpaper_info_.layout = ash::CENTER_CROPPED;
    info->type = current_user_wallpaper_info_.type = User::DEFAULT;
    return true;
  }

  return GetUserWallpaperInfo(UserManager::Get()->GetLoggedInUser()->email(),
                              info);
}

void WallpaperManager::InitializeWallpaper() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UserManager* user_manager = UserManager::Get();

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType))
    WizardController::SetZeroDelays();

  // Zero delays is also set in autotests.
  if (WizardController::IsZeroDelayEnabled()) {
    // Ensure tests have some sort of wallpaper.
    ash::Shell::GetInstance()->desktop_background_controller()->
        CreateEmptyWallpaper();
    return;
  }

  bool disable_new_oobe = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kDisableNewOobe);
  bool disable_boot_animation = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kDisableBootAnimation);

  if (!user_manager->IsUserLoggedIn()) {
    if (!disable_new_oobe) {
      if (!WizardController::IsDeviceRegistered()) {
        SetDefaultWallpaper(kDefaultOOBEWallpaperIndex);
      } else {
        bool show_users = true;
        bool result = CrosSettings::Get()->GetBoolean(
            kAccountsPrefShowUserNamesOnSignIn, &show_users);
        DCHECK(result) << "Unable to fetch setting "
                       << kAccountsPrefShowUserNamesOnSignIn;
        const chromeos::UserList& users = user_manager->GetUsers();
        if (!show_users || users.empty()) {
          // Boot into sign in form, preload default wallpaper.
          SetDefaultWallpaper(kDefaultOOBEWallpaperIndex);
          return;
        }

        if (!disable_boot_animation) {
          // Normal boot, load user wallpaper.
          // If normal boot animation is disabled wallpaper would be set
          // asynchronously once user pods are loaded.
          SetUserWallpaper(users[0]->email());
        }
      }
    }
    return;
  }
  SetUserWallpaper(user_manager->GetLoggedInUser()->email());
}

void WallpaperManager::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_USER_CHANGED: {
      ClearWallpaperCache();
      break;
    }
    case chrome::NOTIFICATION_LOGIN_WEBUI_VISIBLE: {
      if (!CommandLine::ForCurrentProcess()->
          HasSwitch(switches::kDisableBootAnimation)) {
        BrowserThread::PostDelayedTask(
            BrowserThread::UI, FROM_HERE,
            base::Bind(&WallpaperManager::CacheAllUsersWallpapers,
                       weak_factory_.GetWeakPtr()),
            base::TimeDelta::FromMilliseconds(kCacheWallpaperDelayMs));
      } else {
        should_cache_wallpaper_ = true;
      }
      break;
    }
    case chrome::NOTIFICATION_WALLPAPER_ANIMATION_FINISHED: {
      if (should_cache_wallpaper_) {
        BrowserThread::PostDelayedTask(
            BrowserThread::UI, FROM_HERE,
            base::Bind(&WallpaperManager::CacheAllUsersWallpapers,
                       weak_factory_.GetWeakPtr()),
            base::TimeDelta::FromMilliseconds(kCacheWallpaperDelayMs));
        should_cache_wallpaper_ = false;
      }
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification " << type;
  }
}

void WallpaperManager::RemoveUserWallpaperInfo(const std::string& email) {
  PrefService* prefs = g_browser_process->local_state();
  DictionaryPrefUpdate prefs_wallpapers_update(prefs,
      kUserWallpapersProperties);
  prefs_wallpapers_update->RemoveWithoutPathExpansion(email, NULL);

  DictionaryPrefUpdate prefs_wallpapers_info_update(prefs,
      prefs::kUsersWallpaperInfo);
  prefs_wallpapers_info_update->RemoveWithoutPathExpansion(email, NULL);

  DeleteUserWallpapers(email);
}

void WallpaperManager::ResizeAndSaveWallpaper(const UserImage& wallpaper,
                                              const FilePath& path,
                                              ash::WallpaperLayout layout,
                                              int preferred_width,
                                              int preferred_height) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  int width = wallpaper.image().width();
  int height = wallpaper.image().height();
  int resized_width;
  int resized_height;

  if (layout == ash::CENTER_CROPPED) {
    // Do not resize custom wallpaper if it is smaller than preferred size.
    if (!(width > preferred_width && height > preferred_height))
      return;

    double horizontal_ratio = static_cast<double>(preferred_width) / width;
    double vertical_ratio = static_cast<double>(preferred_height) / height;
    if (vertical_ratio > horizontal_ratio) {
      resized_width =
          RoundPositive(static_cast<double>(width) * vertical_ratio);
      resized_height = preferred_height;
    } else {
      resized_width = preferred_width;
      resized_height =
          RoundPositive(static_cast<double>(height) * horizontal_ratio);
    }
  } else if (layout == ash::STRETCH) {
    resized_width = preferred_width;
    resized_height = preferred_height;
  } else {
    // TODO(bshe): Generates cropped custom wallpaper for CENTER layout.
    if (file_util::PathExists(path))
      file_util::Delete(path, false);
    return;
  }

  gfx::ImageSkia resized_image = gfx::ImageSkiaOperations::CreateResizedImage(
      wallpaper.image(),
      skia::ImageOperations::RESIZE_LANCZOS3,
      gfx::Size(resized_width, resized_height));

  scoped_refptr<base::RefCountedBytes> data = new base::RefCountedBytes();
  // Uses simple JPG encoder to encode image on worker pool so we do not block
  // chrome shutdown on image encoding.
  SimpleJpegEncoder* jpeg_encoder = new SimpleJpegEncoder(
      data, *(resized_image.bitmap()));
  jpeg_encoder->Run(
      base::Bind(&WallpaperManager::OnWallpaperEncoded,
                 weak_factory_.GetWeakPtr(),
                 path));
}

void WallpaperManager::RestartTimer() {
  timer_.Stop();
  // Determine the next update time as the earliest local midnight after now.
  // Note that this may be more than kWallpaperUpdateIntervalSec seconds in the
  // future due to DST.
  base::Time now = base::Time::Now();
  base::TimeDelta update_interval = base::TimeDelta::FromSeconds(
      kWallpaperUpdateIntervalSec);
  base::Time future = now + update_interval;
  base::Time next_update = future.LocalMidnight();
  while (next_update < now) {
    future += update_interval;
    next_update = future.LocalMidnight();
  }
  int64 remaining_seconds = (next_update - now).InSeconds();
  DCHECK(remaining_seconds > 0);
  // Set up a one shot timer which will batch update wallpaper at midnight.
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromSeconds(remaining_seconds),
               this,
               &WallpaperManager::BatchUpdateWallpaper);
}

void WallpaperManager::SetCustomWallpaper(const std::string& username,
    ash::WallpaperLayout layout,
    User::WallpaperType type,
    base::WeakPtr<WallpaperDelegate> delegate,
    const UserImage& wallpaper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::string wallpaper_path =
      GetOriginalWallpaperPathForUser(username).value();

  // If decoded wallpaper is empty, we are probably failed to decode the file.
  // Use default wallpaper in this case.
  if (wallpaper.image().isNull()) {
    SetDefaultWallpaper(ash::GetDefaultWallpaperIndex());
    return;
  }

  bool is_persistent = ShouldPersistDataForUser(username);

  wallpaper.image().EnsureRepsForSupportedScaleFactors();
  scoped_ptr<gfx::ImageSkia> deep_copy(wallpaper.image().DeepCopy());

  WallpaperInfo wallpaper_info = {
      wallpaper_path,
      layout,
      type,
      // Date field is not used.
      base::Time::Now().LocalMidnight()
  };
  // TODO(bshe): This may break if RawImage becomes RefCountedMemory.
  BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&WallpaperManager::ProcessCustomWallpaper,
                   base::Unretained(this),
                   username,
                   is_persistent,
                   wallpaper_info,
                   delegate,
                   base::Passed(&deep_copy),
                   wallpaper.raw_image()));
  ash::Shell::GetInstance()->desktop_background_controller()->
      SetCustomWallpaper(wallpaper.image(), layout);

  // User's custom wallpaper path is determined by username/email and the
  // appropriate wallpaper resolution in GetCustomWallpaperInternal. So use
  // DUMMY as file name here.
  WallpaperInfo info = {
      "DUMMY",
      layout,
      User::CUSTOMIZED,
      base::Time::Now().LocalMidnight()
  };
  SetUserWallpaperInfo(username, info, is_persistent);

  SetUserWallpaperProperties(username, type, layout, is_persistent);
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
      base::Bind(&WallpaperManager::SetCustomWallpaper,
                 base::Unretained(this), username, layout, User::CUSTOMIZED,
                 delegate));
}

void WallpaperManager::SetUserWallpaperProperties(const std::string& email,
                                                  User::WallpaperType type,
                                                  int index,
                                                  bool is_persistent) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  current_user_wallpaper_type_ = type;
  current_user_wallpaper_index_ = index;
  if (!is_persistent)
    return;

  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate wallpaper_update(local_state, kUserWallpapersProperties);

  base::DictionaryValue* wallpaper_properties = new base::DictionaryValue();
  wallpaper_properties->Set(kWallpaperTypeNodeName,
                            new base::FundamentalValue(type));
  wallpaper_properties->Set(kWallpaperIndexNodeName,
                            new base::FundamentalValue(index));
  wallpaper_properties->SetString(kWallpaperDateNodeName,
      base::Int64ToString(base::Time::Now().LocalMidnight().ToInternalValue()));
  wallpaper_update->SetWithoutPathExpansion(email, wallpaper_properties);
}

void WallpaperManager::SetDefaultWallpaper(int index) {
  // Prevents loading of the same wallpaper as the currently loading/loaded one.
  if (current_default_wallpaper_index_ == index)
    return;
  current_default_wallpaper_index_ = index;
  current_wallpaper_path_ = FilePath("");
  loaded_wallpapers_++;
  ash::Shell::GetInstance()->desktop_background_controller()->
      SetDefaultWallpaper(index);
}

void WallpaperManager::SetInitialUserWallpaper(const std::string& username,
                                               bool is_persistent) {
  current_user_wallpaper_type_ = User::DEFAULT;
  if (username == kGuestUser)
    current_user_wallpaper_index_ = ash::GetGuestWallpaperIndex();
  else
    current_user_wallpaper_index_ = ash::GetDefaultWallpaperIndex();

  current_user_wallpaper_info_.file = "";
  current_user_wallpaper_info_.layout = ash::CENTER_CROPPED;
  current_user_wallpaper_info_.type = User::DEFAULT;
  current_user_wallpaper_info_.date = base::Time::Now().LocalMidnight();

  WallpaperInfo info = current_user_wallpaper_info_;
  SetUserWallpaperInfo(username, info, is_persistent);

  // Some browser tests do not have shell instance. And it is not necessary to
  // create a wallpaper for these tests. Add HasInstance check to prevent tests
  // crash and speed up the tests by avoid loading wallpaper.
  if (ash::Shell::HasInstance())
    SetDefaultWallpaper(current_user_wallpaper_index_);
}

void WallpaperManager::SetUserWallpaperInfo(const std::string& username,
                                            const WallpaperInfo& info,
                                            bool is_persistent) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  current_user_wallpaper_info_ = info;
  if (!is_persistent)
    return;

  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate wallpaper_update(local_state,
                                        prefs::kUsersWallpaperInfo);

  base::DictionaryValue* wallpaper_info_dict = new base::DictionaryValue();
  wallpaper_info_dict->SetString(kNewWallpaperDateNodeName,
      base::Int64ToString(info.date.ToInternalValue()));
  wallpaper_info_dict->SetString(kNewWallpaperFileNodeName, info.file);
  wallpaper_info_dict->SetInteger(kNewWallpaperLayoutNodeName, info.layout);
  wallpaper_info_dict->SetInteger(kNewWallpaperTypeNodeName, info.type);
  wallpaper_update->SetWithoutPathExpansion(username, wallpaper_info_dict);
}

void WallpaperManager::SetLastSelectedUser(
    const std::string& last_selected_user) {
  last_selected_user_ = last_selected_user;
}

void WallpaperManager::SetUserWallpaper(const std::string& email) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (email == kGuestUser) {
    SetDefaultWallpaper(ash::GetGuestWallpaperIndex());
    return;
  }

  if (!UserManager::Get()->IsKnownUser(email))
    return;

  SetLastSelectedUser(email);

  WallpaperInfo info;

  if (GetUserWallpaperInfo(email, &info)) {
    gfx::ImageSkia user_wallpaper;
    if (GetWallpaperFromCache(email, &user_wallpaper)) {
      ash::Shell::GetInstance()->desktop_background_controller()->
          SetCustomWallpaper(user_wallpaper, info.layout);
    } else {
      if (info.type == User::CUSTOMIZED) {
        ash::WallpaperResolution resolution = ash::Shell::GetInstance()->
            desktop_background_controller()->GetAppropriateResolution();
        bool is_small  = (resolution == ash::SMALL);
        FilePath wallpaper_path = GetWallpaperPathForUser(email, is_small);
        if (current_wallpaper_path_ == wallpaper_path)
          return;
        current_wallpaper_path_ = wallpaper_path;
        current_default_wallpaper_index_ = ash::GetInvalidWallpaperIndex();
        loaded_wallpapers_++;

        BrowserThread::PostTask(
            BrowserThread::FILE, FROM_HERE,
            base::Bind(&WallpaperManager::GetCustomWallpaperInternal,
                       base::Unretained(this), email, info, wallpaper_path,
                       true /* update wallpaper */));
        return;
      }

      if (info.file.empty()) {
        // Uses default built-in wallpaper when file is empty. Eventually, we
        // will only ship one built-in wallpaper in ChromeOS image.
        SetDefaultWallpaper(ash::GetDefaultWallpaperIndex());
        return;
      }

      // Load downloaded ONLINE or converted DEFAULT wallpapers.
      LoadWallpaper(email, info, true /* update wallpaper */);
    }
  } else {
    SetInitialUserWallpaper(email, true);
  }
}

void WallpaperManager::SetSigninWallpaper() {
  SetDefaultWallpaper(kDefaultOOBEWallpaperIndex);
}

void WallpaperManager::SetWallpaperFromImageSkia(
    const gfx::ImageSkia& wallpaper,
    ash::WallpaperLayout layout) {
  ash::Shell::GetInstance()->desktop_background_controller()->
      SetCustomWallpaper(wallpaper, layout);
}

void WallpaperManager::UpdateWallpaper() {
  ClearWallpaperCache();
  SetUserWallpaper(last_selected_user_);
}

// WallpaperManager, private: --------------------------------------------------

WallpaperManager::~WallpaperManager() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
  system::TimezoneSettings::GetInstance()->RemoveObserver(this);
  weak_factory_.InvalidateWeakPtrs();
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
        SetUserWallpaperProperties(email, type, index, true);
      }
      // Force a wallpaper update for logged in / last selected user.
      // TODO(bshe): Notify lock screen, wallpaper picker UI to update wallpaper
      // as well.
      if (user_manager->IsUserLoggedIn() &&
          email == user_manager->GetLoggedInUser()->email()) {
        SetUserWallpaper(email);
      } else if (show_users &&
                 email == last_selected_user_) {
        SetUserWallpaper(email);
      }
    }
  }
  RestartTimer();
}

void WallpaperManager::CacheAllUsersWallpapers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UserList users = UserManager::Get()->GetUsers();

  if (!users.empty()) {
    UserList::const_iterator it = users.begin();
    // Skip the wallpaper of first user in the list. It should have been cached.
    it++;
    for (; it != users.end(); ++it) {
      std::string user_email = (*it)->email();
      CacheUserWallpaper(user_email);
    }
  }
}

void WallpaperManager::CacheUserWallpaper(const std::string& email) {
  if (wallpaper_cache_.find(email) == wallpaper_cache_.end())
    return;
  WallpaperInfo info;
  if (GetUserWallpaperInfo(email, &info)) {
    FilePath wallpaper_dir;
    FilePath wallpaper_path;
    if (info.type == User::CUSTOMIZED) {
      ash::WallpaperResolution resolution = ash::Shell::GetInstance()->
          desktop_background_controller()->GetAppropriateResolution();
      bool is_small  = (resolution == ash::SMALL);
      FilePath wallpaper_path = GetWallpaperPathForUser(email, is_small);
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(&WallpaperManager::GetCustomWallpaperInternal,
                     base::Unretained(this), email, info, wallpaper_path,
                     false /* do not update wallpaper */));
      return;
    }
    LoadWallpaper(email, info, false /* do not update wallpaper */);
  }
}

void WallpaperManager::CacheThumbnail(const std::string& email,
                                      scoped_ptr<gfx::ImageSkia> wallpaper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  custom_wallpaper_thumbnail_cache_[email] =
      GetWallpaperThumbnail(*wallpaper.get());
}

void WallpaperManager::DeleteWallpaperInList(
    const std::vector<FilePath>& file_list) {
  for (std::vector<FilePath>::const_iterator it = file_list.begin();
       it != file_list.end(); ++it) {
    FilePath path = *it;
    if (!file_util::Delete(path, false)) {
      LOG(ERROR) << "Failed to remove user wallpaper.";
    }

    // Some users may still have wallpapers with a file extension. This will
    // delete those legacy wallpapers.
    file_util::Delete(path.AddExtension(".png"), false);
  }
}

void WallpaperManager::DeleteUserWallpapers(const std::string& email) {
  std::vector<FilePath> file_to_remove;
  // Remove small user wallpaper.
  FilePath wallpaper_path = GetWallpaperPathForUser(email, true);
  file_to_remove.push_back(wallpaper_path);

  // Remove large user wallpaper.
  wallpaper_path = GetWallpaperPathForUser(email, false);
  file_to_remove.push_back(wallpaper_path);

  // Remove original user wallpaper.
  wallpaper_path = GetOriginalWallpaperPathForUser(email);
  file_to_remove.push_back(wallpaper_path);

  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&WallpaperManager::DeleteWallpaperInList,
                 base::Unretained(this),
                 file_to_remove),
      false);
}

void WallpaperManager::LoadWallpaper(const std::string& email,
                                     const WallpaperInfo& info,
                                     bool update_wallpaper) {
  FilePath wallpaper_dir;
  FilePath wallpaper_path;
  if (info.type == User::ONLINE) {
    std::string file_name = GURL(info.file).ExtractFileName();
    ash::WallpaperResolution resolution = ash::Shell::GetInstance()->
        desktop_background_controller()->GetAppropriateResolution();
    // Only solid color wallpapers have stretch layout and they have only one
    // resolution.
    if (info.layout != ash::STRETCH && resolution == ash::SMALL) {
      file_name = FilePath(file_name).InsertBeforeExtension(
          kSmallWallpaperSuffix).value();
    }
    CHECK(PathService::Get(chrome::DIR_CHROMEOS_WALLPAPERS, &wallpaper_dir));
    wallpaper_path = wallpaper_dir.Append(file_name);
    if (current_wallpaper_path_ == wallpaper_path)
      return;
    if (update_wallpaper) {
      current_wallpaper_path_ = wallpaper_path;
      current_default_wallpaper_index_ = ash::GetInvalidWallpaperIndex();
    }
    loaded_wallpapers_++;
    StartLoad(email, info, update_wallpaper, wallpaper_path);
  } else {
    // For custom wallpapers, we have increment |loaded_wallpapers_| in
    // SetUserWallpaper(). We should not increment it again here.
    FilePath user_data_dir;
    PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    wallpaper_path = user_data_dir.Append(info.file);
    base::WorkerPool::PostTask(
        FROM_HERE,
        base::Bind(&WallpaperManager::ValidateAndLoadWallpaper,
                   base::Unretained(this),
                   email,
                   info,
                   update_wallpaper,
                   wallpaper_path),
        false);
  }
}

bool WallpaperManager::GetUserWallpaperInfo(const std::string& email,
                                            WallpaperInfo* info){
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!ShouldPersistDataForUser(email)) {
    // Default to the values cached in memory.
    *info = current_user_wallpaper_info_;

    // Ephemeral users do not save anything to local state. But we have got
    // wallpaper info from memory. Returns true.
    return true;
  }

  const DictionaryValue* user_wallpapers = g_browser_process->local_state()->
      GetDictionary(prefs::kUsersWallpaperInfo);
  const base::DictionaryValue* wallpaper_info_dict;
  if (user_wallpapers->GetDictionaryWithoutPathExpansion(
          email, &wallpaper_info_dict)) {
    info->file = "";
    info->layout = ash::CENTER_CROPPED;
    info->type = User::UNKNOWN;
    info->date = base::Time::Now().LocalMidnight();
    wallpaper_info_dict->GetString(kNewWallpaperFileNodeName, &(info->file));
    int temp;
    wallpaper_info_dict->GetInteger(kNewWallpaperLayoutNodeName, &temp);
    info->layout = static_cast<ash::WallpaperLayout>(temp);
    wallpaper_info_dict->GetInteger(kNewWallpaperTypeNodeName, &temp);
    info->type = static_cast<User::WallpaperType>(temp);
    std::string date_string;
    int64 val;
    if (!(wallpaper_info_dict->GetString(kNewWallpaperDateNodeName,
                                         &date_string) &&
          base::StringToInt64(date_string, &val)))
      val = 0;
    info->date = base::Time::FromInternalValue(val);
    return true;
  }

  return false;
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
        GetDictionary(kUserWallpapersProperties);
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

  // Index maybe invalid when update from official build to unofficial build.
  // Revert to default in that case.
  if (*type == User::DEFAULT &&
      (*index < 0 || *index >= ash::GetWallpaperCount())) {
    *index = ash::GetDefaultWallpaperIndex();
  }
}

void WallpaperManager::GenerateUserWallpaperThumbnail(
    const std::string& email,
    User::WallpaperType type,
    base::WeakPtr<WallpaperDelegate> delegate,
    const gfx::ImageSkia& wallpaper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  custom_wallpaper_thumbnail_cache_[email] = GetWallpaperThumbnail(wallpaper);

  // Notify thumbnail is ready.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&WallpaperManager::OnThumbnailUpdated,
                 base::Unretained(this), delegate));
}

void WallpaperManager::GetCustomWallpaperInternal(
    const std::string& email,
    const WallpaperInfo& info,
    const FilePath& wallpaper_path,
    bool update_wallpaper) {
  std::string file_name = wallpaper_path.BaseName().value();

  if (!file_util::PathExists(wallpaper_path)) {
    if (file_util::PathExists(wallpaper_path.AddExtension(".png"))) {
      // Old wallpaper may have a png extension.
      file_name += ".png";
    } else {
      // Falls back on original file if the correct resoltuion file does not
      // exist. This may happen when the original custom wallpaper is small or
      // browser shutdown before resized wallpaper saved.
      file_name = GetOriginalWallpaperPathForUser(email).BaseName().value();
    }
  }

  WallpaperInfo new_info = {
      file_name,
      info.layout,
      info.type,
      info.date
  };

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WallpaperManager::LoadWallpaper,
                 base::Unretained(this), email, new_info, update_wallpaper));
}

void WallpaperManager::MigrateWallpaperData(const UserList& users) {
  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    const DictionaryValue* user_wallpapers =
          local_state->GetDictionary(kUserWallpapers);
    int index;
    const DictionaryValue* new_user_wallpapers =
        local_state->GetDictionary(kUserWallpapersProperties);
    if (new_user_wallpapers->empty()) {
      for (UserList::const_iterator it = users.begin();
           it != users.end();
           ++it) {
        std::string username = (*it)->email();
        if (user_wallpapers->GetIntegerWithoutPathExpansion(
                username, &index)) {
          DictionaryPrefUpdate prefs_wallpapers_update(
              local_state, kUserWallpapers);
          prefs_wallpapers_update->RemoveWithoutPathExpansion(username, NULL);
          SetUserWallpaperProperties(username, User::DEFAULT, index, true);
        } else {
          // Before M20, wallpaper index is not saved into LocalState unless
          // user specifically sets a wallpaper. After M20, the default
          // wallpaper index is saved to LocalState as soon as a new user login.
          // When migrating wallpaper index from M20 to M21, we only migrate
          // data that is in LocalState. This cause a problem when users login
          // on a M20 device and then update the device to M21. The default
          // wallpaper index failed to migrate because it was not saved into
          // LocalState. Then we assume that all users have index saved in
          // LocalState in M21. This is not true and it results an empty
          // wallpaper for those users as described in cr/130685. So here we use
          // default wallpaper for users that exist in user list but does not
          // have an index saved in LocalState.
          SetUserWallpaperProperties(
              username, User::DEFAULT, ash::GetDefaultWallpaperIndex(), true);
        }
      }
    }
  }
}

void WallpaperManager::MigrateBuiltInWallpaper(const std::string& email) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!email.empty() && UserManager::Get()->IsKnownUser(email)) {
    PrefService* prefs = g_browser_process->local_state();
    const DictionaryValue* user_wallpapers = prefs->
        GetDictionary(kUserWallpapersProperties);
    const base::DictionaryValue* wallpaper_properties;
    User::WallpaperType type;
    int index;
    ash::WallpaperResolution resolution = ash::Shell::GetInstance()->
        desktop_background_controller()->GetAppropriateResolution();
    if (user_wallpapers->GetDictionaryWithoutPathExpansion(email,
            &wallpaper_properties)) {
      base::Time date;
      GetUserWallpaperProperties(email, &type, &index, &date);

      FilePath wallpaper_dir;
      CHECK(PathService::Get(chrome::DIR_CHROMEOS_WALLPAPERS, &wallpaper_dir));
      DCHECK(type != User::UNKNOWN);

      FilePath wallpaper_path;
      if (type != User::CUSTOMIZED) {
        base::RefCountedStaticMemory* wallpaper =
            ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
                GetWallpaperViewInfo(index, ash::LARGE).id);

        // Saves large wallpaper to user custom wallpaper path.
        wallpaper_path = GetWallpaperPathForUser(email, false);
        BrowserThread::PostTask(
            BrowserThread::FILE,
            FROM_HERE,
            base::Bind(&WallpaperManager::SaveWallpaperInternal,
                   base::Unretained(this), wallpaper_path,
                   reinterpret_cast<const char*>(wallpaper->front()),
                   wallpaper->size()));

        wallpaper =
            ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
                GetWallpaperViewInfo(index, ash::SMALL).id);

        // Saves small wallpaper to user custom wallpaper thumbnail path.
        wallpaper_path = GetWallpaperPathForUser(email, true);
        BrowserThread::PostTask(
            BrowserThread::FILE,
            FROM_HERE,
            base::Bind(&WallpaperManager::SaveWallpaperInternal,
                   base::Unretained(this), wallpaper_path,
                   reinterpret_cast<const char*>(wallpaper->front()),
                   wallpaper->size()));
      }

      bool small_resolution = (resolution == ash::SMALL);
      std::string file_name =
          GetWallpaperPathForUser(email, small_resolution).BaseName().value();
      WallpaperInfo info = {
                               file_name,
                               GetWallpaperViewInfo(index, resolution).layout,
                               type,
                               base::Time::Now().LocalMidnight()
                           };
      SetUserWallpaperInfo(email, info, true);
    }
  }
}

void WallpaperManager::OnThumbnailUpdated(
    base::WeakPtr<WallpaperDelegate> delegate) {
  if (delegate)
    delegate->SetCustomWallpaperThumbnail();
}

void WallpaperManager::OnWallpaperDecoded(const std::string& email,
                                          ash::WallpaperLayout layout,
                                          bool update_wallpaper,
                                          const UserImage& wallpaper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If decoded wallpaper is empty, we are probably failed to decode the file.
  // Use default wallpaper in this case.
  if (wallpaper.image().isNull()) {
    // Updates user pref to default wallpaper.
    WallpaperInfo info = {
                           "",
                           ash::CENTER_CROPPED,
                           User::DEFAULT,
                           base::Time::Now().LocalMidnight()
                         };
    SetUserWallpaperInfo(email, info, true);

    if (update_wallpaper) {
      SetDefaultWallpaper(ash::GetDefaultWallpaperIndex());
    } else {
      ash::Shell::GetInstance()->desktop_background_controller()->
          CacheDefaultWallpaper(ash::GetDefaultWallpaperIndex());
    }
    return;
  }
  // Generate all reps before passing to another thread.
  wallpaper.image().EnsureRepsForSupportedScaleFactors();
  scoped_ptr<gfx::ImageSkia> deep_copy(wallpaper.image().DeepCopy());

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&WallpaperManager::CacheThumbnail,
                 base::Unretained(this), email,
                 base::Passed(&deep_copy)));
  // Only cache user wallpaper at login screen.
  if (!UserManager::Get()->IsUserLoggedIn()) {
    wallpaper_cache_.insert(std::make_pair(email, wallpaper.image()));
  }
  if (update_wallpaper) {
    ash::Shell::GetInstance()->desktop_background_controller()->
        SetCustomWallpaper(wallpaper.image(), layout);
  }
}

void WallpaperManager::ProcessCustomWallpaper(
    const std::string& email,
    bool persistent,
    const WallpaperInfo& info,
    base::WeakPtr<WallpaperDelegate> delegate,
    scoped_ptr<gfx::ImageSkia> image,
    const UserImage::RawImage& raw_image) {
  UserImage wallpaper(*image.get(), raw_image);
  GenerateUserWallpaperThumbnail(email, info.type, delegate, wallpaper.image());
  if (persistent)
    SaveCustomWallpaper(email, FilePath(info.file), info.layout, wallpaper);
}

void WallpaperManager::OnWallpaperEncoded(const FilePath& path,
    scoped_refptr<base::RefCountedBytes> data) {
  SaveWallpaperInternal(path,
                        reinterpret_cast<const char*>(data->front()),
                        data->size());
}

void WallpaperManager::SaveCustomWallpaper(const std::string& email,
                                           const FilePath& path,
                                           ash::WallpaperLayout layout,
                                           const UserImage& wallpaper) {
  FilePath small_wallpaper_path = GetWallpaperPathForUser(email, true);
  // Delete previous saved wallpapers.
  if (file_util::PathExists(small_wallpaper_path))
    file_util::Delete(small_wallpaper_path, false);
  FilePath large_wallpaper_path = GetWallpaperPathForUser(email, false);
  if (file_util::PathExists(large_wallpaper_path))
    file_util::Delete(large_wallpaper_path, false);

  std::vector<unsigned char> image_data = wallpaper.raw_image();
  // Saves the original file in case that resized wallpaper is not generated
  // (i.e. chrome shutdown before resized wallpaper is saved).
  SaveWallpaperInternal(path, reinterpret_cast<char*>(&*image_data.begin()),
                        image_data.size());

  ResizeAndSaveWallpaper(wallpaper, small_wallpaper_path, layout,
                         ash::kSmallWallpaperMaxWidth,
                         ash::kSmallWallpaperMaxHeight);
  ResizeAndSaveWallpaper(wallpaper, large_wallpaper_path, layout,
                         ash::kLargeWallpaperMaxWidth,
                         ash::kLargeWallpaperMaxHeight);
}

void WallpaperManager::RecordUma(User::WallpaperType type, int index) {
  UMA_HISTOGRAM_ENUMERATION("Ash.Wallpaper.Type", type,
                            User::WALLPAPER_TYPE_COUNT);
  if (type == User::DEFAULT) {
    if (index >= 0) {
      // TODO(sschmitz): Remove "if" when the index for new UI is available.
      UMA_HISTOGRAM_ENUMERATION("Ash.Wallpaper.DefaultIndex", index,
                                ash::GetWallpaperCount());
    }
  }
}

void WallpaperManager::SaveWallpaperInternal(const FilePath& path,
                                             const char* data,
                                             int size) {
  int written_bytes = file_util::WriteFile(path, data, size);
  DCHECK(written_bytes == size);
}

bool WallpaperManager::ShouldPersistDataForUser(const std::string& email) {
  UserManager* user_manager = UserManager::Get();
  // |email| is from user list in local state. We should persist data in this
  // case.
  if (!user_manager->IsUserLoggedIn())
    return true;
  return !(email == user_manager->GetLoggedInUser()->email() &&
           user_manager->IsCurrentUserEphemeral());
}

void WallpaperManager::StartLoad(const std::string& email,
                                 const WallpaperInfo& info,
                                 bool update_wallpaper,
                                 const FilePath& wallpaper_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  wallpaper_loader_->Start(wallpaper_path.value(), 0,
                           base::Bind(&WallpaperManager::OnWallpaperDecoded,
                                      base::Unretained(this),
                                      email,
                                      info.layout,
                                      update_wallpaper));
}

void WallpaperManager::SystemResumed() {
  BatchUpdateWallpaper();
}

void WallpaperManager::TimezoneChanged(const icu::TimeZone& timezone) {
  RestartTimer();
}

void WallpaperManager::ValidateAndLoadWallpaper(
    const std::string& email,
    const WallpaperInfo& info,
    bool update_wallpaper,
    const FilePath& wallpaper_path) {
  FilePath valid_path(wallpaper_path);
  if (!file_util::PathExists(wallpaper_path))
    valid_path = wallpaper_path.AddExtension(".png");
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&WallpaperManager::StartLoad,
                                     base::Unretained(this),
                                     email,
                                     info,
                                     update_wallpaper,
                                     valid_path));
}

}  // chromeos
