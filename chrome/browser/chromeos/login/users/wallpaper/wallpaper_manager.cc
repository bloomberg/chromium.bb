// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"

#include <numeric>
#include <vector>

#include "ash/ash_constants.h"
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
#include "base/sys_info.h"
#include "base/threading/worker_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/login/user_names.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "third_party/skia/include/core/SkColor.h"
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

// When no wallpaper image is specified, the screen is filled with a solid
// color.
const SkColor kDefaultWallpaperColor = SK_ColorGRAY;

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

// These global default values are used to set customized default
// wallpaper path in WallpaperManager::InitializeWallpaper().
base::FilePath GetCustomizedWallpaperDefaultRescaledFileName(
    const std::string& suffix) {
  const base::FilePath default_downloaded_file_name =
      ServicesCustomizationDocument::GetCustomizedWallpaperDownloadedFileName();
  const base::FilePath default_cache_dir =
      ServicesCustomizationDocument::GetCustomizedWallpaperCacheDir();
  if (default_downloaded_file_name.empty() || default_cache_dir.empty())
    return base::FilePath();
  return default_cache_dir.Append(
      default_downloaded_file_name.BaseName().value() + suffix);
}

// Whether DesktopBackgroundController should start with customized default
// wallpaper in WallpaperManager::InitializeWallpaper() or not.
bool ShouldUseCustomizedDefaultWallpaper() {
  PrefService* pref_service = g_browser_process->local_state();

  return !(pref_service->FindPreference(
                             prefs::kCustomizationDefaultWallpaperURL)
               ->IsDefaultValue());
}

