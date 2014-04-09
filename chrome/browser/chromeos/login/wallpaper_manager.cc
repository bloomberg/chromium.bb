// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wallpaper_manager.h"

#include <numeric>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/desktop_background/desktop_background_controller.h"
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
#include "grit/ash_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skia_util.h"

using content::BrowserThread;

namespace chromeos {

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

// Maximum number of entries in WallpaperManager::last_load_times_ .
const size_t kLastLoadsStatsMsMaxSize = 4;

// Minimum delay between wallpaper loads, milliseconds.
const unsigned kLoadMinDelayMs = 50;

// Default wallpaper load delay, milliseconds.
const unsigned kLoadDefaultDelayMs = 200;

// Maximum wallpaper load delay, milliseconds.
const unsigned kLoadMaxDelayMs = 2000;

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
                                  const std::string& user_id,
                                  const std::string& user_id_hash) {
  base::FilePath base_path = GetCustomWallpaperDir(sub_dir);
  base::FilePath to_path = base_path.Append(user_id_hash);
  base::FilePath from_path = base_path.Append(user_id);
  if (base::PathExists(from_path))
    return base::Move(from_path, to_path);
  return false;
}

}  // namespace

const char kWallpaperSequenceTokenName[] = "wallpaper-sequence";

const char kSmallWallpaperSuffix[] = "_small";
const char kLargeWallpaperSuffix[] = "_large";

const char kSmallWallpaperSubDir[] = "small";
const char kLargeWallpaperSubDir[] = "large";
const char kOriginalWallpaperSubDir[] = "original";
const char kThumbnailWallpaperSubDir[] = "thumb";

const int kSmallWallpaperMaxWidth = 1366;
const int kSmallWallpaperMaxHeight = 800;
const int kLargeWallpaperMaxWidth = 2560;
const int kLargeWallpaperMaxHeight = 1700;
const int kWallpaperThumbnailWidth = 108;
const int kWallpaperThumbnailHeight = 68;

static WallpaperManager* g_wallpaper_manager = NULL;

// This object is passed between several threads while wallpaper is being
// loaded. It will notify callback when last reference to it is removed
// (thus indicating that the last load action has finished).
class MovableOnDestroyCallback {
 public:
  explicit MovableOnDestroyCallback(const base::Closure& callback)
      : callback_(callback) {
  }

  ~MovableOnDestroyCallback() {
    if (!callback_.is_null())
      callback_.Run();
  }

 private:
  base::Closure callback_;
};

WallpaperManager::PendingWallpaper::PendingWallpaper(
    const base::TimeDelta delay,
    const std::string& user_id)
    : user_id_(user_id),
      default_(false),
      on_finish_(new MovableOnDestroyCallback(
          base::Bind(&WallpaperManager::PendingWallpaper::OnWallpaperSet,
                     this))) {
  timer.Start(
      FROM_HERE,
      delay,
      base::Bind(&WallpaperManager::PendingWallpaper::ProcessRequest, this));
}

WallpaperManager::PendingWallpaper::~PendingWallpaper() {}

void WallpaperManager::PendingWallpaper::ResetSetWallpaperImage(
    const gfx::ImageSkia& user_wallpaper,
    const WallpaperInfo& info) {
  SetMode(user_wallpaper, info, base::FilePath(), false);
}

void WallpaperManager::PendingWallpaper::ResetLoadWallpaper(
    const WallpaperInfo& info) {
  SetMode(gfx::ImageSkia(), info, base::FilePath(), false);
}

void WallpaperManager::PendingWallpaper::ResetSetCustomWallpaper(
    const WallpaperInfo& info,
    const base::FilePath& wallpaper_path) {
  SetMode(gfx::ImageSkia(), info, wallpaper_path, false);
}

void WallpaperManager::PendingWallpaper::ResetSetDefaultWallpaper() {
  SetMode(gfx::ImageSkia(), WallpaperInfo(), base::FilePath(), true);
}

void WallpaperManager::PendingWallpaper::SetMode(
    const gfx::ImageSkia& user_wallpaper,
    const WallpaperInfo& info,
    const base::FilePath& wallpaper_path,
    const bool is_default) {
  user_wallpaper_ = user_wallpaper;
  info_ = info;
  wallpaper_path_ = wallpaper_path;
  default_ = is_default;
}

void WallpaperManager::PendingWallpaper::ProcessRequest() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  timer.Stop();  // Erase reference to self.

  WallpaperManager* manager = WallpaperManager::Get();
  if (manager->pending_inactive_ == this)
    manager->pending_inactive_ = NULL;

  started_load_at_ = base::Time::Now();

  if (default_) {
    manager->DoSetDefaultWallpaper(user_id_, on_finish_.Pass());
  } else if (!user_wallpaper_.isNull()) {
    ash::Shell::GetInstance()
        ->desktop_background_controller()
        ->SetWallpaperImage(user_wallpaper_, info_.layout);
  } else if (!wallpaper_path_.empty()) {
    manager->task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&WallpaperManager::GetCustomWallpaperInternal,
                   base::Unretained(manager),
                   user_id_,
                   info_,
                   wallpaper_path_,
                   true /* update wallpaper */,
                   base::Passed(on_finish_.Pass())));
  } else if (!info_.file.empty()) {
    manager->LoadWallpaper(user_id_, info_, true, on_finish_.Pass());
  } else {
    // PendingWallpaper was created and never initialized?
    NOTREACHED();
    // Error. Do not record time.
    started_load_at_ = base::Time();
  }
  on_finish_.reset();
}

