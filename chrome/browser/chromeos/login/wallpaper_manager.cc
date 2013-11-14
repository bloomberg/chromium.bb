// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wallpaper_manager.h"

#include <vector>

#include "ash/shell.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/worker_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skia_util.h"

using content::BrowserThread;

namespace {

// The amount of delay before starts to move custom wallpapers to the new place.
const int kMoveCustomWallpaperDelaySeconds = 30;

// Default quality for encoding wallpaper.
const int kDefaultEncodingQuality = 90;

// A dictionary pref that maps usernames to file paths to their wallpapers.
// Deprecated. Will remove this const char after done migration.
const char kUserWallpapers[] = "UserWallpapers";

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

// Returns custom wallpaper directory by appending corresponding |sub_dir|.
base::FilePath GetCustomWallpaperDir(const char* sub_dir) {
  base::FilePath custom_wallpaper_dir;
  CHECK(PathService::Get(chrome::DIR_CHROMEOS_CUSTOM_WALLPAPERS,
                         &custom_wallpaper_dir));
  return custom_wallpaper_dir.Append(sub_dir);
}

bool MoveCustomWallpaperDirectory(const char* sub_dir,
                                  const std::string& email,
                                  const std::string& user_id_hash) {
  base::FilePath base_path = GetCustomWallpaperDir(sub_dir);
  base::FilePath to_path = base_path.Append(user_id_hash);
  base::FilePath from_path = base_path.Append(email);
  if (base::PathExists(from_path))
    return base::Move(from_path, to_path);
  return false;
}

}  // namespace

namespace chromeos {

const char kWallpaperSequenceTokenName[] = "wallpaper-sequence";

const char kSmallWallpaperSuffix[] = "_small";
const char kLargeWallpaperSuffix[] = "_large";

const char kSmallWallpaperSubDir[] = "small";
const char kLargeWallpaperSubDir[] = "large";
const char kOriginalWallpaperSubDir[] = "original";
const char kThumbnailWallpaperSubDir[] = "thumb";

static WallpaperManager* g_wallpaper_manager = NULL;

// WallpaperManager, public: ---------------------------------------------------

// TestApi. For testing purpose
WallpaperManager::TestApi::TestApi(WallpaperManager* wallpaper_manager)
    : wallpaper_manager_(wallpaper_manager) {
}

WallpaperManager::TestApi::~TestApi() {
}

base::FilePath WallpaperManager::TestApi::current_wallpaper_path() {
  return wallpaper_manager_->current_wallpaper_path_;
}

// static
WallpaperManager* WallpaperManager::Get() {
  if (!g_wallpaper_manager)
    g_wallpaper_manager = new WallpaperManager();
  return g_wallpaper_manager;
}

WallpaperManager::WallpaperManager()
    : loaded_wallpapers_(0),
      command_line_for_testing_(NULL),
      should_cache_wallpaper_(false),
      weak_factory_(this) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
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
  wallpaper_loader_ = new UserImageLoader(ImageDecoder::ROBUST_JPEG_CODEC,
                                          task_runner_);
}

WallpaperManager::~WallpaperManager() {
  // TODO(bshe): Lifetime of WallpaperManager needs more consideration.
  // http://crbug.com/171694
  DCHECK(!show_user_name_on_signin_subscription_);
  ClearObsoleteWallpaperPrefs();
  weak_factory_.InvalidateWeakPtrs();
}

void WallpaperManager::Shutdown() {
  show_user_name_on_signin_subscription_.reset();
}

// static
void WallpaperManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kUsersWallpaperInfo);
  registry->RegisterDictionaryPref(kUserWallpapers);
  registry->RegisterDictionaryPref(kUserWallpapersProperties);
}

