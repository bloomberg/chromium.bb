// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wallpaper_manager.h"

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
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skia_util.h"

using content::BrowserThread;

namespace {

const int kWallpaperUpdateIntervalSec = 24 * 60 * 60;

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
const int kDefaultOOBEWallpaperIndex = 20; // IDR_AURA_WALLPAPERS_2_LANDSCAPE8
#else
const int kDefaultOOBEWallpaperIndex = 0;  // IDR_AURA_WALLPAPERS_ROMAINGUY_0
#endif

// Names of nodes with info about wallpaper.
const char kNewWallpaperDateNodeName[] = "date";
const char kNewWallpaperLayoutNodeName[] = "layout";
const char kNewWallpaperFileNodeName[] = "file";
const char kNewWallpaperTypeNodeName[] = "type";

// TODO(bshe): Address the file extension issue. All custom wallpapers should be
// jpeg files.
// File path suffix of the original custom wallpaper.
const char kOriginalCustomWallpaperSuffix[] = "_wallpaper.png";

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
    : ALLOW_THIS_IN_INITIALIZER_LIST(wallpaper_loader_(
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
}

void WallpaperManager::AddObservers() {
  if (!DBusThreadManager::Get()->GetPowerManagerClient()->HasObserver(this))
    DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
  system::TimezoneSettings::GetInstance()->AddObserver(this);
}

void WallpaperManager::EnsureLoggedInUserWallpaperLoaded() {
  bool new_wallpaper_ui_disabled = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kDisableNewWallpaperUI);
  WallpaperInfo info;
  if (!new_wallpaper_ui_disabled &&
      GetLoggedInUserWallpaperInfo(&info)) {
    // TODO(sschmitz): We need an index for default wallpapers for the new UI.
    RecordUma(info.type, -1);
    if (info == current_user_wallpaper_info_)
      return;
  } else {
    User::WallpaperType type;
    int index;
    base::Time date;
    GetLoggedInUserWallpaperProperties(&type, &index, &date);
    RecordUma(type, index);
    if (type == current_user_wallpaper_type_ &&
        index == current_user_wallpaper_index_) {
      return;
    }
  }
  SetUserWallpaper(UserManager::Get()->GetLoggedInUser().email());
}

void WallpaperManager::GetCustomWallpaper(const std::string& email,
                                          bool update_wallpaper) {
  User::WallpaperType type;
  int index;
  base::Time date;
  // Get layout, type and date while on UI thread.
  GetUserWallpaperProperties(email, &type, &index, &date);
  ash::WallpaperLayout layout = static_cast<ash::WallpaperLayout>(index);
  // Initializes file name to a dummy string here. The real file path will
  // be set in GetCustomWallpaperInternal.
  WallpaperInfo info = {
      "DUMMY",
      layout,
      type,
      date
  };
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&WallpaperManager::GetCustomWallpaperInternal,
                 base::Unretained(this), email, info, update_wallpaper));
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

  // TODO(bshe): Remove file extension completely. It is not used and can be
  // misleading. For example, the file extension below should be ".jpg", since
  // the user wallpaper files contain JPEG data.
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

  GetUserWallpaperProperties(UserManager::Get()->GetLoggedInUser().email(),
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

  return GetUserWallpaperInfo(UserManager::Get()->GetLoggedInUser().email(),
                              info);
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
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_USER_CHANGED: {
      // Cancel callback for previous cache requests.
      weak_factory_.InvalidateWeakPtrs();
      wallpaper_cache_.clear();
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
  // Uses simple png encoder to encode image on worker pool. So we do not block
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
    ash::Shell::GetInstance()->desktop_background_controller()->
        SetDefaultWallpaper(ash::GetDefaultWallpaperIndex(), false);
    return;
  }

  bool is_persistent = ShouldPersistDataForUser(username);

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&WallpaperManager::GenerateUserWallpaperThumbnail,
                 base::Unretained(this), username, type, delegate,
                 wallpaper.image()));

  if (is_persistent) {
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&WallpaperManager::SaveCustomWallpaper,
                   base::Unretained(this),
                   username,
                   FilePath(wallpaper_path),
                   layout,
                   wallpaper));
  }

  ash::Shell::GetInstance()->desktop_background_controller()->
      SetCustomWallpaper(wallpaper.image(), layout);

  bool new_wallpaper_ui_disabled = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kDisableNewWallpaperUI);
  if (!new_wallpaper_ui_disabled) {
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
  }
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