// Deletes everything else except |path| in the same directory.
void DeleteAllExcept(const base::FilePath& path) {
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

// Deletes a list of wallpaper files in |file_list|.
void DeleteWallpaperInList(const std::vector<base::FilePath>& file_list) {
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

// Creates all new custom wallpaper directories for |user_id_hash| if not exist.
// static
void EnsureCustomWallpaperDirectories(const std::string& user_id_hash) {
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

// Saves wallpaper image raw |data| to |path| (absolute path) in file system.
// Returns true on success.
bool SaveWallpaperInternal(const base::FilePath& path,
                           const char* data,
                           int size) {
  int written_bytes = base::WriteFile(path, data, size);
  return written_bytes == size;
}

// Returns index of the first public session user found in |users|
// or -1 otherwise.
int FindPublicSession(const user_manager::UserList& users) {
  int index = -1;
  int i = 0;
  for (user_manager::UserList::const_iterator it = users.begin();
       it != users.end();
       ++it, ++i) {
    if ((*it)->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
      index = i;
      break;
    }
  }

  return index;
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

class WallpaperManager::CustomizedWallpaperRescaledFiles {
 public:
  CustomizedWallpaperRescaledFiles(const base::FilePath& path_downloaded,
                                   const base::FilePath& path_rescaled_small,
                                   const base::FilePath& path_rescaled_large);

  bool AllSizesExist() const;

  // Closure will hold unretained pointer to this object. So caller must
  // make sure that the closure will be destoyed before this object.
  // Closure must be called on BlockingPool.
  base::Closure CreateCheckerClosure();

  const base::FilePath& path_downloaded() const { return path_downloaded_; }
  const base::FilePath& path_rescaled_small() const {
    return path_rescaled_small_;
  }
  const base::FilePath& path_rescaled_large() const {
    return path_rescaled_large_;
  }

  const bool downloaded_exists() const { return downloaded_exists_; }
  const bool rescaled_small_exists() const { return rescaled_small_exists_; }
  const bool rescaled_large_exists() const { return rescaled_large_exists_; }

 private:
  // Must be called on BlockingPool.
  void CheckCustomizedWallpaperFilesExist();

  const base::FilePath path_downloaded_;
  const base::FilePath path_rescaled_small_;
  const base::FilePath path_rescaled_large_;

  bool downloaded_exists_;
  bool rescaled_small_exists_;
  bool rescaled_large_exists_;

  DISALLOW_COPY_AND_ASSIGN(CustomizedWallpaperRescaledFiles);
};

WallpaperManager::CustomizedWallpaperRescaledFiles::
    CustomizedWallpaperRescaledFiles(const base::FilePath& path_downloaded,
                                     const base::FilePath& path_rescaled_small,
                                     const base::FilePath& path_rescaled_large)
    : path_downloaded_(path_downloaded),
      path_rescaled_small_(path_rescaled_small),
      path_rescaled_large_(path_rescaled_large),
      downloaded_exists_(false),
      rescaled_small_exists_(false),
      rescaled_large_exists_(false) {
}

base::Closure
WallpaperManager::CustomizedWallpaperRescaledFiles::CreateCheckerClosure() {
  return base::Bind(&WallpaperManager::CustomizedWallpaperRescaledFiles::
                        CheckCustomizedWallpaperFilesExist,
                    base::Unretained(this));
}

void WallpaperManager::CustomizedWallpaperRescaledFiles::
    CheckCustomizedWallpaperFilesExist() {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  downloaded_exists_ = base::PathExists(path_downloaded_);
  rescaled_small_exists_ = base::PathExists(path_rescaled_small_);
  rescaled_large_exists_ = base::PathExists(path_rescaled_large_);
}

bool WallpaperManager::CustomizedWallpaperRescaledFiles::AllSizesExist() const {
  return rescaled_small_exists_ && rescaled_large_exists_;
}

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
    const gfx::ImageSkia& image,
    const WallpaperInfo& info) {
  SetMode(image, info, base::FilePath(), false);
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
    const gfx::ImageSkia& image,
    const WallpaperInfo& info,
    const base::FilePath& wallpaper_path,
    const bool is_default) {
  user_wallpaper_ = image;
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
                   user_id_,
                   info_,
                   wallpaper_path_,
                   true /* update wallpaper */,
                   base::Passed(on_finish_.Pass()),
                   manager->weak_factory_.GetWeakPtr()));
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
  manager->RemovePendingWallpaperFromList(this);
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
                     weak_factory_.GetWeakPtr()));
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
  // Keep the wallpaper of logged in users in cache at multi-profile mode.
  std::set<std::string> logged_in_users_names;
  const user_manager::UserList& logged_users =
      UserManager::Get()->GetLoggedInUsers();
  for (user_manager::UserList::const_iterator it = logged_users.begin();
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

bool WallpaperManager::GetLoggedInUserWallpaperInfo(WallpaperInfo* info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (UserManager::Get()->IsLoggedInAsStub()) {
    info->file = current_user_wallpaper_info_.file = "";
    info->layout = current_user_wallpaper_info_.layout =
        ash::WALLPAPER_LAYOUT_CENTER_CROPPED;
    info->type = current_user_wallpaper_info_.type =
        user_manager::User::DEFAULT;
    info->date = current_user_wallpaper_info_.date =
        base::Time::Now().LocalMidnight();
    return true;
  }

  return GetUserWallpaperInfo(UserManager::Get()->GetLoggedInUser()->email(),
                              info);
}

void WallpaperManager::InitializeWallpaper() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UserManager* user_manager = UserManager::Get();

  // Apply device customization.
  if (ShouldUseCustomizedDefaultWallpaper()) {
    SetDefaultWallpaperPath(
        GetCustomizedWallpaperDefaultRescaledFileName(kSmallWallpaperSuffix),
        scoped_ptr<gfx::ImageSkia>().Pass(),
        GetCustomizedWallpaperDefaultRescaledFileName(kLargeWallpaperSuffix),
        scoped_ptr<gfx::ImageSkia>().Pass());
  }

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
      SetDefaultWallpaperDelayed(chromeos::login::kSignInUser);
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

// static
bool WallpaperManager::ResizeImage(const gfx::ImageSkia& image,
                                   ash::WallpaperLayout layout,
                                   int preferred_width,
                                   int preferred_height,
                                   scoped_refptr<base::RefCountedBytes>* output,
                                   gfx::ImageSkia* output_skia) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  int width = image.width();
  int height = image.height();
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
      image,
      skia::ImageOperations::RESIZE_LANCZOS3,
      gfx::Size(resized_width, resized_height));

  SkBitmap bitmap = *(resized_image.bitmap());
  SkAutoLockPixels lock_input(bitmap);
  gfx::JPEGCodec::Encode(
      reinterpret_cast<unsigned char*>(bitmap.getAddr32(0, 0)),
      gfx::JPEGCodec::FORMAT_SkBitmap,
      bitmap.width(),
      bitmap.height(),
      bitmap.width() * bitmap.bytesPerPixel(),
      kDefaultEncodingQuality,
      &(*output)->data());

  if (output_skia) {
    resized_image.MakeThreadSafe();
    *output_skia = resized_image;
  }

  return true;
}

