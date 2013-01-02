// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wallpaper_manager.h"

#include <vector>

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
#include "chromeos/dbus/root_power_manager_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skia_util.h"

using content::BrowserThread;

namespace {

const int kWallpaperUpdateIntervalSec = 24 * 60 * 60;

// Default quality for encoding wallpaper.
const int kDefaultEncodingQuality = 90;

// A dictionary pref that maps usernames to file paths to their wallpapers.
// Deprecated. Will remove this const char after done migration.
const char kUserWallpapers[] = "UserWallpapers";

const int kThumbnailWidth = 128;
const int kThumbnailHeight = 80;

const int kCacheWallpaperDelayMs = 500;

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

// Maximum number of wallpapers cached by CacheUsersWallpapers().
const int kMaxWallpapersToCache = 3;

// For our scaling ratios we need to round positive numbers.
int RoundPositive(double x) {
  return static_cast<int>(floor(x + 0.5));
}

}  // namespace

namespace chromeos {

const char kWallpaperSequenceTokenName[] = "wallpaper-sequence";

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
      ALLOW_THIS_IN_INITIALIZER_LIST(wallpaper_loader_(
          new UserImageLoader(ImageDecoder::ROBUST_JPEG_CODEC))),
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
  sequence_token_ = BrowserThread::GetBlockingPool()->
      GetNamedSequenceToken(kWallpaperSequenceTokenName);
  task_runner_ = BrowserThread::GetBlockingPool()->
      GetSequencedTaskRunnerWithShutdownBehavior(
          sequence_token_,
          base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
}

// static
void WallpaperManager::RegisterPrefs(PrefServiceSimple* local_state) {
  local_state->RegisterDictionaryPref(prefs::kUsersWallpaperInfo);
  local_state->RegisterDictionaryPref(kUserWallpapers);
  local_state->RegisterDictionaryPref(kUserWallpapersProperties);
}

void WallpaperManager::AddObservers() {
  if (!DBusThreadManager::Get()->GetRootPowerManagerClient()->HasObserver(this))
    DBusThreadManager::Get()->GetRootPowerManagerClient()->AddObserver(this);
  system::TimezoneSettings::GetInstance()->AddObserver(this);
}

void WallpaperManager::EnsureLoggedInUserWallpaperLoaded() {
  // Some browser tests do not have a shell instance. As no wallpaper is needed
  // in these tests anyway, avoid loading one, preventing crashes and speeding
  // up the tests.
  if (!ash::Shell::HasInstance())
    return;

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

bool WallpaperManager::GetLoggedInUserWallpaperInfo(WallpaperInfo* info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (UserManager::Get()->IsLoggedInAsStub()) {
    info->file = current_user_wallpaper_info_.file = "";
    info->layout = current_user_wallpaper_info_.layout =
        ash::WALLPAPER_LAYOUT_CENTER_CROPPED;
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

  if (!user_manager->IsUserLoggedIn()) {
    if (!disable_new_oobe) {
      if (!WizardController::IsDeviceRegistered()) {
        SetDefaultWallpaper();
      } else {
        InitializeRegisteredDeviceWallpaper();
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
            base::Bind(&WallpaperManager::CacheUsersWallpapers,
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
            base::Bind(&WallpaperManager::CacheUsersWallpapers,
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
  DCHECK(BrowserThread::GetBlockingPool()->
      IsRunningSequenceOnCurrentThread(sequence_token_));
  int width = wallpaper.image().width();
  int height = wallpaper.image().height();
  int resized_width;
  int resized_height;

  if (layout == ash::WALLPAPER_LAYOUT_CENTER_CROPPED) {
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
  } else if (layout == ash::WALLPAPER_LAYOUT_STRETCH) {
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

  SkBitmap image = *(resized_image.bitmap());
  scoped_refptr<base::RefCountedBytes> data = new base::RefCountedBytes();
  SkAutoLockPixels lock_input(image);
  gfx::JPEGCodec::Encode(
      reinterpret_cast<unsigned char*>(image.getAddr32(0, 0)),
      gfx::JPEGCodec::FORMAT_SkBitmap,
      image.width(),
      image.height(),
      image.width() * image.bytesPerPixel(),
      kDefaultEncodingQuality, &data->data());
  SaveWallpaperInternal(path,
                        reinterpret_cast<const char*>(data->front()),
                        data->size());
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
    const UserImage& wallpaper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::string wallpaper_path =
      GetOriginalWallpaperPathForUser(username).value();

  // If decoded wallpaper is empty, we are probably failed to decode the file.
  // Use default wallpaper in this case.
  if (wallpaper.image().isNull()) {
    SetDefaultWallpaper();
    return;
  }

  bool is_persistent =
      !UserManager::Get()->IsUserNonCryptohomeDataEphemeral(username);

  wallpaper.image().EnsureRepsForSupportedScaleFactors();
  scoped_ptr<gfx::ImageSkia> deep_copy(wallpaper.image().DeepCopy());

  WallpaperInfo wallpaper_info = {
      wallpaper_path,
      layout,
      type,
      // Date field is not used.
      base::Time::Now().LocalMidnight()
  };
  // Block shutdown on this task. Otherwise, we may lost the custom wallpaper
  // user selected.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner =
      BrowserThread::GetBlockingPool()->
          GetSequencedTaskRunnerWithShutdownBehavior(sequence_token_,
              base::SequencedWorkerPool::BLOCK_SHUTDOWN);
  // TODO(bshe): This may break if RawImage becomes RefCountedMemory.
  blocking_task_runner->PostTask(FROM_HERE,
      base::Bind(&WallpaperManager::ProcessCustomWallpaper,
                 base::Unretained(this),
                 username,
                 is_persistent,
                 wallpaper_info,
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
}

void WallpaperManager::SetDefaultWallpaper() {
  ash::DesktopBackgroundController* controller =
      ash::Shell::GetInstance()->desktop_background_controller();
  ash::WallpaperResolution resolution = controller->GetAppropriateResolution();
  ash::WallpaperInfo info;
  if (UserManager::Get()->IsLoggedInAsGuest()) {
    info = (resolution == ash::WALLPAPER_RESOLUTION_LARGE) ?
        ash::kGuestLargeWallpaper : ash::kGuestSmallWallpaper;
  } else {
    info = (resolution == ash::WALLPAPER_RESOLUTION_LARGE) ?
        ash::kDefaultLargeWallpaper : ash::kDefaultSmallWallpaper;
  }

  // Prevents loading of the same wallpaper as the currently loading/loaded one.
  if (controller->GetWallpaperIDR() == info.idr)
    return;

  current_wallpaper_path_.clear();
  loaded_wallpapers_++;
  controller->SetDefaultWallpaper(info);
}

void WallpaperManager::SetInitialUserWallpaper(const std::string& username,
                                               bool is_persistent) {
  current_user_wallpaper_info_.file = "";
  current_user_wallpaper_info_.layout = ash::WALLPAPER_LAYOUT_CENTER_CROPPED;
  current_user_wallpaper_info_.type = User::DEFAULT;
  current_user_wallpaper_info_.date = base::Time::Now().LocalMidnight();

  WallpaperInfo info = current_user_wallpaper_info_;
  SetUserWallpaperInfo(username, info, is_persistent);

  // Some browser tests do not have a shell instance. As no wallpaper is needed
  // in these tests anyway, avoid loading one, preventing crashes and speeding
  // up the tests.
  if (ash::Shell::HasInstance())
    SetDefaultWallpaper();
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
  if (email == kGuestUserEMail) {
    SetDefaultWallpaper();
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
        bool is_small  = (resolution == ash::WALLPAPER_RESOLUTION_SMALL);
        FilePath wallpaper_path = GetWallpaperPathForUser(email, is_small);
        if (current_wallpaper_path_ == wallpaper_path)
          return;
        current_wallpaper_path_ = wallpaper_path;
        loaded_wallpapers_++;

        task_runner_->PostTask(FROM_HERE,
            base::Bind(&WallpaperManager::GetCustomWallpaperInternal,
                       base::Unretained(this), email, info, wallpaper_path,
                       true /* update wallpaper */));
        return;
      }

      if (info.file.empty()) {
        // Uses default built-in wallpaper when file is empty. Eventually, we
        // will only ship one built-in wallpaper in ChromeOS image.
        SetDefaultWallpaper();
        return;
      }

      // Load downloaded ONLINE or converted DEFAULT wallpapers.
      LoadWallpaper(email, info, true /* update wallpaper */);
    }
  } else {
    SetInitialUserWallpaper(email, true);
  }
}

void WallpaperManager::SetWallpaperFromImageSkia(
    const gfx::ImageSkia& wallpaper,
    ash::WallpaperLayout layout) {
  ash::Shell::GetInstance()->desktop_background_controller()->
      SetCustomWallpaper(wallpaper, layout);
}

void WallpaperManager::UpdateWallpaper() {
  ClearWallpaperCache();
  current_wallpaper_path_.clear();
  SetUserWallpaper(last_selected_user_);
}

// WallpaperManager, private: --------------------------------------------------

WallpaperManager::~WallpaperManager() {
  ClearObsoleteWallpaperPrefs();
  DBusThreadManager::Get()->GetRootPowerManagerClient()->RemoveObserver(this);
  system::TimezoneSettings::GetInstance()->RemoveObserver(this);
  weak_factory_.InvalidateWeakPtrs();
}

void WallpaperManager::BatchUpdateWallpaper() {
  NOTIMPLEMENTED();
}

void WallpaperManager::CacheUsersWallpapers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UserList users = UserManager::Get()->GetUsers();

  if (!users.empty()) {
    UserList::const_iterator it = users.begin();
    // Skip the wallpaper of first user in the list. It should have been cached.
    it++;
    for (int cached = 0;
         it != users.end() && cached < kMaxWallpapersToCache;
         ++it, ++cached) {
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
      bool is_small  = (resolution == ash::WALLPAPER_RESOLUTION_SMALL);
      FilePath wallpaper_path = GetWallpaperPathForUser(email, is_small);
      task_runner_->PostTask(FROM_HERE,
          base::Bind(&WallpaperManager::GetCustomWallpaperInternal,
                     base::Unretained(this), email, info, wallpaper_path,
                     false /* do not update wallpaper */));
      return;
    }
    LoadWallpaper(email, info, false /* do not update wallpaper */);
  }
}

void WallpaperManager::ClearObsoleteWallpaperPrefs() {
  PrefService* prefs = g_browser_process->local_state();
  DictionaryPrefUpdate wallpaper_properties_pref(prefs,
      kUserWallpapersProperties);
  wallpaper_properties_pref->Clear();
  DictionaryPrefUpdate wallpapers_pref(prefs, kUserWallpapers);
  wallpapers_pref->Clear();
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

void WallpaperManager::InitializeRegisteredDeviceWallpaper() {
  if (CrosSettingsProvider::TEMPORARILY_UNTRUSTED ==
      CrosSettings::Get()->PrepareTrustedValues(
          base::Bind(&WallpaperManager::InitializeRegisteredDeviceWallpaper,
                     base::Unretained(this)))) {
    return;
  }
  bool disable_boot_animation = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kDisableBootAnimation);
  bool show_users = true;
  bool result = CrosSettings::Get()->GetBoolean(
      kAccountsPrefShowUserNamesOnSignIn, &show_users);
  DCHECK(result) << "Unable to fetch setting "
                 << kAccountsPrefShowUserNamesOnSignIn;
  const chromeos::UserList& users = UserManager::Get()->GetUsers();
  if (!show_users || users.empty()) {
    // Boot into sign in form, preload default wallpaper.
    SetDefaultWallpaper();
    return;
  }

  if (!disable_boot_animation) {
    // Normal boot, load user wallpaper.
    // If normal boot animation is disabled wallpaper would be set
    // asynchronously once user pods are loaded.
    SetUserWallpaper(users[0]->email());
  }
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
    if (info.layout != ash::WALLPAPER_LAYOUT_STRETCH &&
        resolution == ash::WALLPAPER_RESOLUTION_SMALL) {
      file_name = FilePath(file_name).InsertBeforeExtension(
          kSmallWallpaperSuffix).value();
    }
    CHECK(PathService::Get(chrome::DIR_CHROMEOS_WALLPAPERS, &wallpaper_dir));
    wallpaper_path = wallpaper_dir.Append(file_name);
    if (current_wallpaper_path_ == wallpaper_path)
      return;
    if (update_wallpaper)
      current_wallpaper_path_ = wallpaper_path;
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

  if (UserManager::Get()->IsUserNonCryptohomeDataEphemeral(email)) {
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
    info->layout = ash::WALLPAPER_LAYOUT_CENTER_CROPPED;
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

void WallpaperManager::GetCustomWallpaperInternal(
    const std::string& email,
    const WallpaperInfo& info,
    const FilePath& wallpaper_path,
    bool update_wallpaper) {
  DCHECK(BrowserThread::GetBlockingPool()->
      IsRunningSequenceOnCurrentThread(sequence_token_));
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
                           ash::WALLPAPER_LAYOUT_CENTER_CROPPED,
                           User::DEFAULT,
                           base::Time::Now().LocalMidnight()
                         };
    SetUserWallpaperInfo(email, info, true);

    if (update_wallpaper)
      SetDefaultWallpaper();
    return;
  }

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
    scoped_ptr<gfx::ImageSkia> image,
    const UserImage::RawImage& raw_image) {
  DCHECK(BrowserThread::GetBlockingPool()->
      IsRunningSequenceOnCurrentThread(sequence_token_));
  UserImage wallpaper(*image.get(), raw_image);
  if (persistent)
    SaveCustomWallpaper(email, FilePath(info.file), info.layout, wallpaper);
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
}

void WallpaperManager::SaveWallpaperInternal(const FilePath& path,
                                             const char* data,
                                             int size) {
  int written_bytes = file_util::WriteFile(path, data, size);
  DCHECK(written_bytes == size);
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

void WallpaperManager::OnResume(const base::TimeDelta& sleep_duration) {
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