void WallpaperManager::SetInitialUserWallpaper(const std::string& username,
                                               bool is_persistent) {
  current_user_wallpaper_type_ = User::DEFAULT;
  if (username == kGuestUser)
    current_user_wallpaper_index_ = ash::GetGuestWallpaperIndex();
  else
    current_user_wallpaper_index_ = ash::GetDefaultWallpaperIndex();

  bool new_wallpaper_ui_disabled = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kDisableNewWallpaperUI);
  if (!new_wallpaper_ui_disabled) {
    current_user_wallpaper_info_.file = "";
    current_user_wallpaper_info_.layout = ash::CENTER_CROPPED;
    current_user_wallpaper_info_.type = User::DEFAULT;
    current_user_wallpaper_info_.date = base::Time::Now().LocalMidnight();

    WallpaperInfo info = current_user_wallpaper_info_;
    SetUserWallpaperInfo(username, info, is_persistent);
  } else {
    SetUserWallpaperProperties(username,
                               current_user_wallpaper_type_,
                               current_user_wallpaper_index_,
                               is_persistent);
  }

  // Some browser tests do not have shell instance. And it is not necessary to
  // create a wallpaper for these tests. Add HasInstance check to prevent tests
  // crash and speed up the tests by avoid loading wallpaper.
  if (ash::Shell::HasInstance()) {
    ash::Shell::GetInstance()->desktop_background_controller()->
        SetDefaultWallpaper(current_user_wallpaper_index_, false);
  }
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
    ash::Shell::GetInstance()->desktop_background_controller()->
      SetDefaultWallpaper(ash::GetGuestWallpaperIndex(), false);
  }

  if (!UserManager::Get()->IsKnownUser(email))
    return;

  bool new_wallpaper_ui_disabled = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kDisableNewWallpaperUI);

  WallpaperInfo info;

  if (!new_wallpaper_ui_disabled &&
      GetUserWallpaperInfo(email, &info)) {
    gfx::ImageSkia user_wallpaper;
    if (GetWallpaperFromCache(email, &user_wallpaper)) {
      ash::Shell::GetInstance()->desktop_background_controller()->
          SetCustomWallpaper(user_wallpaper, info.layout);
    } else {
      if (info.type == User::CUSTOMIZED) {
        BrowserThread::PostTask(
            BrowserThread::FILE, FROM_HERE,
            base::Bind(&WallpaperManager::GetCustomWallpaperInternal,
                       base::Unretained(this), email, info,
                       true /* update wallpaper */));
        return;
      }

      if (info.file.empty()) {
        // Uses default built-in wallpaper when file is empty. Eventually, we
        // will only ship one built-in wallpaper in ChromeOS image.
        ash::Shell::GetInstance()->desktop_background_controller()->
            SetDefaultWallpaper(ash::GetDefaultWallpaperIndex(), false);
        return;
      }

      // Load downloaded ONLINE or converted DEFAULT wallpapers.
      LoadWallpaper(email, info, true /* update wallpaper */);
    }
  } else {
    User::WallpaperType type;
    int index;
    base::Time date;
    GetUserWallpaperProperties(email, &type, &index, &date);
    if (type == User::DAILY && date != base::Time::Now().LocalMidnight()) {
      index = ash::GetNextWallpaperIndex(index);
      SetUserWallpaperProperties(email, User::DAILY, index,
                                 ShouldPersistDataForUser(email));
    } else if (type == User::CUSTOMIZED) {
      gfx::ImageSkia user_wallpaper;
      if (GetWallpaperFromCache(email, &user_wallpaper)) {
        ash::Shell::GetInstance()->desktop_background_controller()->
            SetCustomWallpaper(user_wallpaper, info.layout);
      } else {
        GetCustomWallpaper(email, true /* update wallpaper */);
      }
      return;
    }
    ash::Shell::GetInstance()->desktop_background_controller()->
        SetDefaultWallpaper(index, false);
    if (!new_wallpaper_ui_disabled)
      MigrateBuiltInWallpaper(email);
  }
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
        index = ash::GetNextWallpaperIndex(index);
        SetUserWallpaperProperties(email, type, index, true);
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
  bool new_wallpaper_ui_disabled = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kDisableNewWallpaperUI);

  if (!new_wallpaper_ui_disabled) {
    if (wallpaper_cache_.find(email) == wallpaper_cache_.end())
      return;
    WallpaperInfo info;
    if (GetUserWallpaperInfo(email, &info)) {
      FilePath wallpaper_dir;
      FilePath wallpaper_path;
      if (info.type == User::CUSTOMIZED) {
        BrowserThread::PostTask(
            BrowserThread::FILE, FROM_HERE,
            base::Bind(&WallpaperManager::GetCustomWallpaperInternal,
                       base::Unretained(this), email, info,
                       false /* do not update wallpaper */));
        return;
      }
      LoadWallpaper(email, info, false /* do not update wallpaper */);
    } else {
      MigrateBuiltInWallpaper(email);
      return;
    }
  } else {
    User::WallpaperType type;
    int index;
    base::Time date;
    GetUserWallpaperProperties(email, &type, &index, &date);
    if (type == User::CUSTOMIZED) {
      GetCustomWallpaper(email, false /* do not update wallpaper */);
      return;
    }
    ash::Shell::GetInstance()->desktop_background_controller()->
        CacheDefaultWallpaper(index);
  }
}