// static
bool WallpaperManager::ResizeAndSaveWallpaper(const gfx::ImageSkia& image,
                                              const base::FilePath& path,
                                              ash::WallpaperLayout layout,
                                              int preferred_width,
                                              int preferred_height,
                                              gfx::ImageSkia* output_skia) {
  if (layout == ash::WALLPAPER_LAYOUT_CENTER) {
    // TODO(bshe): Generates cropped custom wallpaper for CENTER layout.
    if (base::PathExists(path))
      base::DeleteFile(path, false);
    return false;
  }
  scoped_refptr<base::RefCountedBytes> data;
  if (ResizeImage(image,
                  layout,
                  preferred_width,
                  preferred_height,
                  &data,
                  output_skia)) {
    return SaveWallpaperInternal(
        path, reinterpret_cast<const char*>(data->front()), data->size());
  }
  return false;
}

bool WallpaperManager::IsPolicyControlled(const std::string& user_id) const {
  chromeos::WallpaperInfo info;
  if (!GetUserWallpaperInfo(user_id, &info))
    return false;
  return info.type == user_manager::User::POLICY;
}

void WallpaperManager::OnPolicySet(const std::string& policy,
                                   const std::string& user_id) {
  WallpaperInfo info;
  GetUserWallpaperInfo(user_id, &info);
  info.type = user_manager::User::POLICY;
  SetUserWallpaperInfo(user_id, info, true /* is_persistent */);
}

void WallpaperManager::OnPolicyCleared(const std::string& policy,
                                       const std::string& user_id) {
  WallpaperInfo info;
  GetUserWallpaperInfo(user_id, &info);
  info.type = user_manager::User::DEFAULT;
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

// static
base::FilePath WallpaperManager::GetCustomWallpaperPath(
    const char* sub_dir,
    const std::string& user_id_hash,
    const std::string& file) {
  base::FilePath custom_wallpaper_path = GetCustomWallpaperDir(sub_dir);
  return custom_wallpaper_path.Append(user_id_hash).Append(file);
}

void WallpaperManager::SetPolicyControlledWallpaper(
    const std::string& user_id,
    const user_manager::UserImage& user_image) {
  const user_manager::User* user =
      chromeos::UserManager::Get()->FindUser(user_id);
  if (!user) {
    NOTREACHED() << "Unknown user.";
    return;
  }
  SetCustomWallpaper(user_id,
                     user->username_hash(),
                     "policy-controlled.jpeg",
                     ash::WALLPAPER_LAYOUT_CENTER_CROPPED,
                     user_manager::User::POLICY,
                     user_image.image(),
                     true /* update wallpaper */);
}

void WallpaperManager::SetCustomWallpaper(
    const std::string& user_id,
    const std::string& user_id_hash,
    const std::string& file,
    ash::WallpaperLayout layout,
    user_manager::User::WallpaperType type,
    const gfx::ImageSkia& image,
    bool update_wallpaper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(UserManager::Get()->IsUserLoggedIn());

  // There is no visible background in kiosk mode.
  if (UserManager::Get()->IsLoggedInAsKioskApp())
    return;

  // Don't allow custom wallpapers while policy is in effect.
  if (type != user_manager::User::POLICY && IsPolicyControlled(user_id))
    return;

  base::FilePath wallpaper_path =
      GetCustomWallpaperPath(kOriginalWallpaperSubDir, user_id_hash, file);

  // If decoded wallpaper is empty, we have probably failed to decode the file.
  // Use default wallpaper in this case.
  if (image.isNull()) {
    SetDefaultWallpaperDelayed(user_id);
    return;
  }

  bool is_persistent =
      !UserManager::Get()->IsUserNonCryptohomeDataEphemeral(user_id);

  WallpaperInfo wallpaper_info = {
      wallpaper_path.value(),
      layout,
      type,
      // Date field is not used.
      base::Time::Now().LocalMidnight()
  };
  if (is_persistent) {
    image.EnsureRepsForSupportedScales();
    scoped_ptr<gfx::ImageSkia> deep_copy(image.DeepCopy());
    // Block shutdown on this task. Otherwise, we may lose the custom wallpaper
    // that the user selected.
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner =
        BrowserThread::GetBlockingPool()
            ->GetSequencedTaskRunnerWithShutdownBehavior(
                sequence_token_, base::SequencedWorkerPool::BLOCK_SHUTDOWN);
    // TODO(bshe): This may break if RawImage becomes RefCountedMemory.
    blocking_task_runner->PostTask(
        FROM_HERE,
        base::Bind(&WallpaperManager::SaveCustomWallpaper,
                   user_id_hash,
                   base::FilePath(wallpaper_info.file),
                   wallpaper_info.layout,
                   base::Passed(deep_copy.Pass())));
  }

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
    GetPendingWallpaper(user_id, false)->ResetSetWallpaperImage(image, info);
  }

  wallpaper_cache_[user_id] = image;
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
  ash::WallpaperLayout layout = use_small
                                    ? ash::WALLPAPER_LAYOUT_CENTER
                                    : ash::WALLPAPER_LAYOUT_CENTER_CROPPED;
  DCHECK(file);
  if (!default_wallpaper_image_.get() ||
      default_wallpaper_image_->file_path() != file->value()) {
    default_wallpaper_image_.reset();
    if (!file->empty()) {
      loaded_wallpapers_++;
      StartLoadAndSetDefaultWallpaper(
          *file, layout, on_finish.Pass(), &default_wallpaper_image_);
      return;
    }

    CreateSolidDefaultWallpaper();
  }
  // 1x1 wallpaper is actually solid color, so it should be stretched.
  if (default_wallpaper_image_->image().width() == 1 &&
      default_wallpaper_image_->image().height() == 1)
    layout = ash::WALLPAPER_LAYOUT_STRETCH;

  ash::Shell::GetInstance()->desktop_background_controller()->SetWallpaperImage(
      default_wallpaper_image_->image(), layout);
}