void WallpaperManager::PendingWallpaper::OnWallpaperSet() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // The only known case for this check to fail is global destruction during
  // wallpaper load. It should never happen.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI))
    return; // We are in a process of global destruction.

  timer.Stop();  // Erase reference to self.

  WallpaperManager* manager = WallpaperManager::Get();
  if (!started_load_at_.is_null()) {
    const base::TimeDelta elapsed = base::Time::Now() - started_load_at_;
    manager->SaveLastLoadTime(elapsed);
  }
  if (manager->pending_inactive_ == this) {
    // ProcessRequest() was never executed.
    manager->pending_inactive_ = NULL;
  }

  // Destroy self.
  DCHECK(manager->loading_.size() > 0);

  for (WallpaperManager::PendingList::iterator i = manager->loading_.begin();
       i != manager->loading_.end();
       ++i)
    if (i->get() == this) {
      manager->loading_.erase(i);
      break;
    }
}

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

bool WallpaperManager::TestApi::GetWallpaperFromCache(
    const std::string& user_id, gfx::ImageSkia* image) {
  return wallpaper_manager_->GetWallpaperFromCache(user_id, image);
}

void WallpaperManager::TestApi::SetWallpaperCache(const std::string& user_id,
                                                  const gfx::ImageSkia& image) {
  DCHECK(!image.isNull());
  wallpaper_manager_->wallpaper_cache_[user_id] = image;
}

void WallpaperManager::TestApi::ClearDisposableWallpaperCache() {
  wallpaper_manager_->ClearDisposableWallpaperCache();
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
      weak_factory_(this),
      pending_inactive_(NULL) {
  SetDefaultWallpaperPathsFromCommandLine(
      base::CommandLine::ForCurrentProcess());
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
  SetUserWallpaperNow(UserManager::Get()->GetLoggedInUser()->email());
}

void WallpaperManager::ClearDisposableWallpaperCache() {
  // Cancel callback for previous cache requests.
  weak_factory_.InvalidateWeakPtrs();
  if (!UserManager::IsMultipleProfilesAllowed()) {
    wallpaper_cache_.clear();
  } else {
    // Keep the wallpaper of logged in users in cache at multi-profile mode.
    std::set<std::string> logged_in_users_names;
    const UserList& logged_users = UserManager::Get()->GetLoggedInUsers();
    for (UserList::const_iterator it = logged_users.begin();
         it != logged_users.end();
         ++it) {
      logged_in_users_names.insert((*it)->email());
    }

    CustomWallpaperMap logged_in_users_cache;
    for (CustomWallpaperMap::iterator it = wallpaper_cache_.begin();
         it != wallpaper_cache_.end(); ++it) {
      if (logged_in_users_names.find(it->first) !=
          logged_in_users_names.end()) {
        logged_in_users_cache.insert(*it);
      }
    }
    wallpaper_cache_ = logged_in_users_cache;
  }
}

base::FilePath WallpaperManager::GetCustomWallpaperPath(
    const char* sub_dir,
    const std::string& user_id_hash,
    const std::string& file) const {
  base::FilePath custom_wallpaper_path = GetCustomWallpaperDir(sub_dir);
  return custom_wallpaper_path.Append(user_id_hash).Append(file);
}

base::FilePath WallpaperManager::GetOriginalWallpaperPathForUser(
    const std::string& user_id) {
  std::string filename = user_id + kOriginalCustomWallpaperSuffix;
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

  CommandLine* command_line = GetCommandLine();
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
      SetDefaultWallpaperDelayed(UserManager::kSignInUser);
    else
      InitializeRegisteredDeviceWallpaper();
    return;
  }
  SetUserWallpaperDelayed(user_manager->GetLoggedInUser()->email());
}