void WallpaperManager::AddObservers() {
  show_user_name_on_signin_subscription_ =
      CrosSettings::Get()->AddSettingsObserver(
          kAccountsPrefShowUserNamesOnSignIn,
          base::Bind(&WallpaperManager::InitializeRegisteredDeviceWallpaper,
                     base::Unretained(this)));
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

base::FilePath WallpaperManager::GetCustomWallpaperPath(
    const char* sub_dir,
    const std::string& user_id_hash,
    const std::string& file) {
  base::FilePath custom_wallpaper_path = GetCustomWallpaperDir(sub_dir);
  return custom_wallpaper_path.Append(user_id_hash).Append(file);
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

base::FilePath WallpaperManager::GetOriginalWallpaperPathForUser(
    const std::string& username) {
  std::string filename = username + kOriginalCustomWallpaperSuffix;
  base::FilePath user_data_dir;
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

  CommandLine* command_line = GetComandLine();
  if (command_line->HasSwitch(chromeos::switches::kGuestSession)) {
    // Guest wallpaper should be initialized when guest login.
    // Note: This maybe called before login. So IsLoggedInAsGuest can not be
    // used here to determine if current user is guest.
    return;
  }

  if (command_line->HasSwitch(::switches::kTestType))
    WizardController::SetZeroDelays();

  // Zero delays is also set in autotests.
  if (WizardController::IsZeroDelayEnabled()) {
    // Ensure tests have some sort of wallpaper.
    ash::Shell::GetInstance()->desktop_background_controller()->
        CreateEmptyWallpaper();
    return;
  }

  if (!user_manager->IsUserLoggedIn()) {
    if (!StartupUtils::IsDeviceRegistered())
      SetDefaultWallpaper();
    else
      InitializeRegisteredDeviceWallpaper();
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
      BrowserThread::PostDelayedTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&WallpaperManager::MoveLoggedInUserCustomWallpaper,
                     weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromSeconds(kMoveCustomWallpaperDelaySeconds));
      break;
    }
    case chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE: {
      if (!GetComandLine()->HasSwitch(switches::kDisableBootAnimation)) {
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
      NotifyAnimationFinished();
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
  WallpaperInfo info;
  GetUserWallpaperInfo(email, &info);
  PrefService* prefs = g_browser_process->local_state();
  DictionaryPrefUpdate prefs_wallpapers_info_update(prefs,
      prefs::kUsersWallpaperInfo);
  prefs_wallpapers_info_update->RemoveWithoutPathExpansion(email, NULL);
  DeleteUserWallpapers(email, info.file);
}

bool WallpaperManager::ResizeWallpaper(
    const UserImage& wallpaper,
    ash::WallpaperLayout layout,
    int preferred_width,
    int preferred_height,
    scoped_refptr<base::RefCountedBytes>* output) {
  DCHECK(BrowserThread::GetBlockingPool()->
      IsRunningSequenceOnCurrentThread(sequence_token_));
  int width = wallpaper.image().width();
  int height = wallpaper.image().height();
  int resized_width;
  int resized_height;
  *output = new base::RefCountedBytes();

  if (layout == ash::WALLPAPER_LAYOUT_CENTER_CROPPED) {
    // Do not resize custom wallpaper if it is smaller than preferred size.
    if (!(width > preferred_width && height > preferred_height))
      return false;

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
    resized_width = width;
    resized_height = height;
  }

  gfx::ImageSkia resized_image = gfx::ImageSkiaOperations::CreateResizedImage(
      wallpaper.image(),
      skia::ImageOperations::RESIZE_LANCZOS3,
      gfx::Size(resized_width, resized_height));

  SkBitmap image = *(resized_image.bitmap());
  SkAutoLockPixels lock_input(image);
  gfx::JPEGCodec::Encode(
      reinterpret_cast<unsigned char*>(image.getAddr32(0, 0)),
      gfx::JPEGCodec::FORMAT_SkBitmap,
      image.width(),
      image.height(),
      image.width() * image.bytesPerPixel(),
      kDefaultEncodingQuality, &(*output)->data());
  return true;
}

void WallpaperManager::ResizeAndSaveWallpaper(const UserImage& wallpaper,
                                              const base::FilePath& path,
                                              ash::WallpaperLayout layout,
                                              int preferred_width,
                                              int preferred_height) {
  if (layout == ash::WALLPAPER_LAYOUT_CENTER) {
    // TODO(bshe): Generates cropped custom wallpaper for CENTER layout.
    if (base::PathExists(path))
      base::DeleteFile(path, false);
    return;
  }
  scoped_refptr<base::RefCountedBytes> data;
  if (ResizeWallpaper(wallpaper, layout, preferred_width, preferred_height,
                      &data)) {
    SaveWallpaperInternal(path,
                          reinterpret_cast<const char*>(data->front()),
                          data->size());
  }
}

void WallpaperManager::SetCustomWallpaper(const std::string& username,
                                          const std::string& user_id_hash,
                                          const std::string& file,
                                          ash::WallpaperLayout layout,
                                          User::WallpaperType type,
                                          const UserImage& wallpaper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::FilePath wallpaper_path =
      GetCustomWallpaperPath(kOriginalWallpaperSubDir, user_id_hash, file);

  // If decoded wallpaper is empty, we are probably failed to decode the file.
  // Use default wallpaper in this case.
  if (wallpaper.image().isNull()) {
    SetDefaultWallpaper();
    return;
  }

  bool is_persistent =
      !UserManager::Get()->IsUserNonCryptohomeDataEphemeral(username);

  wallpaper.image().EnsureRepsForSupportedScales();
  scoped_ptr<gfx::ImageSkia> deep_copy(wallpaper.image().DeepCopy());

  WallpaperInfo wallpaper_info = {
      wallpaper_path.value(),
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
                 user_id_hash,
                 is_persistent,
                 wallpaper_info,
                 base::Passed(&deep_copy),
                 wallpaper.raw_image()));
  ash::Shell::GetInstance()->desktop_background_controller()->
      SetCustomWallpaper(wallpaper.image(), layout);

  std::string relative_path = base::FilePath(user_id_hash).Append(file).value();
  // User's custom wallpaper path is determined by relative path and the
  // appropriate wallpaper resolution in GetCustomWallpaperInternal.
  WallpaperInfo info = {
      relative_path,
      layout,
      User::CUSTOMIZED,
      base::Time::Now().LocalMidnight()
  };
  SetUserWallpaperInfo(username, info, is_persistent);
}

void WallpaperManager::SetDefaultWallpaper() {
  current_wallpaper_path_.clear();
  if (ash::Shell::GetInstance()->desktop_background_controller()->
          SetDefaultWallpaper(UserManager::Get()->IsLoggedInAsGuest()))
    loaded_wallpapers_++;
}

void WallpaperManager::SetInitialUserWallpaper(const std::string& username,
                                               bool is_persistent) {
  current_user_wallpaper_info_.file = "";
  current_user_wallpaper_info_.layout = ash::WALLPAPER_LAYOUT_CENTER_CROPPED;
  current_user_wallpaper_info_.type = User::DEFAULT;
  current_user_wallpaper_info_.date = base::Time::Now().LocalMidnight();

  WallpaperInfo info = current_user_wallpaper_info_;
  SetUserWallpaperInfo(username, info, is_persistent);
  SetLastSelectedUser(username);

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
  if (email == UserManager::kGuestUserName) {
    SetDefaultWallpaper();
    return;
  }

  if (!UserManager::Get()->IsKnownUser(email))
    return;

  SetLastSelectedUser(email);

  WallpaperInfo info;

  if (GetUserWallpaperInfo(email, &info)) {
    gfx::ImageSkia user_wallpaper;
    current_user_wallpaper_info_ = info;
    if (GetWallpaperFromCache(email, &user_wallpaper)) {
      ash::Shell::GetInstance()->desktop_background_controller()->
          SetCustomWallpaper(user_wallpaper, info.layout);
    } else {
      if (info.type == User::CUSTOMIZED) {
        ash::WallpaperResolution resolution = ash::Shell::GetInstance()->
            desktop_background_controller()->GetAppropriateResolution();
        const char* sub_dir = (resolution == ash::WALLPAPER_RESOLUTION_SMALL) ?
            kSmallWallpaperSubDir : kLargeWallpaperSubDir;
        // Wallpaper is not resized when layout is ash::WALLPAPER_LAYOUT_CENTER.
        // Original wallpaper should be used in this case.
        // TODO(bshe): Generates cropped custom wallpaper for CENTER layout.
        if (info.layout == ash::WALLPAPER_LAYOUT_CENTER)
          sub_dir = kOriginalWallpaperSubDir;
        base::FilePath wallpaper_path = GetCustomWallpaperDir(sub_dir);
        wallpaper_path = wallpaper_path.Append(info.file);
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
  // For GAIA login flow, the last_selected_user_ may not be set before user
  // login. If UpdateWallpaper is called at GAIA login screen, no wallpaper will
  // be set. It could result a black screen on external monitors.
  // See http://crbug.com/265689 for detail.
  if (last_selected_user_.empty()) {
    SetDefaultWallpaper();
    return;
  }
  SetUserWallpaper(last_selected_user_);
}

void WallpaperManager::AddObserver(WallpaperManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void WallpaperManager::RemoveObserver(WallpaperManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void WallpaperManager::NotifyAnimationFinished() {
  FOR_EACH_OBSERVER(
      Observer, observers_, OnWallpaperAnimationFinished(last_selected_user_));
}

// WallpaperManager, private: --------------------------------------------------

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
    base::FilePath wallpaper_dir;
    base::FilePath wallpaper_path;
    if (info.type == User::CUSTOMIZED) {
      ash::WallpaperResolution resolution = ash::Shell::GetInstance()->
          desktop_background_controller()->GetAppropriateResolution();
      const char* sub_dir  = (resolution == ash::WALLPAPER_RESOLUTION_SMALL) ?
            kSmallWallpaperSubDir : kLargeWallpaperSubDir;
      base::FilePath wallpaper_path = GetCustomWallpaperDir(sub_dir);
      wallpaper_path = wallpaper_path.Append(info.file);
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

void WallpaperManager::DeleteAllExcept(const base::FilePath& path) {
  base::FilePath dir = path.DirName();
  if (base::DirectoryExists(dir)) {
    base::FileEnumerator files(dir, false, base::FileEnumerator::FILES);
    for (base::FilePath current = files.Next(); !current.empty();
         current = files.Next()) {
      if (current != path)
        base::DeleteFile(current, false);
    }
  }
}

void WallpaperManager::DeleteWallpaperInList(
    const std::vector<base::FilePath>& file_list) {
  for (std::vector<base::FilePath>::const_iterator it = file_list.begin();
       it != file_list.end(); ++it) {
    base::FilePath path = *it;
    // Some users may still have legacy wallpapers with png extension. We need
    // to delete these wallpapers too.
    if (!base::DeleteFile(path, true) &&
        !base::DeleteFile(path.AddExtension(".png"), false)) {
      LOG(ERROR) << "Failed to remove user wallpaper at " << path.value();
    }
  }
}

void WallpaperManager::DeleteUserWallpapers(const std::string& email,
                                            const std::string& path_to_file) {
  std::vector<base::FilePath> file_to_remove;
  // Remove small user wallpaper.
  base::FilePath wallpaper_path =
      GetCustomWallpaperDir(kSmallWallpaperSubDir);
  // Remove old directory if exists
  file_to_remove.push_back(wallpaper_path.Append(email));
  wallpaper_path = wallpaper_path.Append(path_to_file).DirName();
  file_to_remove.push_back(wallpaper_path);

  // Remove large user wallpaper.
  wallpaper_path = GetCustomWallpaperDir(kLargeWallpaperSubDir);
  file_to_remove.push_back(wallpaper_path.Append(email));
  wallpaper_path = wallpaper_path.Append(path_to_file);
  file_to_remove.push_back(wallpaper_path);

  // Remove user wallpaper thumbnail.
  wallpaper_path = GetCustomWallpaperDir(kThumbnailWallpaperSubDir);
  file_to_remove.push_back(wallpaper_path.Append(email));
  wallpaper_path = wallpaper_path.Append(path_to_file);
  file_to_remove.push_back(wallpaper_path);

  // Remove original user wallpaper.
  wallpaper_path = GetCustomWallpaperDir(kOriginalWallpaperSubDir);
  file_to_remove.push_back(wallpaper_path.Append(email));
  wallpaper_path = wallpaper_path.Append(path_to_file);
  file_to_remove.push_back(wallpaper_path);

  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&WallpaperManager::DeleteWallpaperInList,
                 base::Unretained(this),
                 file_to_remove),
      false);
}

void WallpaperManager::EnsureCustomWallpaperDirectories(
    const std::string& user_id_hash) {
  base::FilePath dir;
  dir = GetCustomWallpaperDir(kSmallWallpaperSubDir);
  dir = dir.Append(user_id_hash);
  if (!base::PathExists(dir))
    file_util::CreateDirectory(dir);
  dir = GetCustomWallpaperDir(kLargeWallpaperSubDir);
  dir = dir.Append(user_id_hash);
  if (!base::PathExists(dir))
    file_util::CreateDirectory(dir);
  dir = GetCustomWallpaperDir(kOriginalWallpaperSubDir);
  dir = dir.Append(user_id_hash);
  if (!base::PathExists(dir))
    file_util::CreateDirectory(dir);
  dir = GetCustomWallpaperDir(kThumbnailWallpaperSubDir);
  dir = dir.Append(user_id_hash);
  if (!base::PathExists(dir))
    file_util::CreateDirectory(dir);
}

CommandLine* WallpaperManager::GetComandLine() {
  CommandLine* command_line = command_line_for_testing_ ?
      command_line_for_testing_ : CommandLine::ForCurrentProcess();
  return command_line;
}

void WallpaperManager::InitializeRegisteredDeviceWallpaper() {
  if (UserManager::Get()->IsUserLoggedIn())
    return;

  bool disable_boot_animation = GetComandLine()->
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
  base::FilePath wallpaper_dir;
  base::FilePath wallpaper_path;
  if (info.type == User::ONLINE) {
    std::string file_name = GURL(info.file).ExtractFileName();
    ash::WallpaperResolution resolution = ash::Shell::GetInstance()->
        desktop_background_controller()->GetAppropriateResolution();
    // Only solid color wallpapers have stretch layout and they have only one
    // resolution.
    if (info.layout != ash::WALLPAPER_LAYOUT_STRETCH &&
        resolution == ash::WALLPAPER_RESOLUTION_SMALL) {
      file_name = base::FilePath(file_name).InsertBeforeExtension(
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
  } else if (info.type == User::DEFAULT) {
    // Default wallpapers are migrated from M21 user profiles. A code refactor
    // overlooked that case and caused these wallpapers not being loaded at all.
    // On some slow devices, it caused login webui not visible after upgrade to
    // M26 from M21. See crosbug.com/38429 for details.
    base::FilePath user_data_dir;
    PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    wallpaper_path = user_data_dir.Append(info.file);
    StartLoad(email, info, update_wallpaper, wallpaper_path);
  } else {
    // In unexpected cases, revert to default wallpaper to fail safely. See
    // crosbug.com/38429.
    LOG(ERROR) << "Wallpaper reverts to default unexpected.";
    SetDefaultWallpaper();
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

void WallpaperManager::MoveCustomWallpapersOnWorker(
    const std::string& email,
    const std::string& user_id_hash) {
  DCHECK(BrowserThread::GetBlockingPool()->
      IsRunningSequenceOnCurrentThread(sequence_token_));
  if (MoveCustomWallpaperDirectory(kOriginalWallpaperSubDir,
                                   email,
                                   user_id_hash)) {
    // Consider success if the original wallpaper is moved to the new directory.
    // Original wallpaper is the fallback if the correct resolution wallpaper
    // can not be found.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperManager::MoveCustomWallpapersSuccess,
                   base::Unretained(this),
                   email,
                   user_id_hash));
  }
  MoveCustomWallpaperDirectory(kLargeWallpaperSubDir, email, user_id_hash);
  MoveCustomWallpaperDirectory(kSmallWallpaperSubDir, email, user_id_hash);
  MoveCustomWallpaperDirectory(kThumbnailWallpaperSubDir, email, user_id_hash);
}

void WallpaperManager::MoveCustomWallpapersSuccess(
    const std::string& email,
    const std::string& user_id_hash) {
  WallpaperInfo info;
  GetUserWallpaperInfo(email, &info);
  if (info.type == User::CUSTOMIZED) {
    // New file field should include user id hash in addition to file name.
    // This is needed because at login screen, user id hash is not available.
    std::string relative_path =
        base::FilePath(user_id_hash).Append(info.file).value();
    info.file = relative_path;
    bool is_persistent =
        !UserManager::Get()->IsUserNonCryptohomeDataEphemeral(email);
    SetUserWallpaperInfo(email, info, is_persistent);
  }
}

void WallpaperManager::MoveLoggedInUserCustomWallpaper() {
  const User* logged_in_user = UserManager::Get()->GetLoggedInUser();
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WallpaperManager::MoveCustomWallpapersOnWorker,
                 base::Unretained(this),
                 logged_in_user->email(),
                 logged_in_user->username_hash()));
}

void WallpaperManager::GetCustomWallpaperInternal(
    const std::string& email,
    const WallpaperInfo& info,
    const base::FilePath& wallpaper_path,
    bool update_wallpaper) {
  DCHECK(BrowserThread::GetBlockingPool()->
      IsRunningSequenceOnCurrentThread(sequence_token_));

  base::FilePath valid_path = wallpaper_path;
  if (!base::PathExists(wallpaper_path)) {
    // Falls back on original file if the correct resoltuion file does not
    // exist. This may happen when the original custom wallpaper is small or
    // browser shutdown before resized wallpaper saved.
    valid_path = GetCustomWallpaperDir(kOriginalWallpaperSubDir);
    valid_path = valid_path.Append(info.file);
  }

  if (!base::PathExists(valid_path)) {
    // Falls back to custom wallpaper that uses email as part of its file path.
    // Note that email is used instead of user_id_hash here.
    valid_path = GetCustomWallpaperPath(kOriginalWallpaperSubDir, email,
                                        info.file);
  }

  if (!base::PathExists(valid_path)) {
    LOG(ERROR) << "Failed to load previously selected custom wallpaper. " <<
                  "Fallback to default wallpaper";
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&WallpaperManager::SetDefaultWallpaper,
                                       base::Unretained(this)));
  } else {
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&WallpaperManager::StartLoad,
                                       base::Unretained(this),
                                       email,
                                       info,
                                       update_wallpaper,
                                       valid_path));
  }
}

void WallpaperManager::OnWallpaperDecoded(const std::string& email,
                                          ash::WallpaperLayout layout,
                                          bool update_wallpaper,
                                          const UserImage& wallpaper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TRACE_EVENT_ASYNC_END0("ui", "LoadAndDecodeWallpaper", this);

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
    const std::string& user_id_hash,
    bool persistent,
    const WallpaperInfo& info,
    scoped_ptr<gfx::ImageSkia> image,
    const UserImage::RawImage& raw_image) {
  DCHECK(BrowserThread::GetBlockingPool()->
      IsRunningSequenceOnCurrentThread(sequence_token_));
  UserImage wallpaper(*image.get(), raw_image);
  if (persistent) {
    SaveCustomWallpaper(user_id_hash, base::FilePath(info.file), info.layout,
                        wallpaper);
  }
}

void WallpaperManager::SaveCustomWallpaper(const std::string& user_id_hash,
                                           const base::FilePath& original_path,
                                           ash::WallpaperLayout layout,
                                           const UserImage& wallpaper) {
  DCHECK(BrowserThread::GetBlockingPool()->
      IsRunningSequenceOnCurrentThread(sequence_token_));
  EnsureCustomWallpaperDirectories(user_id_hash);
  std::string file_name = original_path.BaseName().value();
  base::FilePath small_wallpaper_path =
      GetCustomWallpaperPath(kSmallWallpaperSubDir, user_id_hash, file_name);
  base::FilePath large_wallpaper_path =
      GetCustomWallpaperPath(kLargeWallpaperSubDir, user_id_hash, file_name);

  // Re-encode orginal file to jpeg format and saves the result in case that
  // resized wallpaper is not generated (i.e. chrome shutdown before resized
  // wallpaper is saved).
  ResizeAndSaveWallpaper(wallpaper, original_path,
                         ash::WALLPAPER_LAYOUT_STRETCH,
                         wallpaper.image().width(),
                         wallpaper.image().height());
  DeleteAllExcept(original_path);

  ResizeAndSaveWallpaper(wallpaper, small_wallpaper_path, layout,
                         ash::kSmallWallpaperMaxWidth,
                         ash::kSmallWallpaperMaxHeight);
  DeleteAllExcept(small_wallpaper_path);
  ResizeAndSaveWallpaper(wallpaper, large_wallpaper_path, layout,
                         ash::kLargeWallpaperMaxWidth,
                         ash::kLargeWallpaperMaxHeight);
  DeleteAllExcept(large_wallpaper_path);
}

void WallpaperManager::RecordUma(User::WallpaperType type, int index) {
  UMA_HISTOGRAM_ENUMERATION("Ash.Wallpaper.Type", type,
                            User::WALLPAPER_TYPE_COUNT);
}

void WallpaperManager::SaveWallpaperInternal(const base::FilePath& path,
                                             const char* data,
                                             int size) {
  int written_bytes = file_util::WriteFile(path, data, size);
  DCHECK(written_bytes == size);
}

void WallpaperManager::StartLoad(const std::string& email,
                                 const WallpaperInfo& info,
                                 bool update_wallpaper,
                                 const base::FilePath& wallpaper_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TRACE_EVENT_ASYNC_BEGIN0("ui", "LoadAndDecodeWallpaper", this);

  wallpaper_loader_->Start(wallpaper_path.value(), 0,
                           base::Bind(&WallpaperManager::OnWallpaperDecoded,
                                      base::Unretained(this),
                                      email,
                                      info.layout,
                                      update_wallpaper));
}

}  // namespace chromeos