// static
void WallpaperManager::RecordUma(user_manager::User::WallpaperType type,
                                 int index) {
  UMA_HISTOGRAM_ENUMERATION(
      "Ash.Wallpaper.Type", type, user_manager::User::WALLPAPER_TYPE_COUNT);
}

// static
void WallpaperManager::SaveCustomWallpaper(const std::string& user_id_hash,
                                           const base::FilePath& original_path,
                                           ash::WallpaperLayout layout,
                                           scoped_ptr<gfx::ImageSkia> image) {
  EnsureCustomWallpaperDirectories(user_id_hash);
  std::string file_name = original_path.BaseName().value();
  base::FilePath small_wallpaper_path =
      GetCustomWallpaperPath(kSmallWallpaperSubDir, user_id_hash, file_name);
  base::FilePath large_wallpaper_path =
      GetCustomWallpaperPath(kLargeWallpaperSubDir, user_id_hash, file_name);

  // Re-encode orginal file to jpeg format and saves the result in case that
  // resized wallpaper is not generated (i.e. chrome shutdown before resized
  // wallpaper is saved).
  ResizeAndSaveWallpaper(*image,
                         original_path,
                         ash::WALLPAPER_LAYOUT_STRETCH,
                         image->width(),
                         image->height(),
                         NULL);
  DeleteAllExcept(original_path);

  ResizeAndSaveWallpaper(*image,
                         small_wallpaper_path,
                         layout,
                         kSmallWallpaperMaxWidth,
                         kSmallWallpaperMaxHeight,
                         NULL);
  DeleteAllExcept(small_wallpaper_path);
  ResizeAndSaveWallpaper(*image,
                         large_wallpaper_path,
                         layout,
                         kLargeWallpaperMaxWidth,
                         kLargeWallpaperMaxHeight,
                         NULL);
  DeleteAllExcept(large_wallpaper_path);
}

// static
void WallpaperManager::MoveCustomWallpapersOnWorker(
    const std::string& user_id,
    const std::string& user_id_hash,
    base::WeakPtr<WallpaperManager> weak_ptr) {

  if (MoveCustomWallpaperDirectory(
          kOriginalWallpaperSubDir, user_id, user_id_hash)) {
    // Consider success if the original wallpaper is moved to the new directory.
    // Original wallpaper is the fallback if the correct resolution wallpaper
    // can not be found.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&WallpaperManager::MoveCustomWallpapersSuccess,
                   weak_ptr,
                   user_id,
                   user_id_hash));
  }
  MoveCustomWallpaperDirectory(kLargeWallpaperSubDir, user_id, user_id_hash);
  MoveCustomWallpaperDirectory(kSmallWallpaperSubDir, user_id, user_id_hash);
  MoveCustomWallpaperDirectory(
      kThumbnailWallpaperSubDir, user_id, user_id_hash);
}