void WallpaperManager::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_USER_CHANGED: {
      ClearDisposableWallpaperCache();
      BrowserThread::PostDelayedTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&WallpaperManager::MoveLoggedInUserCustomWallpaper,
                     weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromSeconds(kMoveCustomWallpaperDelaySeconds));
      break;
    }
    case chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE: {
      if (!GetCommandLine()->HasSwitch(switches::kDisableBootAnimation)) {
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

void WallpaperManager::RemoveUserWallpaperInfo(const std::string& user_id) {
  WallpaperInfo info;
  GetUserWallpaperInfo(user_id, &info);
  PrefService* prefs = g_browser_process->local_state();
  DictionaryPrefUpdate prefs_wallpapers_info_update(prefs,
      prefs::kUsersWallpaperInfo);
  prefs_wallpapers_info_update->RemoveWithoutPathExpansion(user_id, NULL);
  DeleteUserWallpapers(user_id, info.file);
}

bool WallpaperManager::ResizeWallpaper(
    const UserImage& wallpaper,
    ash::WallpaperLayout layout,
    int preferred_width,
    int preferred_height,
    scoped_refptr<base::RefCountedBytes>* output,
    gfx::ImageSkia* output_skia) const {
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

  if (output_skia) {
    resized_image.MakeThreadSafe();
    *output_skia = resized_image;
  }

  return true;
}

bool WallpaperManager::ResizeAndSaveWallpaper(
    const UserImage& wallpaper,
    const base::FilePath& path,
    ash::WallpaperLayout layout,
    int preferred_width,
    int preferred_height,
    gfx::ImageSkia* result_out) const {
  if (layout == ash::WALLPAPER_LAYOUT_CENTER) {
    // TODO(bshe): Generates cropped custom wallpaper for CENTER layout.
    if (base::PathExists(path))
      base::DeleteFile(path, false);
    return false;
  }
  scoped_refptr<base::RefCountedBytes> data;
  if (ResizeWallpaper(wallpaper,
                      layout,
                      preferred_width,
                      preferred_height,
                      &data,
                      result_out)) {
    return SaveWallpaperInternal(
        path, reinterpret_cast<const char*>(data->front()), data->size());
  }
  return false;
}

bool WallpaperManager::IsPolicyControlled(const std::string& user_id) const {
  chromeos::WallpaperInfo info;
  if (!GetUserWallpaperInfo(user_id, &info))
    return false;
  return info.type == chromeos::User::POLICY;
}

void WallpaperManager::OnPolicySet(const std::string& policy,
                                   const std::string& user_id) {
  WallpaperInfo info;
  GetUserWallpaperInfo(user_id, &info);
  info.type = User::POLICY;
  SetUserWallpaperInfo(user_id, info, true /* is_persistent */);
}

void WallpaperManager::OnPolicyCleared(const std::string& policy,
                                       const std::string& user_id) {
  WallpaperInfo info;
  GetUserWallpaperInfo(user_id, &info);
  info.type = User::DEFAULT;
  SetUserWallpaperInfo(user_id, info, true /* is_persistent */);
  SetDefaultWallpaperNow(user_id);
}

void WallpaperManager::OnPolicyFetched(const std::string& policy,
                                       const std::string& user_id,
                                       scoped_ptr<std::string> data) {
  if (!data)
    return;

  wallpaper_loader_->Start(
      data.Pass(),
      0,  // Do not crop.
      base::Bind(&WallpaperManager::SetPolicyControlledWallpaper,
                 weak_factory_.GetWeakPtr(),
                 user_id));
}

// static
WallpaperManager::WallpaperResolution
WallpaperManager::GetAppropriateResolution() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  gfx::Size size =
      ash::DesktopBackgroundController::GetMaxDisplaySizeInNative();
  return (size.width() > kSmallWallpaperMaxWidth ||
          size.height() > kSmallWallpaperMaxHeight)
             ? WALLPAPER_RESOLUTION_LARGE
             : WALLPAPER_RESOLUTION_SMALL;
}

void WallpaperManager::SetPolicyControlledWallpaper(
    const std::string& user_id,
    const UserImage& wallpaper) {
  const User *user = chromeos::UserManager::Get()->FindUser(user_id);
  if (!user) {
    NOTREACHED() << "Unknown user.";
    return;
  }
  SetCustomWallpaper(user_id,
                     user->username_hash(),
                     "policy-controlled.jpeg",
                     ash::WALLPAPER_LAYOUT_CENTER_CROPPED,
                     User::POLICY,
                     wallpaper,
                     true /* update wallpaper */);
}

void WallpaperManager::SetCustomWallpaper(const std::string& user_id,
                                          const std::string& user_id_hash,
                                          const std::string& file,
                                          ash::WallpaperLayout layout,
                                          User::WallpaperType type,
                                          const UserImage& wallpaper,
                                          bool update_wallpaper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(UserManager::Get()->IsUserLoggedIn());

  // There is no visible background in kiosk mode.
  if (UserManager::Get()->IsLoggedInAsKioskApp())
    return;

  // Don't allow custom wallpapers while policy is in effect.
  if (type != User::POLICY && IsPolicyControlled(user_id))
    return;

  base::FilePath wallpaper_path =
      GetCustomWallpaperPath(kOriginalWallpaperSubDir, user_id_hash, file);

  // If decoded wallpaper is empty, we have probably failed to decode the file.
  // Use default wallpaper in this case.
  if (wallpaper.image().isNull()) {
    SetDefaultWallpaperDelayed(user_id);
    return;
  }

  bool is_persistent =
      !UserManager::Get()->IsUserNonCryptohomeDataEphemeral(user_id);

  wallpaper.image().EnsureRepsForSupportedScales();
  scoped_ptr<gfx::ImageSkia> deep_copy(wallpaper.image().DeepCopy());

  WallpaperInfo wallpaper_info = {
      wallpaper_path.value(),
      layout,
      type,
      // Date field is not used.
      base::Time::Now().LocalMidnight()
  };
  // Block shutdown on this task. Otherwise, we may lose the custom wallpaper
  // that the user selected.
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

  std::string relative_path = base::FilePath(user_id_hash).Append(file).value();
  // User's custom wallpaper path is determined by relative path and the
  // appropriate wallpaper resolution in GetCustomWallpaperInternal.
  WallpaperInfo info = {
      relative_path,
      layout,
      type,
      base::Time::Now().LocalMidnight()
  };
  SetUserWallpaperInfo(user_id, info, is_persistent);
  if (update_wallpaper) {
    GetPendingWallpaper(user_id, false)
        ->ResetSetWallpaperImage(wallpaper.image(), info);
  }

  if (UserManager::IsMultipleProfilesAllowed())
    wallpaper_cache_[user_id] = wallpaper.image();
}

void WallpaperManager::SetDefaultWallpaperNow(const std::string& user_id) {
  GetPendingWallpaper(user_id, false)->ResetSetDefaultWallpaper();
}

void WallpaperManager::SetDefaultWallpaperDelayed(const std::string& user_id) {
  GetPendingWallpaper(user_id, true)->ResetSetDefaultWallpaper();
}

void WallpaperManager::DoSetDefaultWallpaper(
    const std::string& user_id,
    MovableOnDestroyCallbackHolder on_finish) {
  // There is no visible background in kiosk mode.
  if (UserManager::Get()->IsLoggedInAsKioskApp())
    return;
  current_wallpaper_path_.clear();
  wallpaper_cache_.erase(user_id);
  // Some browser tests do not have a shell instance. As no wallpaper is needed
  // in these tests anyway, avoid loading one, preventing crashes and speeding
  // up the tests.
  if (!ash::Shell::HasInstance())
    return;

  WallpaperResolution resolution = GetAppropriateResolution();
  const bool use_small = (resolution == WALLPAPER_RESOLUTION_SMALL);

  const base::FilePath* file = NULL;

  if (UserManager::Get()->IsLoggedInAsGuest()) {
    file =
        use_small ? &guest_small_wallpaper_file_ : &guest_large_wallpaper_file_;
  } else {
    file = use_small ? &default_small_wallpaper_file_
                     : &default_large_wallpaper_file_;
  }
  const ash::WallpaperLayout layout =
      use_small ? ash::WALLPAPER_LAYOUT_CENTER
                : ash::WALLPAPER_LAYOUT_CENTER_CROPPED;
  DCHECK(file);
  if (!default_wallpaper_image_.get() ||
      default_wallpaper_image_->url().spec() != file->value()) {
    default_wallpaper_image_.reset();
    if (!file->empty()) {
      loaded_wallpapers_++;
      StartLoadAndSetDefaultWallpaper(
          *file, layout, on_finish.Pass(), &default_wallpaper_image_);
      return;
    }

    const int resource_id = use_small ? IDR_AURA_WALLPAPER_DEFAULT_SMALL
                                      : IDR_AURA_WALLPAPER_DEFAULT_LARGE;

    loaded_wallpapers_ += ash::Shell::GetInstance()
                              ->desktop_background_controller()
                              ->SetWallpaperResource(resource_id, layout);
    return;
  }
  ash::Shell::GetInstance()->desktop_background_controller()->SetWallpaperImage(
      default_wallpaper_image_->image(), layout);
}

void WallpaperManager::InitInitialUserWallpaper(const std::string& user_id,
                                                bool is_persistent) {
  current_user_wallpaper_info_.file = "";
  current_user_wallpaper_info_.layout = ash::WALLPAPER_LAYOUT_CENTER_CROPPED;
  current_user_wallpaper_info_.type = User::DEFAULT;
  current_user_wallpaper_info_.date = base::Time::Now().LocalMidnight();

  WallpaperInfo info = current_user_wallpaper_info_;
  SetUserWallpaperInfo(user_id, info, is_persistent);
}

void WallpaperManager::SetUserWallpaperInfo(const std::string& user_id,
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
  wallpaper_update->SetWithoutPathExpansion(user_id, wallpaper_info_dict);
}

void WallpaperManager::SetLastSelectedUser(
    const std::string& last_selected_user) {
  last_selected_user_ = last_selected_user;
}

void WallpaperManager::SetUserWallpaperDelayed(const std::string& user_id) {
  ScheduleSetUserWallpaper(user_id, true);
}

void WallpaperManager::SetUserWallpaperNow(const std::string& user_id) {
  ScheduleSetUserWallpaper(user_id, false);
}

void WallpaperManager::ScheduleSetUserWallpaper(const std::string& user_id,
                                                bool delayed) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Some unit tests come here without a UserManager or without a pref system.
  if (!UserManager::IsInitialized() || !g_browser_process->local_state())
    return;
  // There is no visible background in kiosk mode.
  if (UserManager::Get()->IsLoggedInAsKioskApp())
    return;
  // Guest user, regular user in ephemeral mode, or kiosk app.
  const User* user = UserManager::Get()->FindUser(user_id);
  if (UserManager::Get()->IsUserNonCryptohomeDataEphemeral(user_id) ||
      (user != NULL && user->GetType() == User::USER_TYPE_KIOSK_APP)) {
    InitInitialUserWallpaper(user_id, false);
    GetPendingWallpaper(user_id, delayed)->ResetSetDefaultWallpaper();
    return;
  }

  if (!UserManager::Get()->IsKnownUser(user_id))
    return;

  SetLastSelectedUser(user_id);

  WallpaperInfo info;

  if (!GetUserWallpaperInfo(user_id, &info)) {
    InitInitialUserWallpaper(user_id, true);
    GetUserWallpaperInfo(user_id, &info);
  }

  gfx::ImageSkia user_wallpaper;
  current_user_wallpaper_info_ = info;
  if (GetWallpaperFromCache(user_id, &user_wallpaper)) {
    GetPendingWallpaper(user_id, delayed)
        ->ResetSetWallpaperImage(user_wallpaper, info);
  } else {
    if (info.type == User::CUSTOMIZED || info.type == User::POLICY) {
      const char* sub_dir = GetCustomWallpaperSubdirForCurrentResolution();
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

      GetPendingWallpaper(user_id, delayed)
          ->ResetSetCustomWallpaper(info, wallpaper_path);
      return;
    }

    if (info.file.empty()) {
      // Uses default built-in wallpaper when file is empty. Eventually, we
      // will only ship one built-in wallpaper in ChromeOS image.
      GetPendingWallpaper(user_id, delayed)->ResetSetDefaultWallpaper();
      return;
    }

    // Load downloaded ONLINE or converted DEFAULT wallpapers.
    GetPendingWallpaper(user_id, delayed)->ResetLoadWallpaper(info);
  }
}