void WallpaperManager::CacheThumbnail(const std::string& email,
                                      const gfx::ImageSkia& wallpaper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  custom_wallpaper_thumbnail_cache_[email] = GetWallpaperThumbnail(wallpaper);
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
    if (resolution == ash::SMALL) {
      file_name = FilePath(file_name).InsertBeforeExtension(
          kSmallWallpaperSuffix).value();
    }
    CHECK(PathService::Get(chrome::DIR_CHROMEOS_WALLPAPERS, &wallpaper_dir));
    wallpaper_path = wallpaper_dir.Append(file_name);
  } else {
    FilePath user_data_dir;
    PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    wallpaper_path = user_data_dir.Append(info.file);
  }

  wallpaper_loader_->Start(wallpaper_path.value(), 0,
                           base::Bind(&WallpaperManager::OnWallpaperDecoded,
                                      base::Unretained(this),
                                      email,
                                      info.layout,
                                      update_wallpaper));
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
  custom_wallpaper_thumbnail_cache_[email] = GetWallpaperThumbnail(wallpaper);

  // Notify thumbnail is ready.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&WallpaperManager::OnThumbnailUpdated,
                 base::Unretained(this), delegate));
}

void WallpaperManager::GetCustomWallpaperInternal(const std::string& email,
                                                  const WallpaperInfo& info,
                                                  bool update_wallpaper) {
  ash::WallpaperResolution resolution = ash::Shell::GetInstance()->
      desktop_background_controller()->GetAppropriateResolution();
  bool is_small  = (resolution == ash::SMALL);
  FilePath wallpaper_path = GetWallpaperPathForUser(email, is_small);
  std::string file_name = wallpaper_path.BaseName().value();
  // Falls back on original file if the correct resoltuion file does not
  // exist. This may happen when the original custom wallpaper is small or
  // browser shutdown before resized wallpaper saved.
  if (!file_util::PathExists(wallpaper_path))
    file_name = GetOriginalWallpaperPathForUser(email).BaseName().value();

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

void WallpaperManager::MigrateBuiltInWallpaper(const std::string& email) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!email.empty() && UserManager::Get()->IsKnownUser(email)) {
    PrefService* prefs = g_browser_process->local_state();
    const DictionaryValue* user_wallpapers = prefs->
        GetDictionary(UserManager::kUserWallpapersProperties);
    const base::DictionaryValue* wallpaper_properties;
    User::WallpaperType type;
    int index;
    ash::WallpaperResolution resolution = ash::Shell::GetInstance()->
        desktop_background_controller()->GetAppropriateResolution();
    if (user_wallpapers->GetDictionaryWithoutPathExpansion(email,
            &wallpaper_properties)) {
      type = User::UNKNOWN;
      index = ash::GetInvalidWallpaperIndex();
      int temp;
      wallpaper_properties->GetInteger(kWallpaperTypeNodeName, &temp);
      type = static_cast<User::WallpaperType>(temp);
      wallpaper_properties->GetInteger(kWallpaperIndexNodeName, &index);

      FilePath wallpaper_dir;
      CHECK(PathService::Get(chrome::DIR_CHROMEOS_WALLPAPERS, &wallpaper_dir));
      DCHECK(type != User::UNKNOWN);

      FilePath wallpaper_path;
      if (type != User::CUSTOMIZED) {
        base::RefCountedStaticMemory* wallpaper =
            ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
                GetWallpaperViewInfo(index, ash::LARGE).id,
                ui::SCALE_FACTOR_NONE);

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
                GetWallpaperViewInfo(index, ash::SMALL).id,
                ui::SCALE_FACTOR_NONE);

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
    bool new_wallpaper_ui_disabled = CommandLine::ForCurrentProcess()->
        HasSwitch(switches::kDisableNewWallpaperUI);
    // Updates user pref to default wallpaper.
    if (!new_wallpaper_ui_disabled) {
      WallpaperInfo info = {
                             "",
                             ash::CENTER_CROPPED,
                             User::DEFAULT,
                             base::Time::Now().LocalMidnight()
                         };
      SetUserWallpaperInfo(email, info, true);
    } else {
      SetUserWallpaperProperties(email, User::DEFAULT,
                                 ash::GetDefaultWallpaperIndex(), true);
    }

    if (update_wallpaper) {
      ash::Shell::GetInstance()->desktop_background_controller()->
          SetDefaultWallpaper(ash::GetDefaultWallpaperIndex(), false);
    } else {
      ash::Shell::GetInstance()->desktop_background_controller()->
          CacheDefaultWallpaper(ash::GetDefaultWallpaperIndex());
    }
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&WallpaperManager::CacheThumbnail,
                 base::Unretained(this), email,
                 wallpaper.image().DeepCopy()));
  // Only cache user wallpaper at login screen.
  if (!UserManager::Get()->IsUserLoggedIn()) {
    wallpaper_cache_.insert(std::make_pair(email, wallpaper.image()));
  }
  if (update_wallpaper) {
    ash::Shell::GetInstance()->desktop_background_controller()->
        SetCustomWallpaper(wallpaper.image(), layout);
  }
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
  return !(email == user_manager->GetLoggedInUser().email() &&
           user_manager->IsCurrentUserEphemeral());
}

void WallpaperManager::OnWallpaperLoaded(ash::WallpaperLayout layout,
                                         const UserImage& user_image) {
  SetWallpaperFromImageSkia(user_image.image(), layout);
}

void WallpaperManager::SystemResumed() {
  BatchUpdateWallpaper();
}

void WallpaperManager::TimezoneChanged(const icu::TimeZone& timezone) {
  RestartTimer();
}

}  // chromeos