// static
void WallpaperManager::GetCustomWallpaperInternal(
    const std::string& user_id,
    const WallpaperInfo& info,
    const base::FilePath& wallpaper_path,
    bool update_wallpaper,
    MovableOnDestroyCallbackHolder on_finish,
    base::WeakPtr<WallpaperManager> weak_ptr) {

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
                                       weak_ptr,
                                       user_id,
                                       base::Passed(on_finish.Pass())));
  } else {
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&WallpaperManager::StartLoad,
                                       weak_ptr,
                                       user_id,
                                       info,
                                       update_wallpaper,
                                       valid_path,
                                       base::Passed(on_finish.Pass())));
  }
}

void WallpaperManager::InitInitialUserWallpaper(const std::string& user_id,
                                                bool is_persistent) {
  current_user_wallpaper_info_.file = "";
  current_user_wallpaper_info_.layout = ash::WALLPAPER_LAYOUT_CENTER_CROPPED;
  current_user_wallpaper_info_.type = user_manager::User::DEFAULT;
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
  const user_manager::User* user = UserManager::Get()->FindUser(user_id);
  if (UserManager::Get()->IsUserNonCryptohomeDataEphemeral(user_id) ||
      (user != NULL && user->GetType() == user_manager::USER_TYPE_KIOSK_APP)) {
    InitInitialUserWallpaper(user_id, false);
    GetPendingWallpaper(user_id, delayed)->ResetSetDefaultWallpaper();
    return;
  }

  if (!UserManager::Get()->IsKnownUser(user_id))
    return;

  last_selected_user_ = user_id;

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
    if (info.file.empty()) {
      // Uses default built-in wallpaper when file is empty. Eventually, we
      // will only ship one built-in wallpaper in ChromeOS image.
      GetPendingWallpaper(user_id, delayed)->ResetSetDefaultWallpaper();
      return;
    }

    if (info.type == user_manager::User::CUSTOMIZED ||
        info.type == user_manager::User::POLICY) {
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

    // Load downloaded ONLINE or converted DEFAULT wallpapers.
    GetPendingWallpaper(user_id, delayed)->ResetLoadWallpaper(info);
  }
}

void WallpaperManager::SetWallpaperFromImageSkia(const std::string& user_id,
                                                 const gfx::ImageSkia& image,
                                                 ash::WallpaperLayout layout,
                                                 bool update_wallpaper) {
  DCHECK(UserManager::Get()->IsUserLoggedIn());

  // There is no visible background in kiosk mode.
  if (UserManager::Get()->IsLoggedInAsKioskApp())
    return;
  WallpaperInfo info;
  info.layout = layout;
  wallpaper_cache_[user_id] = image;

  if (update_wallpaper) {
    GetPendingWallpaper(last_selected_user_, false /* Not delayed */)
        ->ResetSetWallpaperImage(image, info);
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
    SetDefaultWallpaperNow(chromeos::login::kSignInUser);
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
                                             gfx::ImageSkia* image) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CustomWallpaperMap::const_iterator it = wallpaper_cache_.find(user_id);
  if (it != wallpaper_cache_.end()) {
    *image = (*it).second;
    return true;
  }
  return false;
}

void WallpaperManager::CacheUsersWallpapers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  user_manager::UserList users = UserManager::Get()->GetUsers();

  if (!users.empty()) {
    user_manager::UserList::const_iterator it = users.begin();
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
    if (info.file.empty())
      return;

    base::FilePath wallpaper_dir;
    base::FilePath wallpaper_path;
    if (info.type == user_manager::User::CUSTOMIZED ||
        info.type == user_manager::User::POLICY) {
      const char* sub_dir = GetCustomWallpaperSubdirForCurrentResolution();
      base::FilePath wallpaper_path = GetCustomWallpaperDir(sub_dir);
      wallpaper_path = wallpaper_path.Append(info.file);
      task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&WallpaperManager::GetCustomWallpaperInternal,
                     user_id,
                     info,
                     wallpaper_path,
                     false /* do not update wallpaper */,
                     base::Passed(MovableOnDestroyCallbackHolder()),
                     weak_factory_.GetWeakPtr()));
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
      base::Bind(&DeleteWallpaperInList, file_to_remove),
      false);
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
  const user_manager::UserList& users = UserManager::Get()->GetUsers();
  int public_session_user_index = FindPublicSession(users);
  if ((!show_users && public_session_user_index == -1) || users.empty()) {
    // Boot into sign in form, preload default wallpaper.
    SetDefaultWallpaperDelayed(chromeos::login::kSignInUser);
    return;
  }

  if (!disable_boot_animation) {
    int index = public_session_user_index != -1 ? public_session_user_index : 0;
    // Normal boot, load user wallpaper.
    // If normal boot animation is disabled wallpaper would be set
    // asynchronously once user pods are loaded.
    SetUserWallpaperDelayed(users[index]->email());
  }
}