void WallpaperManager::SetWallpaperFromImageSkia(
    const std::string& user_id,
    const gfx::ImageSkia& wallpaper,
    ash::WallpaperLayout layout,
    bool update_wallpaper) {
  DCHECK(UserManager::Get()->IsUserLoggedIn());

  // There is no visible background in kiosk mode.
  if (UserManager::Get()->IsLoggedInAsKioskApp())
    return;
  WallpaperInfo info;
  info.layout = layout;
  if (UserManager::IsMultipleProfilesAllowed())
    wallpaper_cache_[user_id] = wallpaper;

  if (update_wallpaper) {
    GetPendingWallpaper(last_selected_user_, false /* Not delayed */)
        ->ResetSetWallpaperImage(wallpaper, info);
  }
}

void WallpaperManager::UpdateWallpaper(bool clear_cache) {
  FOR_EACH_OBSERVER(Observer, observers_, OnUpdateWallpaperForTesting());
  if (clear_cache)
    wallpaper_cache_.clear();
  current_wallpaper_path_.clear();
  // For GAIA login flow, the last_selected_user_ may not be set before user
  // login. If UpdateWallpaper is called at GAIA login screen, no wallpaper will
  // be set. It could result a black screen on external monitors.
  // See http://crbug.com/265689 for detail.
  if (last_selected_user_.empty()) {
    SetDefaultWallpaperNow(UserManager::kSignInUser);
    return;
  }
  SetUserWallpaperNow(last_selected_user_);
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

bool WallpaperManager::GetWallpaperFromCache(const std::string& user_id,
                                             gfx::ImageSkia* wallpaper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CustomWallpaperMap::const_iterator it = wallpaper_cache_.find(user_id);
  if (it != wallpaper_cache_.end()) {
    *wallpaper = (*it).second;
    return true;
  }
  return false;
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
      std::string user_id = (*it)->email();
      CacheUserWallpaper(user_id);
    }
  }
}