void WallpaperManager::LoadWallpaper(const std::string& user_id,
                                     const WallpaperInfo& info,
                                     bool update_wallpaper,
                                     MovableOnDestroyCallbackHolder on_finish) {
  base::FilePath wallpaper_dir;
  base::FilePath wallpaper_path;

  // Do a sanity check that file path information is not empty.
  if (info.type == user_manager::User::ONLINE ||
      info.type == user_manager::User::DEFAULT) {
    if (info.file.empty()) {
      if (base::SysInfo::IsRunningOnChromeOS()) {
        NOTREACHED() << "User wallpaper info appears to be broken: " << user_id;
      } else {
        // Filename might be empty on debug configurations when stub users
        // were created directly in Local State (for testing). Ignore such
        // errors i.e. allowsuch type of debug configurations on the desktop.
        LOG(WARNING) << "User wallpaper info is empty: " << user_id;

        // |on_finish| callback will get called on destruction.
        return;
      }
    }
  }

  if (info.type == user_manager::User::ONLINE) {
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
  } else if (info.type == user_manager::User::DEFAULT) {
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
  info->type = static_cast<user_manager::User::WallpaperType>(type);
  info->date = base::Time::FromInternalValue(date_val);
  return true;
}

void WallpaperManager::MoveCustomWallpapersSuccess(
    const std::string& user_id,
    const std::string& user_id_hash) {
  WallpaperInfo info;
  GetUserWallpaperInfo(user_id, &info);
  if (info.type == user_manager::User::CUSTOMIZED) {
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
  const user_manager::User* logged_in_user =
      UserManager::Get()->GetLoggedInUser();
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WallpaperManager::MoveCustomWallpapersOnWorker,
                 logged_in_user->email(),
                 logged_in_user->username_hash(),
                 weak_factory_.GetWeakPtr()));
}

void WallpaperManager::OnWallpaperDecoded(
    const std::string& user_id,
    ash::WallpaperLayout layout,
    bool update_wallpaper,
    MovableOnDestroyCallbackHolder on_finish,
    const user_manager::UserImage& user_image) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TRACE_EVENT_ASYNC_END0("ui", "LoadAndDecodeWallpaper", this);

  // If decoded wallpaper is empty, we have probably failed to decode the file.
  // Use default wallpaper in this case.
  if (user_image.image().isNull()) {
    // Updates user pref to default wallpaper.
    WallpaperInfo info = {"", ash::WALLPAPER_LAYOUT_CENTER_CROPPED,
                          user_manager::User::DEFAULT,
                          base::Time::Now().LocalMidnight()};
    SetUserWallpaperInfo(user_id, info, true);

    if (update_wallpaper)
      DoSetDefaultWallpaper(user_id, on_finish.Pass());
    return;
  }

  wallpaper_cache_[user_id] = user_image.image();

  if (update_wallpaper) {
    ash::Shell::GetInstance()
        ->desktop_background_controller()
        ->SetWallpaperImage(user_image.image(), layout);
  }
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
                                      weak_factory_.GetWeakPtr(),
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

void WallpaperManager::SetCustomizedDefaultWallpaperAfterCheck(
    const GURL& wallpaper_url,
    const base::FilePath& downloaded_file,
    scoped_ptr<CustomizedWallpaperRescaledFiles> rescaled_files) {
  PrefService* pref_service = g_browser_process->local_state();

  std::string current_url =
      pref_service->GetString(prefs::kCustomizationDefaultWallpaperURL);
  if (current_url != wallpaper_url.spec() || !rescaled_files->AllSizesExist()) {
    DCHECK(rescaled_files->downloaded_exists());

    // Either resized images do not exist or cached version is incorrect.
    // Need to start resize again.
    wallpaper_loader_->Start(
        downloaded_file.value(),
        0,  // Do not crop.
        base::Bind(&WallpaperManager::OnCustomizedDefaultWallpaperDecoded,
                   weak_factory_.GetWeakPtr(),
                   wallpaper_url,
                   base::Passed(rescaled_files.Pass())));
  } else {
    SetDefaultWallpaperPath(rescaled_files->path_rescaled_small(),
                            scoped_ptr<gfx::ImageSkia>().Pass(),
                            rescaled_files->path_rescaled_large(),
                            scoped_ptr<gfx::ImageSkia>().Pass());
  }
}

void WallpaperManager::OnCustomizedDefaultWallpaperDecoded(
    const GURL& wallpaper_url,
    scoped_ptr<CustomizedWallpaperRescaledFiles> rescaled_files,
    const user_manager::UserImage& wallpaper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If decoded wallpaper is empty, we have probably failed to decode the file.
  if (wallpaper.image().isNull()) {
    LOG(WARNING) << "Failed to decode customized wallpaper.";
    return;
  }

  wallpaper.image().EnsureRepsForSupportedScales();
  scoped_ptr<gfx::ImageSkia> deep_copy(wallpaper.image().DeepCopy());

  scoped_ptr<bool> success(new bool(false));
  scoped_ptr<gfx::ImageSkia> small_wallpaper_image(new gfx::ImageSkia);
  scoped_ptr<gfx::ImageSkia> large_wallpaper_image(new gfx::ImageSkia);

  // TODO(bshe): This may break if RawImage becomes RefCountedMemory.
  base::Closure resize_closure =
      base::Bind(&WallpaperManager::ResizeCustomizedDefaultWallpaper,
                 base::Passed(&deep_copy),
                 wallpaper.raw_image(),
                 base::Unretained(rescaled_files.get()),
                 base::Unretained(success.get()),
                 base::Unretained(small_wallpaper_image.get()),
                 base::Unretained(large_wallpaper_image.get()));
  base::Closure on_resized_closure =
      base::Bind(&WallpaperManager::OnCustomizedDefaultWallpaperResized,
                 weak_factory_.GetWeakPtr(),
                 wallpaper_url,
                 base::Passed(rescaled_files.Pass()),
                 base::Passed(success.Pass()),
                 base::Passed(small_wallpaper_image.Pass()),
                 base::Passed(large_wallpaper_image.Pass()));

  if (!task_runner_->PostTaskAndReply(
          FROM_HERE, resize_closure, on_resized_closure)) {
    LOG(WARNING) << "Failed to start Customized Wallpaper resize.";
  }
}

void WallpaperManager::ResizeCustomizedDefaultWallpaper(
    scoped_ptr<gfx::ImageSkia> image,
    const user_manager::UserImage::RawImage& raw_image,
    const CustomizedWallpaperRescaledFiles* rescaled_files,
    bool* success,
    gfx::ImageSkia* small_wallpaper_image,
    gfx::ImageSkia* large_wallpaper_image) {
  *success = true;

  *success &= ResizeAndSaveWallpaper(*image,
                                     rescaled_files->path_rescaled_small(),
                                     ash::WALLPAPER_LAYOUT_STRETCH,
                                     kSmallWallpaperMaxWidth,
                                     kSmallWallpaperMaxHeight,
                                     small_wallpaper_image);

  *success &= ResizeAndSaveWallpaper(*image,
                                     rescaled_files->path_rescaled_large(),
                                     ash::WALLPAPER_LAYOUT_STRETCH,
                                     kLargeWallpaperMaxWidth,
                                     kLargeWallpaperMaxHeight,
                                     large_wallpaper_image);
}

void WallpaperManager::OnCustomizedDefaultWallpaperResized(
    const GURL& wallpaper_url,
    scoped_ptr<CustomizedWallpaperRescaledFiles> rescaled_files,
    scoped_ptr<bool> success,
    scoped_ptr<gfx::ImageSkia> small_wallpaper_image,
    scoped_ptr<gfx::ImageSkia> large_wallpaper_image) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(rescaled_files);
  DCHECK(success.get());
  if (!*success) {
    LOG(WARNING) << "Failed to save resized customized default wallpaper";
    return;
  }
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetString(prefs::kCustomizationDefaultWallpaperURL,
                          wallpaper_url.spec());
  SetDefaultWallpaperPath(rescaled_files->path_rescaled_small(),
                          small_wallpaper_image.Pass(),
                          rescaled_files->path_rescaled_large(),
                          large_wallpaper_image.Pass());
  VLOG(1) << "Customized default wallpaper applied.";
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

void WallpaperManager::RemovePendingWallpaperFromList(
    PendingWallpaper* pending) {
  DCHECK(loading_.size() > 0);
  for (WallpaperManager::PendingList::iterator i = loading_.begin();
       i != loading_.end();
       ++i) {
    if (i->get() == pending) {
      loading_.erase(i);
      break;
    }
  }

  if (loading_.empty())
    FOR_EACH_OBSERVER(Observer, observers_, OnPendingListEmptyForTesting());
}