void WallpaperManager::CacheUserWallpaper(const std::string& user_id) {
  if (wallpaper_cache_.find(user_id) != wallpaper_cache_.end())
    return;
  WallpaperInfo info;
  if (GetUserWallpaperInfo(user_id, &info)) {
    base::FilePath wallpaper_dir;
    base::FilePath wallpaper_path;
    if (info.type == User::CUSTOMIZED || info.type == User::POLICY) {
      const char* sub_dir = GetCustomWallpaperSubdirForCurrentResolution();
      base::FilePath wallpaper_path = GetCustomWallpaperDir(sub_dir);
      wallpaper_path = wallpaper_path.Append(info.file);
      task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&WallpaperManager::GetCustomWallpaperInternal,
                     base::Unretained(this),
                     user_id,
                     info,
                     wallpaper_path,
                     false /* do not update wallpaper */,
                     base::Passed(MovableOnDestroyCallbackHolder())));
      return;
    }
    LoadWallpaper(user_id,
                  info,
                  false /* do not update wallpaper */,
                  MovableOnDestroyCallbackHolder().Pass());
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

void WallpaperManager::DeleteUserWallpapers(const std::string& user_id,
                                            const std::string& path_to_file) {
  std::vector<base::FilePath> file_to_remove;
  // Remove small user wallpaper.
  base::FilePath wallpaper_path =
      GetCustomWallpaperDir(kSmallWallpaperSubDir);
  // Remove old directory if exists
  file_to_remove.push_back(wallpaper_path.Append(user_id));
  wallpaper_path = wallpaper_path.Append(path_to_file).DirName();
  file_to_remove.push_back(wallpaper_path);

  // Remove large user wallpaper.
  wallpaper_path = GetCustomWallpaperDir(kLargeWallpaperSubDir);
  file_to_remove.push_back(wallpaper_path.Append(user_id));
  wallpaper_path = wallpaper_path.Append(path_to_file);
  file_to_remove.push_back(wallpaper_path);

  // Remove user wallpaper thumbnail.
  wallpaper_path = GetCustomWallpaperDir(kThumbnailWallpaperSubDir);
  file_to_remove.push_back(wallpaper_path.Append(user_id));
  wallpaper_path = wallpaper_path.Append(path_to_file);
  file_to_remove.push_back(wallpaper_path);

  // Remove original user wallpaper.
  wallpaper_path = GetCustomWallpaperDir(kOriginalWallpaperSubDir);
  file_to_remove.push_back(wallpaper_path.Append(user_id));
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
    base::CreateDirectory(dir);
  dir = GetCustomWallpaperDir(kLargeWallpaperSubDir);
  dir = dir.Append(user_id_hash);
  if (!base::PathExists(dir))
    base::CreateDirectory(dir);
  dir = GetCustomWallpaperDir(kOriginalWallpaperSubDir);
  dir = dir.Append(user_id_hash);
  if (!base::PathExists(dir))
    base::CreateDirectory(dir);
  dir = GetCustomWallpaperDir(kThumbnailWallpaperSubDir);
  dir = dir.Append(user_id_hash);
  if (!base::PathExists(dir))
    base::CreateDirectory(dir);
}