void WallpaperManager::SetCustomizedDefaultWallpaper(
    const GURL& wallpaper_url,
    const base::FilePath& downloaded_file,
    const base::FilePath& resized_directory) {
  // Should fail if this ever happens in tests.
  DCHECK(wallpaper_url.is_valid());
  if (!wallpaper_url.is_valid()) {
    if (!wallpaper_url.is_empty()) {
      LOG(WARNING) << "Invalid Customized Wallpaper URL '"
                   << wallpaper_url.spec() << "'";
    }
    return;
  }
  std::string downloaded_file_name = downloaded_file.BaseName().value();
  scoped_ptr<CustomizedWallpaperRescaledFiles> rescaled_files(
      new CustomizedWallpaperRescaledFiles(
          downloaded_file,
          resized_directory.Append(downloaded_file_name +
                                   kSmallWallpaperSuffix),
          resized_directory.Append(downloaded_file_name +
                                   kLargeWallpaperSuffix)));

  base::Closure check_file_exists = rescaled_files->CreateCheckerClosure();
  base::Closure on_checked_closure =
      base::Bind(&WallpaperManager::SetCustomizedDefaultWallpaperAfterCheck,
                 weak_factory_.GetWeakPtr(),
                 wallpaper_url,
                 downloaded_file,
                 base::Passed(rescaled_files.Pass()));
  if (!BrowserThread::PostBlockingPoolTaskAndReply(
          FROM_HERE, check_file_exists, on_checked_closure)) {
    LOG(WARNING) << "Failed to start check CheckCustomizedWallpaperFilesExist.";
  }
}

size_t WallpaperManager::GetPendingListSizeForTesting() const {
  return loading_.size();
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
    scoped_ptr<user_manager::UserImage>* result_out,
    MovableOnDestroyCallbackHolder on_finish,
    const user_manager::UserImage& user_image) {
  result_out->reset(new user_manager::UserImage(user_image));
  ash::Shell::GetInstance()->desktop_background_controller()->SetWallpaperImage(
      user_image.image(), layout);
}

void WallpaperManager::StartLoadAndSetDefaultWallpaper(
    const base::FilePath& path,
    const ash::WallpaperLayout layout,
    MovableOnDestroyCallbackHolder on_finish,
    scoped_ptr<user_manager::UserImage>* result_out) {
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

void WallpaperManager::SetDefaultWallpaperPath(
    const base::FilePath& default_small_wallpaper_file,
    scoped_ptr<gfx::ImageSkia> small_wallpaper_image,
    const base::FilePath& default_large_wallpaper_file,
    scoped_ptr<gfx::ImageSkia> large_wallpaper_image) {
  default_small_wallpaper_file_ = default_small_wallpaper_file;
  default_large_wallpaper_file_ = default_large_wallpaper_file;

  ash::DesktopBackgroundController* dbc =
      ash::Shell::GetInstance()->desktop_background_controller();

  // |need_update_screen| is true if the previous default wallpaper is visible
  // now, so we need to update wallpaper on the screen.
  //
  // Layout is ignored here, so ash::WALLPAPER_LAYOUT_CENTER is used
  // as a placeholder only.
  const bool need_update_screen =
      default_wallpaper_image_.get() &&
      dbc->WallpaperIsAlreadyLoaded(default_wallpaper_image_->image(),
                                    false /* compare_layouts */,
                                    ash::WALLPAPER_LAYOUT_CENTER);

  default_wallpaper_image_.reset();
  if (GetAppropriateResolution() == WALLPAPER_RESOLUTION_SMALL) {
    if (small_wallpaper_image) {
      default_wallpaper_image_.reset(
          new user_manager::UserImage(*small_wallpaper_image));
      default_wallpaper_image_->set_file_path(
          default_small_wallpaper_file.value());
    }
  } else {
    if (large_wallpaper_image) {
      default_wallpaper_image_.reset(
          new user_manager::UserImage(*large_wallpaper_image));
      default_wallpaper_image_->set_file_path(
          default_large_wallpaper_file.value());
    }
  }

  if (need_update_screen) {
    DoSetDefaultWallpaper(std::string(),
                          MovableOnDestroyCallbackHolder().Pass());
  }
}

void WallpaperManager::CreateSolidDefaultWallpaper() {
  loaded_wallpapers_++;
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(kDefaultWallpaperColor);
  const gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  default_wallpaper_image_.reset(new user_manager::UserImage(image));
}

}  // namespace chromeos