void WallpaperManager::SetCommandLineForTesting(
    base::CommandLine* command_line) {
  command_line_for_testing_ = command_line;
  SetDefaultWallpaperPathsFromCommandLine(command_line);
}

CommandLine* WallpaperManager::GetCommandLine() {
  CommandLine* command_line = command_line_for_testing_ ?
      command_line_for_testing_ : CommandLine::ForCurrentProcess();
  return command_line;
}

void WallpaperManager::InitializeRegisteredDeviceWallpaper() {
  if (UserManager::Get()->IsUserLoggedIn())
    return;

  bool disable_boot_animation =
      GetCommandLine()->HasSwitch(switches::kDisableBootAnimation);
  bool show_users = true;
  bool result = CrosSettings::Get()->GetBoolean(
      kAccountsPrefShowUserNamesOnSignIn, &show_users);
  DCHECK(result) << "Unable to fetch setting "
                 << kAccountsPrefShowUserNamesOnSignIn;
  const chromeos::UserList& users = UserManager::Get()->GetUsers();
  if (!show_users || users.empty()) {
    // Boot into sign in form, preload default wallpaper.
    SetDefaultWallpaperDelayed(UserManager::kSignInUser);
    return;
  }

  if (!disable_boot_animation) {
    // Normal boot, load user wallpaper.
    // If normal boot animation is disabled wallpaper would be set
    // asynchronously once user pods are loaded.
    SetUserWallpaperDelayed(users[0]->email());
  }
}

void WallpaperManager::LoadWallpaper(const std::string& user_id,
                                     const WallpaperInfo& info,
                                     bool update_wallpaper,
                                     MovableOnDestroyCallbackHolder on_finish) {
  base::FilePath wallpaper_dir;
  base::FilePath wallpaper_path;
  if (info.type == User::ONLINE) {
    std::string file_name = GURL(info.file).ExtractFileName();
    WallpaperResolution resolution = GetAppropriateResolution();
    // Only solid color wallpapers have stretch layout and they have only one
    // resolution.
    if (info.layout != ash::WALLPAPER_LAYOUT_STRETCH &&
        resolution == WALLPAPER_RESOLUTION_SMALL) {
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
    StartLoad(
        user_id, info, update_wallpaper, wallpaper_path, on_finish.Pass());
  } else if (info.type == User::DEFAULT) {
    // Default wallpapers are migrated from M21 user profiles. A code refactor
    // overlooked that case and caused these wallpapers not being loaded at all.
    // On some slow devices, it caused login webui not visible after upgrade to
    // M26 from M21. See crosbug.com/38429 for details.
    base::FilePath user_data_dir;
    PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    wallpaper_path = user_data_dir.Append(info.file);
    StartLoad(
        user_id, info, update_wallpaper, wallpaper_path, on_finish.Pass());
  } else {
    // In unexpected cases, revert to default wallpaper to fail safely. See
    // crosbug.com/38429.
    LOG(ERROR) << "Wallpaper reverts to default unexpected.";
    DoSetDefaultWallpaper(user_id, on_finish.Pass());
  }
}

bool WallpaperManager::GetUserWallpaperInfo(const std::string& user_id,
                                            WallpaperInfo* info) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (UserManager::Get()->IsUserNonCryptohomeDataEphemeral(user_id)) {
    // Default to the values cached in memory.
    *info = current_user_wallpaper_info_;

    // Ephemeral users do not save anything to local state. But we have got
    // wallpaper info from memory. Returns true.
    return true;
  }

  const base::DictionaryValue* info_dict;
  if (!g_browser_process->local_state()->
          GetDictionary(prefs::kUsersWallpaperInfo)->
              GetDictionaryWithoutPathExpansion(user_id, &info_dict)) {
    return false;
  }

  // Use temporary variables to keep |info| untouched in the error case.
  std::string file;
  if (!info_dict->GetString(kNewWallpaperFileNodeName, &file))
    return false;
  int layout;
  if (!info_dict->GetInteger(kNewWallpaperLayoutNodeName, &layout))
    return false;
  int type;
  if (!info_dict->GetInteger(kNewWallpaperTypeNodeName, &type))
    return false;
  std::string date_string;
  if (!info_dict->GetString(kNewWallpaperDateNodeName, &date_string))
    return false;
  int64 date_val;
  if (!base::StringToInt64(date_string, &date_val))
    return false;

  info->file = file;
  info->layout = static_cast<ash::WallpaperLayout>(layout);
  info->type = static_cast<User::WallpaperType>(type);
  info->date = base::Time::FromInternalValue(date_val);
  return true;
}

void WallpaperManager::MoveCustomWallpapersOnWorker(
    const std::string& user_id,
    const std::string& user_id_hash) {
  DCHECK(BrowserThread::GetBlockingPool()->
      IsRunningSequenceOnCurrentThread(sequence_token_));
  if (MoveCustomWallpaperDirectory(
          kOriginalWallpaperSubDir, user_id, user_id_hash)) {
    // Consider success if the original wallpaper is moved to the new directory.
    // Original wallpaper is the fallback if the correct resolution wallpaper
    // can not be found.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&WallpaperManager::MoveCustomWallpapersSuccess,
                   base::Unretained(this),
                   user_id,
                   user_id_hash));
  }
  MoveCustomWallpaperDirectory(kLargeWallpaperSubDir, user_id, user_id_hash);
  MoveCustomWallpaperDirectory(kSmallWallpaperSubDir, user_id, user_id_hash);
  MoveCustomWallpaperDirectory(
      kThumbnailWallpaperSubDir, user_id, user_id_hash);
}

void WallpaperManager::MoveCustomWallpapersSuccess(
    const std::string& user_id,
    const std::string& user_id_hash) {
  WallpaperInfo info;
  GetUserWallpaperInfo(user_id, &info);
  if (info.type == User::CUSTOMIZED) {
    // New file field should include user id hash in addition to file name.
    // This is needed because at login screen, user id hash is not available.
    std::string relative_path =
        base::FilePath(user_id_hash).Append(info.file).value();
    info.file = relative_path;
    bool is_persistent =
        !UserManager::Get()->IsUserNonCryptohomeDataEphemeral(user_id);
    SetUserWallpaperInfo(user_id, info, is_persistent);
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
    const std::string& user_id,
    const WallpaperInfo& info,
    const base::FilePath& wallpaper_path,
    bool update_wallpaper,
    MovableOnDestroyCallbackHolder on_finish) {
  DCHECK(BrowserThread::GetBlockingPool()->
      IsRunningSequenceOnCurrentThread(sequence_token_));

  base::FilePath valid_path = wallpaper_path;
  if (!base::PathExists(wallpaper_path)) {
    // Falls back on original file if the correct resolution file does not
    // exist. This may happen when the original custom wallpaper is small or
    // browser shutdown before resized wallpaper saved.
    valid_path = GetCustomWallpaperDir(kOriginalWallpaperSubDir);
    valid_path = valid_path.Append(info.file);
  }

  if (!base::PathExists(valid_path)) {
    // Falls back to custom wallpaper that uses email as part of its file path.
    // Note that email is used instead of user_id_hash here.
    valid_path =
        GetCustomWallpaperPath(kOriginalWallpaperSubDir, user_id, info.file);
  }

  if (!base::PathExists(valid_path)) {
    LOG(ERROR) << "Failed to load previously selected custom wallpaper. " <<
                  "Fallback to default wallpaper";
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&WallpaperManager::DoSetDefaultWallpaper,
                                       base::Unretained(this),
                                       user_id,
                                       base::Passed(on_finish.Pass())));
  } else {
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&WallpaperManager::StartLoad,
                                       base::Unretained(this),
                                       user_id,
                                       info,
                                       update_wallpaper,
                                       valid_path,
                                       base::Passed(on_finish.Pass())));
  }
}

void WallpaperManager::OnWallpaperDecoded(
    const std::string& user_id,
    ash::WallpaperLayout layout,
    bool update_wallpaper,
    MovableOnDestroyCallbackHolder on_finish,
    const UserImage& wallpaper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TRACE_EVENT_ASYNC_END0("ui", "LoadAndDecodeWallpaper", this);

  // If decoded wallpaper is empty, we have probably failed to decode the file.
  // Use default wallpaper in this case.
  if (wallpaper.image().isNull()) {
    // Updates user pref to default wallpaper.
    WallpaperInfo info = {
                           "",
                           ash::WALLPAPER_LAYOUT_CENTER_CROPPED,
                           User::DEFAULT,
                           base::Time::Now().LocalMidnight()
                         };
    SetUserWallpaperInfo(user_id, info, true);

    if (update_wallpaper)
      DoSetDefaultWallpaper(user_id, on_finish.Pass());
    return;
  }

  // Only cache the user wallpaper at login screen and for multi profile users.
  if (!UserManager::Get()->IsUserLoggedIn() ||
      UserManager::IsMultipleProfilesAllowed()) {
    wallpaper_cache_[user_id] = wallpaper.image();
  }

  if (update_wallpaper) {
    ash::Shell::GetInstance()
        ->desktop_background_controller()
        ->SetWallpaperImage(wallpaper.image(), layout);
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
  ResizeAndSaveWallpaper(wallpaper,
                         original_path,
                         ash::WALLPAPER_LAYOUT_STRETCH,
                         wallpaper.image().width(),
                         wallpaper.image().height(),
                         NULL);
  DeleteAllExcept(original_path);

  ResizeAndSaveWallpaper(wallpaper,
                         small_wallpaper_path,
                         layout,
                         kSmallWallpaperMaxWidth,
                         kSmallWallpaperMaxHeight,
                         NULL);
  DeleteAllExcept(small_wallpaper_path);
  ResizeAndSaveWallpaper(wallpaper,
                         large_wallpaper_path,
                         layout,
                         kLargeWallpaperMaxWidth,
                         kLargeWallpaperMaxHeight,
                         NULL);
  DeleteAllExcept(large_wallpaper_path);
}

void WallpaperManager::RecordUma(User::WallpaperType type, int index) const {
  UMA_HISTOGRAM_ENUMERATION("Ash.Wallpaper.Type", type,
                            User::WALLPAPER_TYPE_COUNT);
}

bool WallpaperManager::SaveWallpaperInternal(const base::FilePath& path,
                                             const char* data,
                                             int size) const {
  int written_bytes = base::WriteFile(path, data, size);
  return written_bytes == size;
}

void WallpaperManager::StartLoad(const std::string& user_id,
                                 const WallpaperInfo& info,
                                 bool update_wallpaper,
                                 const base::FilePath& wallpaper_path,
                                 MovableOnDestroyCallbackHolder on_finish) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TRACE_EVENT_ASYNC_BEGIN0("ui", "LoadAndDecodeWallpaper", this);

  wallpaper_loader_->Start(wallpaper_path.value(),
                           0,  // Do not crop.
                           base::Bind(&WallpaperManager::OnWallpaperDecoded,
                                      base::Unretained(this),
                                      user_id,
                                      info.layout,
                                      update_wallpaper,
                                      base::Passed(on_finish.Pass())));
}

void WallpaperManager::SaveLastLoadTime(const base::TimeDelta elapsed) {
  while (last_load_times_.size() >= kLastLoadsStatsMsMaxSize)
    last_load_times_.pop_front();

  if (elapsed > base::TimeDelta::FromMicroseconds(0)) {
    last_load_times_.push_back(elapsed);
    last_load_finished_at_ = base::Time::Now();
  }
}

base::TimeDelta WallpaperManager::GetWallpaperLoadDelay() const {
  base::TimeDelta delay;

  if (last_load_times_.size() == 0) {
    delay = base::TimeDelta::FromMilliseconds(kLoadDefaultDelayMs);
  } else {
    delay = std::accumulate(last_load_times_.begin(),
                            last_load_times_.end(),
                            base::TimeDelta(),
                            std::plus<base::TimeDelta>()) /
            last_load_times_.size();
  }

  if (delay < base::TimeDelta::FromMilliseconds(kLoadMinDelayMs))
    delay = base::TimeDelta::FromMilliseconds(kLoadMinDelayMs);
  else if (delay > base::TimeDelta::FromMilliseconds(kLoadMaxDelayMs))
    delay = base::TimeDelta::FromMilliseconds(kLoadMaxDelayMs);

  // If we had ever loaded wallpaper, adjust wait delay by time since last load.
  if (!last_load_finished_at_.is_null()) {
    const base::TimeDelta interval = base::Time::Now() - last_load_finished_at_;
    if (interval > delay)
      delay = base::TimeDelta::FromMilliseconds(0);
    else if (interval > base::TimeDelta::FromMilliseconds(0))
      delay -= interval;
  }
  return delay;
}

WallpaperManager::PendingWallpaper* WallpaperManager::GetPendingWallpaper(
    const std::string& user_id,
    bool delayed) {
  if (!pending_inactive_) {
    loading_.push_back(new WallpaperManager::PendingWallpaper(
        (delayed ? GetWallpaperLoadDelay()
                 : base::TimeDelta::FromMilliseconds(0)),
        user_id));
    pending_inactive_ = loading_.back();
  }
  return pending_inactive_;
}

void WallpaperManager::SetDefaultWallpaperPathsFromCommandLine(
    base::CommandLine* command_line) {
  default_small_wallpaper_file_ = command_line->GetSwitchValuePath(
      ash::switches::kAshDefaultWallpaperSmall);
  default_large_wallpaper_file_ = command_line->GetSwitchValuePath(
      ash::switches::kAshDefaultWallpaperLarge);
  guest_small_wallpaper_file_ =
      command_line->GetSwitchValuePath(ash::switches::kAshGuestWallpaperSmall);
  guest_large_wallpaper_file_ =
      command_line->GetSwitchValuePath(ash::switches::kAshGuestWallpaperLarge);
  default_wallpaper_image_.reset();
}

void WallpaperManager::OnDefaultWallpaperDecoded(
    const base::FilePath& path,
    const ash::WallpaperLayout layout,
    scoped_ptr<chromeos::UserImage>* result_out,
    MovableOnDestroyCallbackHolder on_finish,
    const UserImage& wallpaper) {
  result_out->reset(new UserImage(wallpaper.image()));
  (*result_out)->set_url(GURL(path.value()));
  ash::Shell::GetInstance()->desktop_background_controller()->SetWallpaperImage(
      wallpaper.image(), layout);
}

void WallpaperManager::StartLoadAndSetDefaultWallpaper(
    const base::FilePath& path,
    const ash::WallpaperLayout layout,
    MovableOnDestroyCallbackHolder on_finish,
    scoped_ptr<chromeos::UserImage>* result_out) {
  wallpaper_loader_->Start(
      path.value(),
      0,  // Do not crop.
      base::Bind(&WallpaperManager::OnDefaultWallpaperDecoded,
                 weak_factory_.GetWeakPtr(),
                 path,
                 layout,
                 base::Unretained(result_out),
                 base::Passed(on_finish.Pass())));
}

const char* WallpaperManager::GetCustomWallpaperSubdirForCurrentResolution() {
  WallpaperResolution resolution = GetAppropriateResolution();
  return resolution == WALLPAPER_RESOLUTION_SMALL ? kSmallWallpaperSubDir
                                                  : kLargeWallpaperSubDir;
}

}  // namespace chromeos
