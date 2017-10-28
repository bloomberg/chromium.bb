// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"

#include <utility>

#include "ash/ash_constants.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "ash/wallpaper/wallpaper_window_state_manager.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/task_scheduler/post_task.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/extensions/wallpaper_manager_util.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_loader.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "components/wallpaper/wallpaper_files_id.h"
#include "components/wallpaper/wallpaper_resizer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "crypto/sha2.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skia_util.h"
#include "url/gurl.h"

using content::BrowserThread;
using wallpaper::WallpaperInfo;

namespace chromeos {

namespace {

WallpaperManager* wallpaper_manager = nullptr;

// The amount of delay before starts to move custom wallpapers to the new place.
const int kMoveCustomWallpaperDelaySeconds = 30;

const int kCacheWallpaperDelayMs = 500;

// Names of nodes with wallpaper info in |kUserWallpapersInfo| dictionary.
const char kNewWallpaperDateNodeName[] = "date";
const char kNewWallpaperLayoutNodeName[] = "layout";
const char kNewWallpaperLocationNodeName[] = "file";
const char kNewWallpaperTypeNodeName[] = "type";

// Known user keys.
const char kWallpaperFilesId[] = "wallpaper-files-id";

// The directory and file name to save the downloaded device policy controlled
// wallpaper.
const char kDeviceWallpaperDir[] = "device_wallpaper";
const char kDeviceWallpaperFile[] = "device_wallpaper_image.jpg";

// Default quality for encoding wallpaper.
const int kDefaultEncodingQuality = 90;

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

// The path ids for directories.
int dir_user_data_path_id = -1;            // chrome::DIR_USER_DATA
int dir_chromeos_wallpapers_path_id = -1;  // chrome::DIR_CHROMEOS_WALLPAPERS
int dir_chromeos_custom_wallpapers_path_id =
    -1;  // chrome::DIR_CHROMEOS_CUSTOM_WALLPAPERS

// The Wallpaper App that the user is using right now on Chrome OS. It's the
// app that is used when the user right clicks on desktop and selects "Set
// wallpaper" or when the user selects "Set wallpaper" from chrome://settings
// page. This is recorded at user login. It's used to back an UMA histogram so
// it should be treated as append-only.
enum WallpaperAppsType {
  WALLPAPERS_PICKER_APP_CHROMEOS = 0,
  WALLPAPERS_APP_ANDROID = 1,
  WALLPAPERS_APPS_NUM = 2,
};

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

// Whether WallpaperController should start with customized default
// wallpaper in WallpaperManager::InitializeWallpaper() or not.
bool ShouldUseCustomizedDefaultWallpaper() {
  PrefService* pref_service = g_browser_process->local_state();

  return !pref_service->FindPreference(
      prefs::kCustomizationDefaultWallpaperURL)->IsDefaultValue();
}

// Returns index of the first public session user found in |users|
// or -1 otherwise.
int FindPublicSession(const user_manager::UserList& users) {
  int index = -1;
  int i = 0;
  for (user_manager::UserList::const_iterator it = users.begin();
       it != users.end(); ++it, ++i) {
    if ((*it)->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
      index = i;
      break;
    }
  }

  return index;
}

// Returns true if |users| contains users other than device local account users.
bool HasNonDeviceLocalAccounts(const user_manager::UserList& users) {
  for (const auto* user : users) {
    if (!policy::IsDeviceLocalAccountUser(user->GetAccountId().GetUserEmail(),
                                          nullptr))
      return true;
  }
  return false;
}

// This has once been copied from
// brillo::cryptohome::home::SanitizeUserName(username) to be used for
// wallpaper identification purpose only.
//
// Historic note: We need some way to identify users wallpaper files in
// the device filesystem. Historically User::username_hash() was used for this
// purpose, but it has two caveats:
// 1. username_hash() is defined only after user has logged in.
// 2. If cryptohome identifier changes, username_hash() will also change,
//    and we may loose user => wallpaper files mapping at that point.
// So this function gives WallpaperManager independent hashing method to break
// this dependency.
//
wallpaper::WallpaperFilesId HashWallpaperFilesIdStr(
    const std::string& files_id_unhashed) {
  SystemSaltGetter* salt_getter = SystemSaltGetter::Get();
  DCHECK(salt_getter);

  // System salt must be defined at this point.
  const SystemSaltGetter::RawSalt* salt = salt_getter->GetRawSalt();
  if (!salt)
    LOG(FATAL) << "WallpaperManager HashWallpaperFilesIdStr(): no salt!";

  unsigned char binmd[base::kSHA1Length];
  std::string lowercase(files_id_unhashed);
  std::transform(lowercase.begin(), lowercase.end(), lowercase.begin(),
                 ::tolower);
  std::vector<uint8_t> data = *salt;
  std::copy(files_id_unhashed.begin(), files_id_unhashed.end(),
            std::back_inserter(data));
  base::SHA1HashBytes(data.data(), data.size(), binmd);
  std::string result = base::HexEncode(binmd, sizeof(binmd));
  std::transform(result.begin(), result.end(), result.begin(), ::tolower);
  return wallpaper::WallpaperFilesId::FromString(result);
}

// Call |closure| when HashWallpaperFilesIdStr will not assert().
void CallWhenCanGetFilesId(const base::Closure& closure) {
  SystemSaltGetter::Get()->AddOnSystemSaltReady(closure);
}

void SetKnownUserWallpaperFilesId(
    const AccountId& account_id,
    const wallpaper::WallpaperFilesId& wallpaper_files_id) {
  user_manager::known_user::SetStringPref(account_id, kWallpaperFilesId,
                                          wallpaper_files_id.id());
}

// A helper to set the wallpaper image for Classic Ash and Mash.
void SetWallpaper(const gfx::ImageSkia& image, wallpaper::WallpaperInfo info) {
  if (ash_util::IsRunningInMash()) {
    // In mash, connect to the WallpaperController interface via mojo.
    service_manager::Connector* connector =
        content::ServiceManagerConnection::GetForProcess()->GetConnector();
    if (!connector)
      return;

    ash::mojom::WallpaperControllerPtr wallpaper_controller;
    connector->BindInterface(ash::mojom::kServiceName, &wallpaper_controller);
    // TODO(crbug.com/655875): Optimize ash wallpaper transport; avoid sending
    // large bitmaps over Mojo; use shared memory like BitmapUploader, etc.
    wallpaper_controller->SetWallpaper(*image.bitmap(), info);
  } else if (ash::Shell::HasInstance()) {
    // Note: Wallpaper setting is skipped in unit tests without shell instances.
    // In classic ash, interact with the WallpaperController class directly.
    ash::Shell::Get()->wallpaper_controller()->SetWallpaperImage(image, info);
  }
}

// A helper function to check the existing/downloaded device wallpaper file's
// hash value matches with the hash value provided in the policy settings.
bool CheckDeviceWallpaperMatchHash(const base::FilePath& device_wallpaper_file,
                                   const std::string& hash) {
  std::string image_data;
  if (base::ReadFileToString(device_wallpaper_file, &image_data)) {
    std::string sha_hash = crypto::SHA256HashString(image_data);
    if (base::ToLowerASCII(base::HexEncode(
            sha_hash.c_str(), sha_hash.size())) == base::ToLowerASCII(hash)) {
      return true;
    }
  }
  return false;
}

bool MoveCustomWallpaperDirectory(const char* sub_dir,
                                  const std::string& from_name,
                                  const std::string& to_name) {
  base::FilePath base_path = WallpaperManager::GetCustomWallpaperDir(sub_dir);
  base::FilePath to_path = base_path.Append(to_name);
  base::FilePath from_path = base_path.Append(from_name);
  if (base::PathExists(from_path))
    return base::Move(from_path, to_path);
  return false;
}

// Deletes a list of wallpaper files in |file_list|.
void DeleteWallpaperInList(const std::vector<base::FilePath>& file_list) {
  for (std::vector<base::FilePath>::const_iterator it = file_list.begin();
       it != file_list.end(); ++it) {
    base::FilePath path = *it;
    if (!base::DeleteFile(path, true))
      LOG(ERROR) << "Failed to remove user wallpaper at " << path.value();
  }
}

// Creates all new custom wallpaper directories for |wallpaper_files_id| if not
// exist.
void EnsureCustomWallpaperDirectories(
    const wallpaper::WallpaperFilesId& wallpaper_files_id) {
  base::FilePath dir;
  dir = WallpaperManager::GetCustomWallpaperDir(kSmallWallpaperSubDir);
  dir = dir.Append(wallpaper_files_id.id());
  if (!base::PathExists(dir))
    base::CreateDirectory(dir);
  dir = WallpaperManager::GetCustomWallpaperDir(kLargeWallpaperSubDir);
  dir = dir.Append(wallpaper_files_id.id());
  if (!base::PathExists(dir))
    base::CreateDirectory(dir);
  dir = WallpaperManager::GetCustomWallpaperDir(kOriginalWallpaperSubDir);
  dir = dir.Append(wallpaper_files_id.id());
  if (!base::PathExists(dir))
    base::CreateDirectory(dir);
  dir = WallpaperManager::GetCustomWallpaperDir(kThumbnailWallpaperSubDir);
  dir = dir.Append(wallpaper_files_id.id());
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

}  // namespace

void AssertCalledOnWallpaperSequence(base::SequencedTaskRunner* task_runner) {
#if DCHECK_IS_ON()
  DCHECK(task_runner->RunsTasksInCurrentSequence());
#endif
}

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

const char kUsersWallpaperInfo[] = "user_wallpaper_info";

// This is "wallpaper either scheduled to load, or loading right now".
//
// While enqueued, it defines moment in the future, when it will be loaded.
// Enqueued but not started request might be updated by subsequent load
// request. Therefore it's created empty, and updated being enqueued.
//
// PendingWallpaper is owned by WallpaperManager, but reference to this object
// is passed to other threads by PostTask() calls, therefore it is
// RefCountedThreadSafe.
class WallpaperManager::PendingWallpaper
    : public base::RefCountedThreadSafe<PendingWallpaper> {
 public:
  PendingWallpaper(const base::TimeDelta delay)
      : on_finish_(new MovableOnDestroyCallback(
            base::Bind(&WallpaperManager::PendingWallpaper::OnWallpaperSet,
                       this))) {
    timer.Start(
        FROM_HERE, delay,
        base::Bind(&WallpaperManager::PendingWallpaper::ProcessRequest, this));
  }

  // There are four cases:
  // 1) gfx::ImageSkia is found in cache.
  void SetWallpaperFromImage(const AccountId& account_id,
                             const gfx::ImageSkia& image,
                             const wallpaper::WallpaperInfo& info) {
    SetMode(account_id, image, info, base::FilePath(), false);
  }

  // 2) WallpaperInfo is found in cache.
  void SetWallpaperFromInfo(const AccountId& account_id,
                            const wallpaper::WallpaperInfo& info) {
    SetMode(account_id, gfx::ImageSkia(), info, base::FilePath(), false);
  }

  // 3) Wallpaper path is not null.
  void SetWallpaperFromPath(const AccountId& account_id,
                            const wallpaper::WallpaperInfo& info,
                            const base::FilePath& wallpaper_path) {
    SetMode(account_id, gfx::ImageSkia(), info, wallpaper_path, false);
  }

  // 4) Set default wallpaper (either on some error, or when user is new).
  void SetDefaultWallpaper(const AccountId& account_id) {
    SetMode(account_id, gfx::ImageSkia(), WallpaperInfo(), base::FilePath(),
            true);
  }

  uint32_t GetImageId() const {
    return wallpaper::WallpaperResizer::GetImageId(user_wallpaper_);
  }

 private:
  friend class base::RefCountedThreadSafe<PendingWallpaper>;

  ~PendingWallpaper() {}

  // All methods use SetMode() to set object to new state.
  void SetMode(const AccountId& account_id,
               const gfx::ImageSkia& image,
               const wallpaper::WallpaperInfo& info,
               const base::FilePath& wallpaper_path,
               const bool is_default) {
    account_id_ = account_id;
    user_wallpaper_ = image;
    info_ = info;
    wallpaper_path_ = wallpaper_path;
    default_ = is_default;
  }

  // This method is usually triggered by timer to actually load request.
  void ProcessRequest() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // Erase reference to self.
    timer.Stop();

    WallpaperManager* manager = WallpaperManager::Get();
    if (manager->pending_inactive_ == this)
      manager->pending_inactive_ = NULL;

    started_load_at_ = base::Time::Now();

    if (default_) {
      // The most recent request is |SetDefaultWallpaper|.
      manager->DoSetDefaultWallpaper(account_id_, true /* update_wallpaper */,
                                     std::move(on_finish_));
    } else if (!user_wallpaper_.isNull()) {
      // The most recent request is |SetWallpaperFromImage|.
      SetWallpaper(user_wallpaper_, info_);
    } else if (!wallpaper_path_.empty()) {
      // The most recent request is |SetWallpaperFromPath|.
      manager->task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&WallpaperManager::GetCustomWallpaperInternal,
                         account_id_, info_, wallpaper_path_,
                         true /* update wallpaper */,
                         base::ThreadTaskRunnerHandle::Get(),
                         base::Passed(std::move(on_finish_)),
                         manager->weak_factory_.GetWeakPtr()));
    } else if (!info_.location.empty()) {
      // The most recent request is |SetWallpaperFromInfo|.
      manager->LoadWallpaper(account_id_, info_, true /* update_wallpaper */,
                             std::move(on_finish_));
    } else {
      // PendingWallpaper was created but none of the four methods was called.
      // This should never happen. Do not record time in this case.
      NOTREACHED();
      started_load_at_ = base::Time();
    }
    on_finish_.reset();
  }

  // This method is called by callback, when load request is finished.
  void OnWallpaperSet() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // The only known case for this check to fail is global destruction during
    // wallpaper load. It should never happen.
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI))
      return;

    // Erase reference to self.
    timer.Stop();

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

  AccountId account_id_;
  wallpaper::WallpaperInfo info_;
  gfx::ImageSkia user_wallpaper_;
  base::FilePath wallpaper_path_;

  // Load default wallpaper instead of user image.
  bool default_;

  // This is "on destroy" callback that will call OnWallpaperSet() when
  // image will be loaded.
  MovableOnDestroyCallbackHolder on_finish_;
  base::OneShotTimer timer;

  // Load start time to calculate duration.
  base::Time started_load_at_;

  DISALLOW_COPY_AND_ASSIGN(PendingWallpaper);
};

// CustomizedWallpaperRescaledFiles:
const base::FilePath&
WallpaperManager::CustomizedWallpaperRescaledFiles::path_downloaded() const {
  return path_downloaded_;
}

const base::FilePath&
WallpaperManager::CustomizedWallpaperRescaledFiles::path_rescaled_small()
    const {
  return path_rescaled_small_;
}

const base::FilePath&
WallpaperManager::CustomizedWallpaperRescaledFiles::path_rescaled_large()
    const {
  return path_rescaled_large_;
}

bool WallpaperManager::CustomizedWallpaperRescaledFiles::downloaded_exists()
    const {
  return downloaded_exists_;
}

bool WallpaperManager::CustomizedWallpaperRescaledFiles::rescaled_small_exists()
    const {
  return rescaled_small_exists_;
}

bool WallpaperManager::CustomizedWallpaperRescaledFiles::rescaled_large_exists()
    const {
  return rescaled_large_exists_;
}

WallpaperManager::CustomizedWallpaperRescaledFiles::
    CustomizedWallpaperRescaledFiles(const base::FilePath& path_downloaded,
                                     const base::FilePath& path_rescaled_small,
                                     const base::FilePath& path_rescaled_large)
    : path_downloaded_(path_downloaded),
      path_rescaled_small_(path_rescaled_small),
      path_rescaled_large_(path_rescaled_large),
      downloaded_exists_(false),
      rescaled_small_exists_(false),
      rescaled_large_exists_(false) {}

base::Closure
WallpaperManager::CustomizedWallpaperRescaledFiles::CreateCheckerClosure() {
  return base::Bind(&WallpaperManager::CustomizedWallpaperRescaledFiles::
                        CheckCustomizedWallpaperFilesExist,
                    base::Unretained(this));
}

void WallpaperManager::CustomizedWallpaperRescaledFiles::
    CheckCustomizedWallpaperFilesExist() {
  downloaded_exists_ = base::PathExists(path_downloaded_);
  rescaled_small_exists_ = base::PathExists(path_rescaled_small_);
  rescaled_large_exists_ = base::PathExists(path_rescaled_large_);
}

bool WallpaperManager::CustomizedWallpaperRescaledFiles::AllSizesExist() const {
  return rescaled_small_exists_ && rescaled_large_exists_;
}

WallpaperManager::MovableOnDestroyCallback::MovableOnDestroyCallback(
    const base::Closure& callback)
    : callback_(callback) {}

WallpaperManager::MovableOnDestroyCallback::~MovableOnDestroyCallback() {
  if (!callback_.is_null())
    callback_.Run();
}

// TestApi:
WallpaperManager::TestApi::TestApi(WallpaperManager* wallpaper_manager)
    : wallpaper_manager_(wallpaper_manager) {}

WallpaperManager::TestApi::~TestApi() {}

bool WallpaperManager::TestApi::GetWallpaperFromCache(
    const AccountId& account_id,
    gfx::ImageSkia* image) {
  return wallpaper_manager_->GetWallpaperFromCache(account_id, image);
}

bool WallpaperManager::TestApi::GetPathFromCache(const AccountId& account_id,
                                                 base::FilePath* path) {
  return wallpaper_manager_->GetPathFromCache(account_id, path);
}

void WallpaperManager::TestApi::SetWallpaperCache(const AccountId& account_id,
                                                  const base::FilePath& path,
                                                  const gfx::ImageSkia& image) {
  DCHECK(!image.isNull());
  wallpaper_manager_->wallpaper_cache_[account_id] =
      CustomWallpaperElement(path, image);
}

void WallpaperManager::TestApi::ClearDisposableWallpaperCache() {
  wallpaper_manager_->ClearDisposableWallpaperCache();
}

// WallpaperManager, public: ---------------------------------------------------

WallpaperManager::~WallpaperManager() {
  show_user_name_on_signin_subscription_.reset();
  device_wallpaper_image_subscription_.reset();
  user_manager::UserManager::Get()->RemoveObserver(this);
  weak_factory_.InvalidateWeakPtrs();
}

// static
void WallpaperManager::Initialize() {
  CHECK(!wallpaper_manager);
  wallpaper_manager = new WallpaperManager();
}

// static
WallpaperManager* WallpaperManager::Get() {
  DCHECK(wallpaper_manager);
  return wallpaper_manager;
}

// static
void WallpaperManager::Shutdown() {
  CHECK(wallpaper_manager);
  delete wallpaper_manager;
  wallpaper_manager = nullptr;
}

// static
void WallpaperManager::SetPathIds(int dir_user_data_enum,
                                  int dir_chromeos_wallpapers_enum,
                                  int dir_chromeos_custom_wallpapers_enum) {
  dir_user_data_path_id = dir_user_data_enum;
  dir_chromeos_wallpapers_path_id = dir_chromeos_wallpapers_enum;
  dir_chromeos_custom_wallpapers_path_id = dir_chromeos_custom_wallpapers_enum;
}

// static
base::FilePath WallpaperManager::GetCustomWallpaperDir(const char* sub_dir) {
  base::FilePath custom_wallpaper_dir;
  DCHECK(dir_chromeos_custom_wallpapers_path_id != -1);
  CHECK(PathService::Get(dir_chromeos_custom_wallpapers_path_id,
                         &custom_wallpaper_dir));
  return custom_wallpaper_dir.Append(sub_dir);
}

// static
void WallpaperManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kUsersWallpaperInfo);
}

// static
bool WallpaperManager::ResizeImage(const gfx::ImageSkia& image,
                                   wallpaper::WallpaperLayout layout,
                                   int preferred_width,
                                   int preferred_height,
                                   scoped_refptr<base::RefCountedBytes>* output,
                                   gfx::ImageSkia* output_skia) {
  int width = image.width();
  int height = image.height();
  int resized_width;
  int resized_height;
  *output = new base::RefCountedBytes();

  if (layout == wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED) {
    // Do not resize custom wallpaper if it is smaller than preferred size.
    if (!(width > preferred_width && height > preferred_height))
      return false;

    double horizontal_ratio = static_cast<double>(preferred_width) / width;
    double vertical_ratio = static_cast<double>(preferred_height) / height;
    if (vertical_ratio > horizontal_ratio) {
      resized_width =
          gfx::ToRoundedInt(static_cast<double>(width) * vertical_ratio);
      resized_height = preferred_height;
    } else {
      resized_width = preferred_width;
      resized_height =
          gfx::ToRoundedInt(static_cast<double>(height) * horizontal_ratio);
    }
  } else if (layout == wallpaper::WALLPAPER_LAYOUT_STRETCH) {
    resized_width = preferred_width;
    resized_height = preferred_height;
  } else {
    resized_width = width;
    resized_height = height;
  }

  gfx::ImageSkia resized_image = gfx::ImageSkiaOperations::CreateResizedImage(
      image, skia::ImageOperations::RESIZE_LANCZOS3,
      gfx::Size(resized_width, resized_height));

  SkBitmap bitmap = *(resized_image.bitmap());
  gfx::JPEGCodec::Encode(bitmap, kDefaultEncodingQuality, &(*output)->data());

  if (output_skia) {
    resized_image.MakeThreadSafe();
    *output_skia = resized_image;
  }

  return true;
}

// static
bool WallpaperManager::ResizeAndSaveWallpaper(const gfx::ImageSkia& image,
                                              const base::FilePath& path,
                                              wallpaper::WallpaperLayout layout,
                                              int preferred_width,
                                              int preferred_height,
                                              gfx::ImageSkia* output_skia) {
  if (layout == wallpaper::WALLPAPER_LAYOUT_CENTER) {
    // TODO(bshe): Generates cropped custom wallpaper for CENTER layout.
    if (base::PathExists(path))
      base::DeleteFile(path, false);
    return false;
  }
  scoped_refptr<base::RefCountedBytes> data;
  if (ResizeImage(image, layout, preferred_width, preferred_height, &data,
                  output_skia)) {
    return SaveWallpaperInternal(
        path, reinterpret_cast<const char*>(data->front()), data->size());
  }
  return false;
}

// static
base::FilePath WallpaperManager::GetCustomWallpaperPath(
    const char* sub_dir,
    const wallpaper::WallpaperFilesId& wallpaper_files_id,
    const std::string& file) {
  base::FilePath custom_wallpaper_path = GetCustomWallpaperDir(sub_dir);
  return custom_wallpaper_path.Append(wallpaper_files_id.id()).Append(file);
}

void WallpaperManager::SetCustomWallpaper(
    const AccountId& account_id,
    const wallpaper::WallpaperFilesId& wallpaper_files_id,
    const std::string& file,
    wallpaper::WallpaperLayout layout,
    wallpaper::WallpaperType type,
    const gfx::ImageSkia& image,
    bool update_wallpaper) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // There is no visible wallpaper in kiosk mode.
  if (user_manager::UserManager::Get()->IsLoggedInAsKioskApp())
    return;

  // Don't allow custom wallpapers while policy is in effect.
  if (type != wallpaper::POLICY && IsPolicyControlled(account_id))
    return;

  base::FilePath wallpaper_path = GetCustomWallpaperPath(
      kOriginalWallpaperSubDir, wallpaper_files_id, file);

  // If decoded wallpaper is empty, we have probably failed to decode the file.
  // Use default wallpaper in this case.
  if (image.isNull()) {
    GetPendingWallpaper()->SetDefaultWallpaper(account_id);
    return;
  }

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  CHECK(user);
  const bool is_persistent =
      !user_manager::UserManager::Get()->IsUserNonCryptohomeDataEphemeral(
          account_id) ||
      (type == wallpaper::POLICY &&
       user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT);

  WallpaperInfo wallpaper_info = {
      wallpaper_path.value(),
      layout,
      type,
      // Date field is not used.
      base::Time::Now().LocalMidnight()
  };
  if (is_persistent) {
    image.EnsureRepsForSupportedScales();
    std::unique_ptr<gfx::ImageSkia> deep_copy(image.DeepCopy());
    // Block shutdown on this task. Otherwise, we may lose the custom wallpaper
    // that the user selected.
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner =
        base::CreateSequencedTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
    // TODO(bshe): This may break if RawImage becomes RefCountedMemory.
    blocking_task_runner->PostTask(
        FROM_HERE, base::BindOnce(&WallpaperManager::SaveCustomWallpaper,
                                  wallpaper_files_id,
                                  base::FilePath(wallpaper_info.location),
                                  wallpaper_info.layout,
                                  base::Passed(std::move(deep_copy))));
  }

  std::string relative_path =
      base::FilePath(wallpaper_files_id.id()).Append(file).value();
  // User's custom wallpaper path is determined by relative path and the
  // appropriate wallpaper resolution in GetCustomWallpaperInternal.
  WallpaperInfo info = {relative_path, layout, type,
                        base::Time::Now().LocalMidnight()};
  SetUserWallpaperInfo(account_id, info, is_persistent);
  if (update_wallpaper) {
    GetPendingWallpaper()->SetWallpaperFromImage(account_id, image, info);
  }

  wallpaper_cache_[account_id] = CustomWallpaperElement(wallpaper_path, image);
}

void WallpaperManager::SetDefaultWallpaper(const AccountId& account_id,
                                           bool update_wallpaper) {
  RemoveUserWallpaperInfo(account_id);
  const wallpaper::WallpaperInfo info = {
      std::string(), wallpaper::WALLPAPER_LAYOUT_CENTER, wallpaper::DEFAULT,
      base::Time::Now().LocalMidnight()};
  const bool is_persistent =
      !user_manager::UserManager::Get()->IsUserNonCryptohomeDataEphemeral(
          account_id);
  SetUserWallpaperInfo(account_id, info, is_persistent);

  if (update_wallpaper)
    GetPendingWallpaper()->SetDefaultWallpaper(account_id);
}

void WallpaperManager::SetUserWallpaperInfo(const AccountId& account_id,
                                            const WallpaperInfo& info,
                                            bool is_persistent) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  current_user_wallpaper_info_ = info;
  if (!is_persistent)
    return;

  PrefService* local_state = g_browser_process->local_state();
  // Remove the color cache of the previous wallpaper if it exists.
  WallpaperInfo old_info;
  if (GetUserWallpaperInfo(account_id, &old_info)) {
    DictionaryPrefUpdate wallpaper_colors_update(local_state,
                                                 ash::prefs::kWallpaperColors);
    wallpaper_colors_update->RemoveWithoutPathExpansion(old_info.location,
                                                        nullptr);
  }
  DictionaryPrefUpdate wallpaper_update(local_state, kUsersWallpaperInfo);

  auto wallpaper_info_dict = base::MakeUnique<base::DictionaryValue>();
  wallpaper_info_dict->SetString(
      kNewWallpaperDateNodeName,
      base::Int64ToString(info.date.ToInternalValue()));
  wallpaper_info_dict->SetString(kNewWallpaperLocationNodeName, info.location);
  wallpaper_info_dict->SetInteger(kNewWallpaperLayoutNodeName, info.layout);
  wallpaper_info_dict->SetInteger(kNewWallpaperTypeNodeName, info.type);
  wallpaper_update->SetWithoutPathExpansion(account_id.GetUserEmail(),
                                            std::move(wallpaper_info_dict));
}

void WallpaperManager::SetWallpaperFromImageSkia(
    const AccountId& account_id,
    const gfx::ImageSkia& image,
    wallpaper::WallpaperLayout layout,
    bool update_wallpaper) {
  DCHECK(user_manager::UserManager::Get()->IsUserLoggedIn());

  // There is no visible wallpaper in kiosk mode.
  if (user_manager::UserManager::Get()->IsLoggedInAsKioskApp())
    return;
  WallpaperInfo info;
  info.layout = layout;

  // This is an API call and we do not know the path. So we set the image, but
  // no path.
  wallpaper_cache_[account_id] =
      CustomWallpaperElement(base::FilePath(), image);

  if (update_wallpaper)
    GetPendingWallpaper()->SetWallpaperFromImage(account_id, image, info);
}

void WallpaperManager::InitializeWallpaper() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  // Apply device customization.
  if (ShouldUseCustomizedDefaultWallpaper()) {
    SetDefaultWallpaperPath(
        GetCustomizedWallpaperDefaultRescaledFileName(kSmallWallpaperSuffix),
        std::unique_ptr<gfx::ImageSkia>(),
        GetCustomizedWallpaperDefaultRescaledFileName(kLargeWallpaperSuffix),
        std::unique_ptr<gfx::ImageSkia>());
  }

  base::CommandLine* command_line = GetCommandLine();
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
    ash::Shell::Get()->wallpaper_controller()->CreateEmptyWallpaper();
    return;
  }

  if (!user_manager->IsUserLoggedIn()) {
    if (!StartupUtils::IsDeviceRegistered())
      GetPendingWallpaper()->SetDefaultWallpaper(
          user_manager::SignInAccountId());
    else
      InitializeRegisteredDeviceWallpaper();
    return;
  }
  SetUserWallpaper(user_manager->GetActiveUser()->GetAccountId());
}

void WallpaperManager::UpdateWallpaper(bool clear_cache) {
  // For GAIA login flow, the last_selected_user_ may not be set before user
  // login. If UpdateWallpaper is called at GAIA login screen, no wallpaper will
  // be set. It could result a black screen on external monitors.
  // See http://crbug.com/265689 for detail.
  if (last_selected_user_.empty())
    GetPendingWallpaper()->SetDefaultWallpaper(user_manager::SignInAccountId());

  for (auto& observer : observers_)
    observer.OnUpdateWallpaperForTesting();
  if (clear_cache)
    wallpaper_cache_.clear();
  SetUserWallpaper(last_selected_user_);
}

bool WallpaperManager::IsPendingWallpaper(uint32_t image_id) {
  for (size_t i = 0; i < loading_.size(); ++i) {
    if (loading_[i]->GetImageId() == image_id) {
      return true;
    }
  }
  return false;
}

WallpaperManager::WallpaperResolution
WallpaperManager::GetAppropriateResolution() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  gfx::Size size = ash::WallpaperController::GetMaxDisplaySizeInNative();
  return (size.width() > kSmallWallpaperMaxWidth ||
          size.height() > kSmallWallpaperMaxHeight)
             ? WALLPAPER_RESOLUTION_LARGE
             : WALLPAPER_RESOLUTION_SMALL;
}

bool WallpaperManager::GetLoggedInUserWallpaperInfo(WallpaperInfo* info) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (user_manager::UserManager::Get()->IsLoggedInAsStub()) {
    info->location = current_user_wallpaper_info_.location = "";
    info->layout = current_user_wallpaper_info_.layout =
        wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED;
    info->type = current_user_wallpaper_info_.type = wallpaper::DEFAULT;
    info->date = current_user_wallpaper_info_.date =
        base::Time::Now().LocalMidnight();
    return true;
  }

  return GetUserWallpaperInfo(
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId(), info);
}

void WallpaperManager::AddObservers() {
  show_user_name_on_signin_subscription_ =
      CrosSettings::Get()->AddSettingsObserver(
          kAccountsPrefShowUserNamesOnSignIn,
          base::Bind(&WallpaperManager::InitializeRegisteredDeviceWallpaper,
                     weak_factory_.GetWeakPtr()));
  device_wallpaper_image_subscription_ =
      CrosSettings::Get()->AddSettingsObserver(
          kDeviceWallpaperImage,
          base::Bind(&WallpaperManager::OnDeviceWallpaperPolicyChanged,
                     weak_factory_.GetWeakPtr()));
}

void WallpaperManager::SetCommandLineForTesting(
    base::CommandLine* command_line) {
  command_line_for_testing_ = command_line;
  SetDefaultWallpaperPathsFromCommandLine(command_line);
}

void WallpaperManager::EnsureLoggedInUserWallpaperLoaded() {
  WallpaperInfo info;
  if (GetLoggedInUserWallpaperInfo(&info)) {
    UMA_HISTOGRAM_ENUMERATION("Ash.Wallpaper.Type", info.type,
                              wallpaper::WALLPAPER_TYPE_COUNT);
    RecordWallpaperAppType();
    if (info == current_user_wallpaper_info_)
      return;
  }
  SetUserWallpaper(
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());
}

void WallpaperManager::RemoveUserWallpaperInfo(const AccountId& account_id) {
  if (wallpaper_cache_.find(account_id) != wallpaper_cache_.end())
    wallpaper_cache_.erase(account_id);

  PrefService* prefs = g_browser_process->local_state();
  // PrefService could be NULL in tests.
  if (!prefs)
    return;
  WallpaperInfo info;
  GetUserWallpaperInfo(account_id, &info);
  DictionaryPrefUpdate prefs_wallpapers_info_update(prefs, kUsersWallpaperInfo);
  prefs_wallpapers_info_update->RemoveWithoutPathExpansion(
      account_id.GetUserEmail(), NULL);
  // Remove the color cache of the previous wallpaper if it exists.
  DictionaryPrefUpdate wallpaper_colors_update(prefs,
                                               ash::prefs::kWallpaperColors);
  wallpaper_colors_update->RemoveWithoutPathExpansion(info.location, nullptr);
  DeleteUserWallpapers(account_id);
}

void WallpaperManager::OnPolicyFetched(const std::string& policy,
                                       const AccountId& account_id,
                                       std::unique_ptr<std::string> data) {
  if (!data)
    return;

  user_image_loader::StartWithData(
      task_runner_, std::move(data), ImageDecoder::ROBUST_JPEG_CODEC,
      0,  // Do not crop.
      base::Bind(&WallpaperManager::SetPolicyControlledWallpaper,
                 weak_factory_.GetWeakPtr(), account_id));
}

size_t WallpaperManager::GetPendingListSizeForTesting() const {
  return loading_.size();
}

wallpaper::WallpaperFilesId WallpaperManager::GetFilesId(
    const AccountId& account_id) const {
  std::string stored_value;
  if (user_manager::known_user::GetStringPref(account_id, kWallpaperFilesId,
                                              &stored_value)) {
    return wallpaper::WallpaperFilesId::FromString(stored_value);
  }
  const std::string& old_id = account_id.GetUserEmail();  // Migrated
  const wallpaper::WallpaperFilesId files_id = HashWallpaperFilesIdStr(old_id);
  SetKnownUserWallpaperFilesId(account_id, files_id);
  return files_id;
}

bool WallpaperManager::SetDeviceWallpaperIfApplicable(
    const AccountId& account_id) {
  std::string url;
  std::string hash;
  if (ShouldSetDeviceWallpaper(account_id, &url, &hash)) {
    // Check if the device wallpaper exists and matches the hash. If so, use it
    // directly. Otherwise download it first.
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::Bind(&base::PathExists, GetDeviceWallpaperFilePath()),
        base::Bind(&WallpaperManager::OnDeviceWallpaperExists,
                   weak_factory_.GetWeakPtr(), account_id, url, hash));
    return true;
  }
  return false;
}

bool WallpaperManager::IsPolicyControlled(const AccountId& account_id) const {
  WallpaperInfo info;
  if (!GetUserWallpaperInfo(account_id, &info))
    return false;
  return info.type == wallpaper::POLICY;
}

void WallpaperManager::OnPolicySet(const std::string& policy,
                                   const AccountId& account_id) {
  WallpaperInfo info;
  GetUserWallpaperInfo(account_id, &info);
  info.type = wallpaper::POLICY;
  SetUserWallpaperInfo(account_id, info, true /* is_persistent */);
}

void WallpaperManager::OnPolicyCleared(const std::string& policy,
                                       const AccountId& account_id) {
  WallpaperInfo info;
  GetUserWallpaperInfo(account_id, &info);
  info.type = wallpaper::DEFAULT;
  SetUserWallpaperInfo(account_id, info, true /* is_persistent */);

  // If we're at the login screen, do not change the wallpaper but defer it
  // until the user logs in to the system.
  if (user_manager::UserManager::Get()->IsUserLoggedIn())
    GetPendingWallpaper()->SetDefaultWallpaper(account_id);
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
  std::unique_ptr<CustomizedWallpaperRescaledFiles> rescaled_files(
      new CustomizedWallpaperRescaledFiles(
          downloaded_file,
          resized_directory.Append(downloaded_file_name +
                                   kSmallWallpaperSuffix),
          resized_directory.Append(downloaded_file_name +
                                   kLargeWallpaperSuffix)));

  base::Closure check_file_exists = rescaled_files->CreateCheckerClosure();
  base::Closure on_checked_closure =
      base::Bind(&WallpaperManager::SetCustomizedDefaultWallpaperAfterCheck,
                 weak_factory_.GetWeakPtr(), wallpaper_url, downloaded_file,
                 base::Passed(std::move(rescaled_files)));
  if (!task_runner_->PostTaskAndReply(FROM_HERE, check_file_exists,
                                      on_checked_closure)) {
    LOG(WARNING) << "Failed to start check CheckCustomizedWallpaperFilesExist.";
  }
}

void WallpaperManager::AddObserver(WallpaperManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void WallpaperManager::RemoveObserver(WallpaperManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void WallpaperManager::Open() {
  if (wallpaper_manager_util::ShouldUseAndroidWallpapersApp(
          ProfileHelper::Get()->GetProfileByUser(
              user_manager::UserManager::Get()->GetActiveUser())) &&
      !ash_util::IsRunningInMash()) {
    // Window activation watch to minimize all inactive windows is only needed
    // by Android Wallpaper app. Legacy Chrome OS Wallpaper Picker app does that
    // via extension API.
    activation_client_observer_.Add(ash::Shell::Get()->activation_client());
  }
  wallpaper_manager_util::OpenWallpaperManager();
}

void WallpaperManager::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_USER_CHANGED: {
      ClearDisposableWallpaperCache();
      BrowserThread::PostDelayedTask(
          BrowserThread::UI, FROM_HERE,
          base::BindOnce(&WallpaperManager::MoveLoggedInUserCustomWallpaper,
                         weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromSeconds(kMoveCustomWallpaperDelaySeconds));
      break;
    }
    case chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE: {
      if (!GetCommandLine()->HasSwitch(switches::kDisableBootAnimation)) {
        BrowserThread::PostDelayedTask(
            BrowserThread::UI, FROM_HERE,
            base::BindOnce(&WallpaperManager::CacheUsersWallpapers,
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
            base::BindOnce(&WallpaperManager::CacheUsersWallpapers,
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

void WallpaperManager::OnChildStatusChanged(const user_manager::User& user) {
  SetUserWallpaper(user.GetAccountId());
}

void WallpaperManager::OnWindowActivated(ActivationReason reason,
                                         aura::Window* gained_active,
                                         aura::Window* lost_active) {
  if (!gained_active)
    return;

  const std::string arc_wallpapers_app_id = ArcAppListPrefs::GetAppId(
      wallpaper_manager_util::kAndroidWallpapersAppPackage,
      wallpaper_manager_util::kAndroidWallpapersAppActivity);
  ash::ShelfID shelf_id =
      ash::ShelfID::Deserialize(gained_active->GetProperty(ash::kShelfIDKey));
  if (shelf_id.app_id == arc_wallpapers_app_id) {
    ash::WallpaperWindowStateManager::MinimizeInactiveWindows(
        user_manager::UserManager::Get()->GetActiveUser()->username_hash());
    DCHECK(!ash_util::IsRunningInMash() && ash::Shell::Get());
    activation_client_observer_.Remove(ash::Shell::Get()->activation_client());
    window_observer_.Add(gained_active);
  }
}

void WallpaperManager::OnWindowDestroying(aura::Window* window) {
  window_observer_.Remove(window);
  ash::WallpaperWindowStateManager::RestoreWindows(
      user_manager::UserManager::Get()->GetActiveUser()->username_hash());
}

// WallpaperManager, private: --------------------------------------------------

WallpaperManager::WallpaperManager()
    : binding_(this),
      activation_client_observer_(this),
      window_observer_(this),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SetPathIds(chrome::DIR_USER_DATA, chrome::DIR_CHROMEOS_WALLPAPERS,
             chrome::DIR_CHROMEOS_CUSTOM_WALLPAPERS);
  SetDefaultWallpaperPathsFromCommandLine(
      base::CommandLine::ForCurrentProcess());
  registrar_.Add(this, chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_WALLPAPER_ANIMATION_FINISHED,
                 content::NotificationService::AllSources());
  task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  user_manager::UserManager::Get()->AddObserver(this);

  content::ServiceManagerConnection* connection =
      content::ServiceManagerConnection::GetForProcess();
  if (connection && connection->GetConnector()) {
    // Connect to the wallpaper controller interface in the ash service.
    ash::mojom::WallpaperControllerPtr wallpaper_controller_ptr;
    connection->GetConnector()->BindInterface(ash::mojom::kServiceName,
                                              &wallpaper_controller_ptr);
    // Register this object as the wallpaper picker.
    ash::mojom::WallpaperPickerPtr picker;
    binding_.Bind(mojo::MakeRequest(&picker));
    wallpaper_controller_ptr->SetWallpaperPicker(std::move(picker));
  }
}

// static
void WallpaperManager::SaveCustomWallpaper(
    const wallpaper::WallpaperFilesId& wallpaper_files_id,
    const base::FilePath& original_path,
    wallpaper::WallpaperLayout layout,
    std::unique_ptr<gfx::ImageSkia> image) {
  base::DeleteFile(GetCustomWallpaperDir(kOriginalWallpaperSubDir)
                       .Append(wallpaper_files_id.id()),
                   true /* recursive */);
  base::DeleteFile(GetCustomWallpaperDir(kSmallWallpaperSubDir)
                       .Append(wallpaper_files_id.id()),
                   true /* recursive */);
  base::DeleteFile(GetCustomWallpaperDir(kLargeWallpaperSubDir)
                       .Append(wallpaper_files_id.id()),
                   true /* recursive */);
  EnsureCustomWallpaperDirectories(wallpaper_files_id);
  std::string file_name = original_path.BaseName().value();
  base::FilePath small_wallpaper_path = GetCustomWallpaperPath(
      kSmallWallpaperSubDir, wallpaper_files_id, file_name);
  base::FilePath large_wallpaper_path = GetCustomWallpaperPath(
      kLargeWallpaperSubDir, wallpaper_files_id, file_name);

  // Re-encode orginal file to jpeg format and saves the result in case that
  // resized wallpaper is not generated (i.e. chrome shutdown before resized
  // wallpaper is saved).
  ResizeAndSaveWallpaper(*image, original_path,
                         wallpaper::WALLPAPER_LAYOUT_STRETCH, image->width(),
                         image->height(), nullptr);
  ResizeAndSaveWallpaper(*image, small_wallpaper_path, layout,
                         kSmallWallpaperMaxWidth, kSmallWallpaperMaxHeight,
                         nullptr);
  ResizeAndSaveWallpaper(*image, large_wallpaper_path, layout,
                         kLargeWallpaperMaxWidth, kLargeWallpaperMaxHeight,
                         nullptr);
}

// static
void WallpaperManager::MoveCustomWallpapersOnWorker(
    const AccountId& account_id,
    const wallpaper::WallpaperFilesId& wallpaper_files_id,
    const scoped_refptr<base::SingleThreadTaskRunner>& reply_task_runner,
    base::WeakPtr<WallpaperManager> weak_ptr) {
  const std::string& temporary_wallpaper_dir =
      account_id.GetUserEmail();  // Migrated
  if (MoveCustomWallpaperDirectory(kOriginalWallpaperSubDir,
                                   temporary_wallpaper_dir,
                                   wallpaper_files_id.id())) {
    // Consider success if the original wallpaper is moved to the new directory.
    // Original wallpaper is the fallback if the correct resolution wallpaper
    // can not be found.
    reply_task_runner->PostTask(
        FROM_HERE, base::Bind(&WallpaperManager::MoveCustomWallpapersSuccess,
                              weak_ptr, account_id, wallpaper_files_id));
  }
  MoveCustomWallpaperDirectory(kLargeWallpaperSubDir, temporary_wallpaper_dir,
                               wallpaper_files_id.id());
  MoveCustomWallpaperDirectory(kSmallWallpaperSubDir, temporary_wallpaper_dir,
                               wallpaper_files_id.id());
  MoveCustomWallpaperDirectory(kThumbnailWallpaperSubDir,
                               temporary_wallpaper_dir,
                               wallpaper_files_id.id());
}

// static
void WallpaperManager::GetCustomWallpaperInternal(
    const AccountId& account_id,
    const WallpaperInfo& info,
    const base::FilePath& wallpaper_path,
    bool update_wallpaper,
    const scoped_refptr<base::SingleThreadTaskRunner>& reply_task_runner,
    MovableOnDestroyCallbackHolder on_finish,
    base::WeakPtr<WallpaperManager> weak_ptr) {
  base::FilePath valid_path = wallpaper_path;
  if (!base::PathExists(wallpaper_path)) {
    // Falls back on original file if the correct resolution file does not
    // exist. This may happen when the original custom wallpaper is small or
    // browser shutdown before resized wallpaper saved.
    valid_path = GetCustomWallpaperDir(kOriginalWallpaperSubDir);
    valid_path = valid_path.Append(info.location);
  }

  if (!base::PathExists(valid_path)) {
    // Falls back to custom wallpaper that uses AccountId as part of its file
    // path.
    // Note that account id is used instead of wallpaper_files_id here.
    const std::string& old_path = account_id.GetUserEmail();  // Migrated
    valid_path = GetCustomWallpaperPath(
        kOriginalWallpaperSubDir,
        wallpaper::WallpaperFilesId::FromString(old_path), info.location);
  }

  if (!base::PathExists(valid_path)) {
    reply_task_runner->PostTask(
        FROM_HERE,
        base::Bind(&WallpaperManager::OnCustomWallpaperFileNotFound, weak_ptr,
                   account_id, wallpaper_path, update_wallpaper,
                   base::Passed(std::move(on_finish))));
  } else {
    reply_task_runner->PostTask(
        FROM_HERE, base::Bind(&WallpaperManager::StartLoad, weak_ptr,
                              account_id, info, update_wallpaper, valid_path,
                              base::Passed(std::move(on_finish))));
  }
}

void WallpaperManager::ResizeCustomizedDefaultWallpaper(
    std::unique_ptr<gfx::ImageSkia> image,
    const CustomizedWallpaperRescaledFiles* rescaled_files,
    bool* success,
    gfx::ImageSkia* small_wallpaper_image,
    gfx::ImageSkia* large_wallpaper_image) {
  *success = true;

  *success &= ResizeAndSaveWallpaper(
      *image, rescaled_files->path_rescaled_small(),
      wallpaper::WALLPAPER_LAYOUT_STRETCH, kSmallWallpaperMaxWidth,
      kSmallWallpaperMaxHeight, small_wallpaper_image);

  *success &= ResizeAndSaveWallpaper(
      *image, rescaled_files->path_rescaled_large(),
      wallpaper::WALLPAPER_LAYOUT_STRETCH, kLargeWallpaperMaxWidth,
      kLargeWallpaperMaxHeight, large_wallpaper_image);
}

void WallpaperManager::InitInitialUserWallpaper(const AccountId& account_id,
                                                bool is_persistent) {
  current_user_wallpaper_info_.location = "";
  current_user_wallpaper_info_.layout =
      wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED;
  current_user_wallpaper_info_.type = wallpaper::DEFAULT;
  current_user_wallpaper_info_.date = base::Time::Now().LocalMidnight();

  WallpaperInfo info = current_user_wallpaper_info_;
  SetUserWallpaperInfo(account_id, info, is_persistent);
}

bool WallpaperManager::GetWallpaperFromCache(const AccountId& account_id,
                                             gfx::ImageSkia* image) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CustomWallpaperMap::const_iterator it = wallpaper_cache_.find(account_id);
  if (it != wallpaper_cache_.end() && !(*it).second.second.isNull()) {
    *image = (*it).second.second;
    return true;
  }
  return false;
}

bool WallpaperManager::GetPathFromCache(const AccountId& account_id,
                                        base::FilePath* path) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CustomWallpaperMap::const_iterator it = wallpaper_cache_.find(account_id);
  if (it != wallpaper_cache_.end()) {
    *path = (*it).second.first;
    return true;
  }
  return false;
}

int WallpaperManager::loaded_wallpapers_for_test() const {
  return loaded_wallpapers_for_test_;
}

void WallpaperManager::CacheUsersWallpapers() {
  // TODO(dpolukhin): crbug.com/408734.
  DCHECK(thread_checker_.CalledOnValidThread());
  user_manager::UserList users = user_manager::UserManager::Get()->GetUsers();

  if (!users.empty()) {
    user_manager::UserList::const_iterator it = users.begin();
    // Skip the wallpaper of first user in the list. It should have been cached.
    it++;
    for (int cached = 0; it != users.end() && cached < kMaxWallpapersToCache;
         ++it, ++cached) {
      CacheUserWallpaper((*it)->GetAccountId());
    }
  }
}

void WallpaperManager::CacheUserWallpaper(const AccountId& account_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CustomWallpaperMap::iterator it = wallpaper_cache_.find(account_id);
  if (it != wallpaper_cache_.end() && !it->second.second.isNull())
    return;
  WallpaperInfo info;
  if (GetUserWallpaperInfo(account_id, &info)) {
    if (info.location.empty())
      return;

    base::FilePath wallpaper_dir;
    base::FilePath wallpaper_path;
    if (info.type == wallpaper::CUSTOMIZED || info.type == wallpaper::POLICY ||
        info.type == wallpaper::DEVICE) {
      base::FilePath wallpaper_path;
      if (info.type == wallpaper::DEVICE) {
        wallpaper_path = GetDeviceWallpaperFilePath();
      } else {
        const char* sub_dir = GetCustomWallpaperSubdirForCurrentResolution();
        wallpaper_path = GetCustomWallpaperDir(sub_dir).Append(info.location);
      }
      // Set the path to the cache.
      wallpaper_cache_[account_id] =
          CustomWallpaperElement(wallpaper_path, gfx::ImageSkia());
      task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&WallpaperManager::GetCustomWallpaperInternal, account_id,
                     info, wallpaper_path, false /* do not update wallpaper */,
                     base::ThreadTaskRunnerHandle::Get(),
                     base::Passed(MovableOnDestroyCallbackHolder()),
                     weak_factory_.GetWeakPtr()));
      return;
    }
    LoadWallpaper(account_id, info, false /* do not update wallpaper */,
                  MovableOnDestroyCallbackHolder());
  }
}

void WallpaperManager::ClearDisposableWallpaperCache() {
  // Cancel callback for previous cache requests.
  weak_factory_.InvalidateWeakPtrs();
  // Keep the wallpaper of logged in users in cache at multi-profile mode.
  std::set<AccountId> logged_in_user_account_ids;
  const user_manager::UserList& logged_users =
      user_manager::UserManager::Get()->GetLoggedInUsers();
  for (user_manager::UserList::const_iterator it = logged_users.begin();
       it != logged_users.end(); ++it) {
    logged_in_user_account_ids.insert((*it)->GetAccountId());
  }

  CustomWallpaperMap logged_in_users_cache;
  for (CustomWallpaperMap::iterator it = wallpaper_cache_.begin();
       it != wallpaper_cache_.end(); ++it) {
    if (logged_in_user_account_ids.find(it->first) !=
        logged_in_user_account_ids.end()) {
      logged_in_users_cache.insert(*it);
    }
  }
  wallpaper_cache_ = logged_in_users_cache;
}

void WallpaperManager::DeleteUserWallpapers(const AccountId& account_id) {
  // System salt might not be ready in tests. Thus we don't have a valid
  // wallpaper files id here.
  if (!CanGetWallpaperFilesId())
    return;

  std::vector<base::FilePath> file_to_remove;
  wallpaper::WallpaperFilesId wallpaper_files_id = GetFilesId(account_id);

  // Remove small user wallpapers.
  base::FilePath wallpaper_path = GetCustomWallpaperDir(kSmallWallpaperSubDir);
  file_to_remove.push_back(wallpaper_path.Append(wallpaper_files_id.id()));

  // Remove large user wallpapers.
  wallpaper_path = GetCustomWallpaperDir(kLargeWallpaperSubDir);
  file_to_remove.push_back(wallpaper_path.Append(wallpaper_files_id.id()));

  // Remove user wallpaper thumbnails.
  wallpaper_path = GetCustomWallpaperDir(kThumbnailWallpaperSubDir);
  file_to_remove.push_back(wallpaper_path.Append(wallpaper_files_id.id()));

  // Remove original user wallpapers.
  wallpaper_path = GetCustomWallpaperDir(kOriginalWallpaperSubDir);
  file_to_remove.push_back(wallpaper_path.Append(wallpaper_files_id.id()));

  base::PostTaskWithTraits(FROM_HERE,
                           {base::MayBlock(), base::TaskPriority::BACKGROUND,
                            base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
                           base::Bind(&DeleteWallpaperInList, file_to_remove));
}

base::CommandLine* WallpaperManager::GetCommandLine() {
  base::CommandLine* command_line =
      command_line_for_testing_ ? command_line_for_testing_
                                : base::CommandLine::ForCurrentProcess();
  return command_line;
}

void WallpaperManager::InitializeRegisteredDeviceWallpaper() {
  if (user_manager::UserManager::Get()->IsUserLoggedIn())
    return;

  bool disable_boot_animation =
      GetCommandLine()->HasSwitch(switches::kDisableBootAnimation);
  bool show_users = true;
  bool result = CrosSettings::Get()->GetBoolean(
      kAccountsPrefShowUserNamesOnSignIn, &show_users);
  DCHECK(result) << "Unable to fetch setting "
                 << kAccountsPrefShowUserNamesOnSignIn;
  const user_manager::UserList& users =
      user_manager::UserManager::Get()->GetUsers();
  int public_session_user_index = FindPublicSession(users);
  if ((!show_users && public_session_user_index == -1) ||
      !HasNonDeviceLocalAccounts(users)) {
    // Boot into sign in form, preload default wallpaper.
    if (!SetDeviceWallpaperIfApplicable(user_manager::SignInAccountId()))
      GetPendingWallpaper()->SetDefaultWallpaper(
          user_manager::SignInAccountId());
    return;
  }

  if (!disable_boot_animation) {
    int index = public_session_user_index != -1 ? public_session_user_index : 0;
    // Normal boot, load user wallpaper.
    // If normal boot animation is disabled wallpaper would be set
    // asynchronously once user pods are loaded.
    SetUserWallpaper(users[index]->GetAccountId());
  }
}

void WallpaperManager::LoadWallpaper(const AccountId& account_id,
                                     const WallpaperInfo& info,
                                     bool update_wallpaper,
                                     MovableOnDestroyCallbackHolder on_finish) {
  base::FilePath wallpaper_dir;
  base::FilePath wallpaper_path;

  // Do a sanity check that file path information is not empty.
  if (info.type == wallpaper::ONLINE || info.type == wallpaper::DEFAULT) {
    if (info.location.empty()) {
      if (base::SysInfo::IsRunningOnChromeOS()) {
        NOTREACHED() << "User wallpaper info appears to be broken: "
                     << account_id.Serialize();
      } else {
        // Filename might be empty on debug configurations when stub users
        // were created directly in Local State (for testing). Ignore such
        // errors i.e. allowsuch type of debug configurations on the desktop.
        LOG(WARNING) << "User wallpaper info is empty: "
                     << account_id.Serialize();

        // |on_finish| callback will get called on destruction.
        return;
      }
    }
  }

  if (info.type == wallpaper::ONLINE) {
    std::string file_name = GURL(info.location).ExtractFileName();
    WallpaperResolution resolution = GetAppropriateResolution();
    // Only solid color wallpapers have stretch layout and they have only one
    // resolution.
    if (info.layout != wallpaper::WALLPAPER_LAYOUT_STRETCH &&
        resolution == WALLPAPER_RESOLUTION_SMALL) {
      file_name = base::FilePath(file_name)
                      .InsertBeforeExtension(kSmallWallpaperSuffix)
                      .value();
    }
    DCHECK(dir_chromeos_wallpapers_path_id != -1);
    CHECK(PathService::Get(dir_chromeos_wallpapers_path_id, &wallpaper_dir));
    wallpaper_path = wallpaper_dir.Append(file_name);

    // If the wallpaper exists and it contains already the correct image we can
    // return immediately.
    CustomWallpaperMap::iterator it = wallpaper_cache_.find(account_id);
    if (it != wallpaper_cache_.end() && it->second.first == wallpaper_path &&
        !it->second.second.isNull())
      return;

    loaded_wallpapers_for_test_++;
    StartLoad(account_id, info, update_wallpaper, wallpaper_path,
              std::move(on_finish));
  } else if (info.type == wallpaper::DEFAULT) {
    // Default wallpapers are migrated from M21 user profiles. A code refactor
    // overlooked that case and caused these wallpapers not being loaded at all.
    // On some slow devices, it caused login webui not visible after upgrade to
    // M26 from M21. See crosbug.com/38429 for details.
    base::FilePath user_data_dir;
    DCHECK(dir_user_data_path_id != -1);
    PathService::Get(dir_user_data_path_id, &user_data_dir);
    wallpaper_path = user_data_dir.Append(info.location);
    StartLoad(account_id, info, update_wallpaper, wallpaper_path,
              std::move(on_finish));
  } else {
    // In unexpected cases, revert to default wallpaper to fail safely. See
    // crosbug.com/38429.
    LOG(ERROR) << "Wallpaper reverts to default unexpected.";
    DoSetDefaultWallpaper(account_id, update_wallpaper, std::move(on_finish));
  }
}

void WallpaperManager::MoveCustomWallpapersSuccess(
    const AccountId& account_id,
    const wallpaper::WallpaperFilesId& wallpaper_files_id) {
  WallpaperInfo info;
  GetUserWallpaperInfo(account_id, &info);
  if (info.type == wallpaper::CUSTOMIZED) {
    // New file field should include user wallpaper_files_id in addition to
    // file name.  This is needed because at login screen, wallpaper_files_id
    // is not available.
    info.location =
        base::FilePath(wallpaper_files_id.id()).Append(info.location).value();
    bool is_persistent =
        !user_manager::UserManager::Get()->IsUserNonCryptohomeDataEphemeral(
            account_id);
    SetUserWallpaperInfo(account_id, info, is_persistent);
  }
}

void WallpaperManager::MoveLoggedInUserCustomWallpaper() {
  DCHECK(thread_checker_.CalledOnValidThread());
  const user_manager::User* logged_in_user =
      user_manager::UserManager::Get()->GetActiveUser();
  if (logged_in_user) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&WallpaperManager::MoveCustomWallpapersOnWorker,
                              logged_in_user->GetAccountId(),
                              GetFilesId(logged_in_user->GetAccountId()),
                              base::ThreadTaskRunnerHandle::Get(),
                              weak_factory_.GetWeakPtr()));
  }
}

bool WallpaperManager::GetUserWallpaperInfo(const AccountId& account_id,
                                            WallpaperInfo* info) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (user_manager::UserManager::Get()->IsUserNonCryptohomeDataEphemeral(
          account_id)) {
    // Default to the values cached in memory.
    *info = current_user_wallpaper_info_;

    // Ephemeral users do not save anything to local state. But we have got
    // wallpaper info from memory. Returns true.
    return true;
  }

  const base::DictionaryValue* info_dict;
  if (!g_browser_process->local_state()
           ->GetDictionary(kUsersWallpaperInfo)
           ->GetDictionaryWithoutPathExpansion(account_id.GetUserEmail(),
                                               &info_dict)) {
    return false;
  }

  // Use temporary variables to keep |info| untouched in the error case.
  std::string location;
  if (!info_dict->GetString(kNewWallpaperLocationNodeName, &location))
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
  int64_t date_val;
  if (!base::StringToInt64(date_string, &date_val))
    return false;

  info->location = location;
  info->layout = static_cast<wallpaper::WallpaperLayout>(layout);
  info->type = static_cast<wallpaper::WallpaperType>(type);
  info->date = base::Time::FromInternalValue(date_val);
  return true;
}

bool WallpaperManager::ShouldSetDeviceWallpaper(const AccountId& account_id,
                                                std::string* url,
                                                std::string* hash) {
  // Only allow the device wallpaper for enterprise managed devices.
  if (!g_browser_process->platform_part()
           ->browser_policy_connector_chromeos()
           ->IsEnterpriseManaged()) {
    return false;
  }

  const base::DictionaryValue* dict = nullptr;
  if (!CrosSettings::Get()->GetDictionary(kDeviceWallpaperImage, &dict) ||
      !dict->GetStringWithoutPathExpansion("url", url) ||
      !dict->GetStringWithoutPathExpansion("hash", hash)) {
    return false;
  }

  // Only set the device wallpaper if we're at the login screen.
  if (user_manager::UserManager::Get()->IsUserLoggedIn())
    return false;

  return true;
}

base::FilePath WallpaperManager::GetDeviceWallpaperDir() {
  base::FilePath wallpaper_dir;
  if (!PathService::Get(chrome::DIR_CHROMEOS_WALLPAPERS, &wallpaper_dir)) {
    LOG(ERROR) << "Unable to get wallpaper dir.";
    return base::FilePath();
  }
  return wallpaper_dir.Append(kDeviceWallpaperDir);
}

base::FilePath WallpaperManager::GetDeviceWallpaperFilePath() {
  return GetDeviceWallpaperDir().Append(kDeviceWallpaperFile);
}

void WallpaperManager::OnWallpaperDecoded(
    const AccountId& account_id,
    const wallpaper::WallpaperInfo& info,
    bool update_wallpaper,
    MovableOnDestroyCallbackHolder on_finish,
    std::unique_ptr<user_manager::UserImage> user_image) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT_ASYNC_END0("ui", "LoadAndDecodeWallpaper", this);

  // If decoded wallpaper is empty, we have probably failed to decode the file.
  // Use default wallpaper in this case.
  if (user_image->image().isNull()) {
    // Updates user pref to default wallpaper.
    wallpaper::WallpaperInfo default_info(
        "", wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED, wallpaper::DEFAULT,
        base::Time::Now().LocalMidnight());
    SetUserWallpaperInfo(account_id, default_info, true);
    DoSetDefaultWallpaper(account_id, update_wallpaper, std::move(on_finish));
    return;
  }

  // Update the image, but keep the path which was set earlier.
  wallpaper_cache_[account_id].second = user_image->image();

  if (update_wallpaper)
    SetWallpaper(user_image->image(), info);
}

void WallpaperManager::SetUserWallpaper(const AccountId& account_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Some unit tests come here without a UserManager or without a pref system.
  if (!user_manager::UserManager::IsInitialized() ||
      !g_browser_process->local_state()) {
    return;
  }

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);

  // User is unknown or there is no visible wallpaper in kiosk mode.
  if (!user || user->GetType() == user_manager::USER_TYPE_KIOSK_APP ||
      user->GetType() == user_manager::USER_TYPE_ARC_KIOSK_APP) {
    return;
  }

  // For a enterprise managed user, set the device wallpaper if we're at the
  // login screen.
  if (!user_manager::UserManager::Get()->IsUserLoggedIn() &&
      SetDeviceWallpaperIfApplicable(account_id))
    return;

  // Guest user or regular user in ephemeral mode.
  if ((user_manager::UserManager::Get()->IsUserNonCryptohomeDataEphemeral(
           account_id) &&
       user->HasGaiaAccount()) ||
      user->GetType() == user_manager::USER_TYPE_GUEST) {
    InitInitialUserWallpaper(account_id, false);
    GetPendingWallpaper()->SetDefaultWallpaper(account_id);
    if (base::SysInfo::IsRunningOnChromeOS()) {
      LOG(ERROR)
          << "User is ephemeral or guest! Fallback to default wallpaper.";
    }
    return;
  }

  last_selected_user_ = account_id;

  WallpaperInfo info;

  if (!GetUserWallpaperInfo(account_id, &info)) {
    InitInitialUserWallpaper(account_id, true);
    GetUserWallpaperInfo(account_id, &info);
  }

  gfx::ImageSkia user_wallpaper;
  current_user_wallpaper_info_ = info;
  if (GetWallpaperFromCache(account_id, &user_wallpaper)) {
    GetPendingWallpaper()->SetWallpaperFromImage(account_id, user_wallpaper,
                                                 info);
  } else {
    if (info.location.empty()) {
      // Uses default built-in wallpaper when file is empty. Eventually, we
      // will only ship one built-in wallpaper in ChromeOS image.
      GetPendingWallpaper()->SetDefaultWallpaper(account_id);
      return;
    }

    if (info.type == wallpaper::CUSTOMIZED || info.type == wallpaper::POLICY ||
        info.type == wallpaper::DEVICE) {
      base::FilePath wallpaper_path;
      if (info.type != wallpaper::DEVICE) {
        const char* sub_dir = GetCustomWallpaperSubdirForCurrentResolution();
        // Wallpaper is not resized when layout is
        // wallpaper::WALLPAPER_LAYOUT_CENTER.
        // Original wallpaper should be used in this case.
        // TODO(bshe): Generates cropped custom wallpaper for CENTER layout.
        if (info.layout == wallpaper::WALLPAPER_LAYOUT_CENTER)
          sub_dir = kOriginalWallpaperSubDir;
        wallpaper_path = GetCustomWallpaperDir(sub_dir);
        wallpaper_path = wallpaper_path.Append(info.location);
      } else {
        wallpaper_path = GetDeviceWallpaperFilePath();
      }

      CustomWallpaperMap::iterator it = wallpaper_cache_.find(account_id);
      // Do not try to load the wallpaper if the path is the same. Since loading
      // could still be in progress, we ignore the existence of the image.
      if (it != wallpaper_cache_.end() && it->second.first == wallpaper_path)
        return;

      // Set the new path and reset the existing image - the image will be
      // added once it becomes available.
      wallpaper_cache_[account_id] =
          CustomWallpaperElement(wallpaper_path, gfx::ImageSkia());
      loaded_wallpapers_for_test_++;

      GetPendingWallpaper()->SetWallpaperFromPath(account_id, info,
                                                  wallpaper_path);
      return;
    }

    // Load downloaded online or converted default wallpapers according to the
    // WallpaperInfo.
    GetPendingWallpaper()->SetWallpaperFromInfo(account_id, info);
  }
}

void WallpaperManager::DoSetDefaultWallpaper(
    const AccountId& account_id,
    bool update_wallpaper,
    MovableOnDestroyCallbackHolder on_finish) {
  // There is no visible wallpaper in kiosk mode.
  if (user_manager::UserManager::Get()->IsLoggedInAsKioskApp())
    return;
  wallpaper_cache_.erase(account_id);

  WallpaperResolution resolution = GetAppropriateResolution();
  const bool use_small = (resolution == WALLPAPER_RESOLUTION_SMALL);

  const base::FilePath* file = NULL;

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);

  if (user_manager::UserManager::Get()->IsLoggedInAsGuest()) {
    file =
        use_small ? &guest_small_wallpaper_file_ : &guest_large_wallpaper_file_;
  } else if (user && user->GetType() == user_manager::USER_TYPE_CHILD) {
    file =
        use_small ? &child_small_wallpaper_file_ : &child_large_wallpaper_file_;
  } else {
    file = use_small ? &default_small_wallpaper_file_
                     : &default_large_wallpaper_file_;
  }
  wallpaper::WallpaperLayout layout =
      use_small ? wallpaper::WALLPAPER_LAYOUT_CENTER
                : wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED;
  DCHECK(file);
  if (!default_wallpaper_image_.get() ||
      default_wallpaper_image_->file_path() != *file) {
    default_wallpaper_image_.reset();
    if (!file->empty()) {
      loaded_wallpapers_for_test_++;
      StartLoadAndSetDefaultWallpaper(*file, layout, update_wallpaper,
                                      std::move(on_finish),
                                      &default_wallpaper_image_);
      return;
    }

    CreateSolidDefaultWallpaper();
  }

  if (update_wallpaper) {
    // 1x1 wallpaper is actually solid color, so it should be stretched.
    if (default_wallpaper_image_->image().width() == 1 &&
        default_wallpaper_image_->image().height() == 1) {
      layout = wallpaper::WALLPAPER_LAYOUT_STRETCH;
    }

    WallpaperInfo info(default_wallpaper_image_->file_path().value(), layout,
                       wallpaper::DEFAULT, base::Time::Now().LocalMidnight());
    SetWallpaper(default_wallpaper_image_->image(), info);
  }
}

void WallpaperManager::StartLoad(const AccountId& account_id,
                                 const WallpaperInfo& info,
                                 bool update_wallpaper,
                                 const base::FilePath& wallpaper_path,
                                 MovableOnDestroyCallbackHolder on_finish) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT_ASYNC_BEGIN0("ui", "LoadAndDecodeWallpaper", this);
  if (update_wallpaper) {
    // We are now about to change the wallpaper, so update the path and remove
    // the existing image.
    wallpaper_cache_[account_id] =
        CustomWallpaperElement(wallpaper_path, gfx::ImageSkia());
  }
  user_image_loader::StartWithFilePath(
      task_runner_, wallpaper_path, ImageDecoder::ROBUST_JPEG_CODEC,
      0,  // Do not crop.
      base::Bind(&WallpaperManager::OnWallpaperDecoded,
                 weak_factory_.GetWeakPtr(), account_id, info, update_wallpaper,
                 base::Passed(std::move(on_finish))));
}

void WallpaperManager::SaveLastLoadTime(const base::TimeDelta elapsed) {
  while (last_load_times_.size() >= kLastLoadsStatsMsMaxSize)
    last_load_times_.pop_front();

  if (elapsed > base::TimeDelta::FromMicroseconds(0)) {
    last_load_times_.push_back(elapsed);
    last_load_finished_at_ = base::Time::Now();
  }
}

void WallpaperManager::NotifyAnimationFinished() {
  for (auto& observer : observers_)
    observer.OnWallpaperAnimationFinished(last_selected_user_);
}

base::TimeDelta WallpaperManager::GetWallpaperLoadDelay() const {
  base::TimeDelta delay;

  if (last_load_times_.size() == 0) {
    delay = base::TimeDelta::FromMilliseconds(kLoadDefaultDelayMs);
  } else {
    delay = std::accumulate(last_load_times_.begin(), last_load_times_.end(),
                            base::TimeDelta(), std::plus<base::TimeDelta>()) /
            last_load_times_.size();

    if (delay < base::TimeDelta::FromMilliseconds(kLoadMinDelayMs))
      delay = base::TimeDelta::FromMilliseconds(kLoadMinDelayMs);
    else if (delay > base::TimeDelta::FromMilliseconds(kLoadMaxDelayMs))
      delay = base::TimeDelta::FromMilliseconds(kLoadMaxDelayMs);

    // If we had ever loaded wallpaper, adjust wait delay by time since last
    // load.
    if (!last_load_finished_at_.is_null()) {
      const base::TimeDelta interval =
          base::Time::Now() - last_load_finished_at_;
      if (interval > delay)
        delay = base::TimeDelta::FromMilliseconds(0);
      else if (interval > base::TimeDelta::FromMilliseconds(0))
        delay -= interval;
    }
  }
  return delay;
}

void WallpaperManager::SetCustomizedDefaultWallpaperAfterCheck(
    const GURL& wallpaper_url,
    const base::FilePath& downloaded_file,
    std::unique_ptr<CustomizedWallpaperRescaledFiles> rescaled_files) {
  PrefService* pref_service = g_browser_process->local_state();

  std::string current_url =
      pref_service->GetString(prefs::kCustomizationDefaultWallpaperURL);
  if (current_url != wallpaper_url.spec() || !rescaled_files->AllSizesExist()) {
    DCHECK(rescaled_files->downloaded_exists());

    // Either resized images do not exist or cached version is incorrect.
    // Need to start resize again.
    user_image_loader::StartWithFilePath(
        task_runner_, downloaded_file, ImageDecoder::ROBUST_JPEG_CODEC,
        0,  // Do not crop.
        base::Bind(&WallpaperManager::OnCustomizedDefaultWallpaperDecoded,
                   weak_factory_.GetWeakPtr(), wallpaper_url,
                   base::Passed(std::move(rescaled_files))));
  } else {
    SetDefaultWallpaperPath(rescaled_files->path_rescaled_small(),
                            std::unique_ptr<gfx::ImageSkia>(),
                            rescaled_files->path_rescaled_large(),
                            std::unique_ptr<gfx::ImageSkia>());
  }
}

void WallpaperManager::OnCustomizedDefaultWallpaperDecoded(
    const GURL& wallpaper_url,
    std::unique_ptr<CustomizedWallpaperRescaledFiles> rescaled_files,
    std::unique_ptr<user_manager::UserImage> wallpaper) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If decoded wallpaper is empty, we have probably failed to decode the file.
  if (wallpaper->image().isNull()) {
    LOG(WARNING) << "Failed to decode customized wallpaper.";
    return;
  }

  wallpaper->image().EnsureRepsForSupportedScales();
  // TODO(crbug.com/593251): DeepCopy() may be unnecessary as this function
  // owns |wallpaper| as scoped_ptr whereas it used to be a const reference.
  std::unique_ptr<gfx::ImageSkia> deep_copy(wallpaper->image().DeepCopy());

  std::unique_ptr<bool> success(new bool(false));
  std::unique_ptr<gfx::ImageSkia> small_wallpaper_image(new gfx::ImageSkia);
  std::unique_ptr<gfx::ImageSkia> large_wallpaper_image(new gfx::ImageSkia);

  base::Closure resize_closure = base::Bind(
      &WallpaperManager::ResizeCustomizedDefaultWallpaper,
      base::Passed(&deep_copy), base::Unretained(rescaled_files.get()),
      base::Unretained(success.get()),
      base::Unretained(small_wallpaper_image.get()),
      base::Unretained(large_wallpaper_image.get()));
  base::Closure on_resized_closure = base::Bind(
      &WallpaperManager::OnCustomizedDefaultWallpaperResized,
      weak_factory_.GetWeakPtr(), wallpaper_url,
      base::Passed(std::move(rescaled_files)), base::Passed(std::move(success)),
      base::Passed(std::move(small_wallpaper_image)),
      base::Passed(std::move(large_wallpaper_image)));

  if (!task_runner_->PostTaskAndReply(FROM_HERE, resize_closure,
                                      on_resized_closure)) {
    LOG(WARNING) << "Failed to start Customized Wallpaper resize.";
  }
}

void WallpaperManager::OnCustomizedDefaultWallpaperResized(
    const GURL& wallpaper_url,
    std::unique_ptr<CustomizedWallpaperRescaledFiles> rescaled_files,
    std::unique_ptr<bool> success,
    std::unique_ptr<gfx::ImageSkia> small_wallpaper_image,
    std::unique_ptr<gfx::ImageSkia> large_wallpaper_image) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(rescaled_files);
  DCHECK(success.get());
  if (!*success) {
    LOG(WARNING) << "Failed to save resized customized default wallpaper";
    return;
  }
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetString(prefs::kCustomizationDefaultWallpaperURL,
                          wallpaper_url.spec());
  SetDefaultWallpaperPath(
      rescaled_files->path_rescaled_small(), std::move(small_wallpaper_image),
      rescaled_files->path_rescaled_large(), std::move(large_wallpaper_image));
  VLOG(1) << "Customized default wallpaper applied.";
}

void WallpaperManager::SetDefaultWallpaperPathsFromCommandLine(
    base::CommandLine* command_line) {
  default_small_wallpaper_file_ = command_line->GetSwitchValuePath(
      chromeos::switches::kDefaultWallpaperSmall);
  default_large_wallpaper_file_ = command_line->GetSwitchValuePath(
      chromeos::switches::kDefaultWallpaperLarge);
  guest_small_wallpaper_file_ = command_line->GetSwitchValuePath(
      chromeos::switches::kGuestWallpaperSmall);
  guest_large_wallpaper_file_ = command_line->GetSwitchValuePath(
      chromeos::switches::kGuestWallpaperLarge);
  child_small_wallpaper_file_ = command_line->GetSwitchValuePath(
      chromeos::switches::kChildWallpaperSmall);
  child_large_wallpaper_file_ = command_line->GetSwitchValuePath(
      chromeos::switches::kChildWallpaperLarge);
  default_wallpaper_image_.reset();
}

void WallpaperManager::OnDefaultWallpaperDecoded(
    const base::FilePath& path,
    const wallpaper::WallpaperLayout layout,
    bool update_wallpaper,
    std::unique_ptr<user_manager::UserImage>* result_out,
    MovableOnDestroyCallbackHolder on_finish,
    std::unique_ptr<user_manager::UserImage> user_image) {
  if (user_image->image().isNull()) {
    LOG(ERROR) << "Failed to decode default wallpaper. ";
    return;
  }

  *result_out = std::move(user_image);
  if (update_wallpaper) {
    WallpaperInfo info(path.value(), layout, wallpaper::DEFAULT,
                       base::Time::Now().LocalMidnight());
    SetWallpaper((*result_out)->image(), info);
  }
}

void WallpaperManager::StartLoadAndSetDefaultWallpaper(
    const base::FilePath& path,
    const wallpaper::WallpaperLayout layout,
    bool update_wallpaper,
    MovableOnDestroyCallbackHolder on_finish,
    std::unique_ptr<user_manager::UserImage>* result_out) {
  user_image_loader::StartWithFilePath(
      task_runner_, path, ImageDecoder::ROBUST_JPEG_CODEC,
      0,  // Do not crop.
      base::Bind(&WallpaperManager::OnDefaultWallpaperDecoded,
                 weak_factory_.GetWeakPtr(), path, layout, update_wallpaper,
                 base::Unretained(result_out),
                 base::Passed(std::move(on_finish))));
}

void WallpaperManager::RecordWallpaperAppType() {
  user_manager::User* user = user_manager::UserManager::Get()->GetActiveUser();
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);

  UMA_HISTOGRAM_ENUMERATION(
      "Ash.Wallpaper.Apps",
      wallpaper_manager_util::ShouldUseAndroidWallpapersApp(profile)
          ? WALLPAPERS_APP_ANDROID
          : WALLPAPERS_PICKER_APP_CHROMEOS,
      WALLPAPERS_APPS_NUM);
}

bool WallpaperManager::CanGetWallpaperFilesId() const {
  return SystemSaltGetter::IsInitialized() &&
         SystemSaltGetter::Get()->GetRawSalt();
}

const char* WallpaperManager::GetCustomWallpaperSubdirForCurrentResolution() {
  WallpaperResolution resolution = GetAppropriateResolution();
  return resolution == WALLPAPER_RESOLUTION_SMALL ? kSmallWallpaperSubDir
                                                  : kLargeWallpaperSubDir;
}

void WallpaperManager::CreateSolidDefaultWallpaper() {
  loaded_wallpapers_for_test_++;
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(kDefaultWallpaperColor);
  const gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  default_wallpaper_image_.reset(new user_manager::UserImage(image));
}

void WallpaperManager::OnCustomWallpaperFileNotFound(
    const AccountId& account_id,
    const base::FilePath& expected_path,
    bool update_wallpaper,
    MovableOnDestroyCallbackHolder on_finish) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::User* user = user_manager->FindUser(account_id);
  LOG(ERROR) << "Failed to load previously selected custom wallpaper. "
             << "Fallback to default wallpaper. Expected wallpaper path: "
             << expected_path.value() << ". Number of users on the device: "
             << user_manager->GetUsers().size()
             << ", Number of logged in users on the device: "
             << user_manager->GetLoggedInUsers().size()
             << ". Current user type: " << user->GetType()
             << ", IsActiveUser=" << (user_manager->GetActiveUser() == user)
             << ", IsPrimaryUser=" << (user_manager->GetPrimaryUser() == user)
             << ".";

  DoSetDefaultWallpaper(account_id, update_wallpaper, std::move(on_finish));
}

WallpaperManager::PendingWallpaper* WallpaperManager::GetPendingWallpaper() {
  // If |pending_inactive_| already exists, return it directly. This allows the
  // pending request (whose timer is still running) to be overriden by a
  // subsequent request.
  if (!pending_inactive_) {
    loading_.push_back(
        new WallpaperManager::PendingWallpaper(GetWallpaperLoadDelay()));
    pending_inactive_ = loading_.back().get();
  }
  return pending_inactive_;
}

void WallpaperManager::RemovePendingWallpaperFromList(
    PendingWallpaper* pending) {
  DCHECK(loading_.size() > 0);
  for (WallpaperManager::PendingList::iterator i = loading_.begin();
       i != loading_.end(); ++i) {
    if (i->get() == pending) {
      loading_.erase(i);
      break;
    }
  }

  if (loading_.empty()) {
    for (auto& observer : observers_)
      observer.OnPendingListEmptyForTesting();
  }
}

void WallpaperManager::SetPolicyControlledWallpaper(
    const AccountId& account_id,
    std::unique_ptr<user_manager::UserImage> user_image) {
  if (!CanGetWallpaperFilesId()) {
    CallWhenCanGetFilesId(
        base::Bind(&WallpaperManager::SetPolicyControlledWallpaper,
                   weak_factory_.GetWeakPtr(), account_id,
                   base::Passed(std::move(user_image))));
    return;
  }

  const wallpaper::WallpaperFilesId wallpaper_files_id = GetFilesId(account_id);

  if (!wallpaper_files_id.is_valid())
    LOG(FATAL) << "Wallpaper flies id if invalid!";

  // If we're at the login screen, do not change the wallpaper to the user
  // policy controlled wallpaper but only update the cache. It will be later
  // updated after the user logs in.
  SetCustomWallpaper(account_id, wallpaper_files_id, "policy-controlled.jpeg",
                     wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED,
                     wallpaper::POLICY, user_image->image(),
                     user_manager::UserManager::Get()
                         ->IsUserLoggedIn() /* update wallpaper */);
}

void WallpaperManager::OnDeviceWallpaperPolicyChanged() {
  SetDeviceWallpaperIfApplicable(
      user_manager::UserManager::Get()->IsUserLoggedIn()
          ? user_manager::UserManager::Get()->GetActiveUser()->GetAccountId()
          : user_manager::SignInAccountId());
}

void WallpaperManager::OnDeviceWallpaperExists(const AccountId& account_id,
                                               const std::string& url,
                                               const std::string& hash,
                                               bool exist) {
  if (exist) {
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::Bind(&CheckDeviceWallpaperMatchHash, GetDeviceWallpaperFilePath(),
                   hash),
        base::Bind(&WallpaperManager::OnCheckDeviceWallpaperMatchHash,
                   weak_factory_.GetWeakPtr(), account_id, url, hash));
  } else {
    GURL device_wallpaper_url(url);
    device_wallpaper_downloader_.reset(new CustomizationWallpaperDownloader(
        g_browser_process->system_request_context(), device_wallpaper_url,
        GetDeviceWallpaperDir(), GetDeviceWallpaperFilePath(),
        base::Bind(&WallpaperManager::OnDeviceWallpaperDownloaded,
                   weak_factory_.GetWeakPtr(), account_id, hash)));
    device_wallpaper_downloader_->Start();
  }
}

void WallpaperManager::OnDeviceWallpaperDownloaded(const AccountId& account_id,
                                                   const std::string& hash,
                                                   bool success,
                                                   const GURL& url) {
  if (!success) {
    LOG(ERROR) << "Failed to download the device wallpaper. Fallback to "
                  "default wallpaper.";
    GetPendingWallpaper()->SetDefaultWallpaper(account_id);
    return;
  }

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::Bind(&CheckDeviceWallpaperMatchHash, GetDeviceWallpaperFilePath(),
                 hash),
      base::Bind(&WallpaperManager::OnCheckDeviceWallpaperMatchHash,
                 weak_factory_.GetWeakPtr(), account_id, url.spec(), hash));
}

void WallpaperManager::OnCheckDeviceWallpaperMatchHash(
    const AccountId& account_id,
    const std::string& url,
    const std::string& hash,
    bool match) {
  if (!match) {
    if (retry_download_if_failed_) {
      // We only retry to download the device wallpaper one more time if the
      // hash doesn't match.
      retry_download_if_failed_ = false;
      GURL device_wallpaper_url(url);
      device_wallpaper_downloader_.reset(new CustomizationWallpaperDownloader(
          g_browser_process->system_request_context(), device_wallpaper_url,
          GetDeviceWallpaperDir(), GetDeviceWallpaperFilePath(),
          base::Bind(&WallpaperManager::OnDeviceWallpaperDownloaded,
                     weak_factory_.GetWeakPtr(), account_id, hash)));
      device_wallpaper_downloader_->Start();
    } else {
      LOG(ERROR) << "The device wallpaper hash doesn't match with provided "
                    "hash value. Fallback to default wallpaper! ";
      GetPendingWallpaper()->SetDefaultWallpaper(account_id);

      // Reset the boolean variable so that it can retry to download when the
      // next device wallpaper request comes in.
      retry_download_if_failed_ = true;
    }
    return;
  }

  user_image_loader::StartWithFilePath(
      task_runner_, GetDeviceWallpaperFilePath(),
      ImageDecoder::ROBUST_JPEG_CODEC,
      0,  // Do not crop.
      base::Bind(&WallpaperManager::OnDeviceWallpaperDecoded,
                 weak_factory_.GetWeakPtr(), account_id));
}

void WallpaperManager::OnDeviceWallpaperDecoded(
    const AccountId& account_id,
    std::unique_ptr<user_manager::UserImage> user_image) {
  // It might be possible that the device policy controlled wallpaper finishes
  // decoding after the user logs in. In this case do nothing.
  if (!user_manager::UserManager::Get()->IsUserLoggedIn()) {
    WallpaperInfo wallpaper_info = {GetDeviceWallpaperFilePath().value(),
                                    wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED,
                                    wallpaper::DEVICE,
                                    base::Time::Now().LocalMidnight()};
    GetPendingWallpaper()->SetWallpaperFromImage(
        account_id, user_image->image(), wallpaper_info);
  }
}

void WallpaperManager::SetDefaultWallpaperPath(
    const base::FilePath& default_small_wallpaper_file,
    std::unique_ptr<gfx::ImageSkia> small_wallpaper_image,
    const base::FilePath& default_large_wallpaper_file,
    std::unique_ptr<gfx::ImageSkia> large_wallpaper_image) {
  default_small_wallpaper_file_ = default_small_wallpaper_file;
  default_large_wallpaper_file_ = default_large_wallpaper_file;

  ash::WallpaperController* controller =
      ash::Shell::Get()->wallpaper_controller();

  // |need_update_screen| is true if the previous default wallpaper is visible
  // now, so we need to update wallpaper on the screen.
  //
  // Layout is ignored here, so wallpaper::WALLPAPER_LAYOUT_CENTER is used
  // as a placeholder only.
  const bool need_update_screen =
      default_wallpaper_image_.get() &&
      controller->WallpaperIsAlreadyLoaded(default_wallpaper_image_->image(),
                                           false /* compare_layouts */,
                                           wallpaper::WALLPAPER_LAYOUT_CENTER);

  default_wallpaper_image_.reset();
  if (GetAppropriateResolution() == WALLPAPER_RESOLUTION_SMALL) {
    if (small_wallpaper_image) {
      default_wallpaper_image_.reset(
          new user_manager::UserImage(*small_wallpaper_image));
      default_wallpaper_image_->set_file_path(default_small_wallpaper_file);
    }
  } else {
    if (large_wallpaper_image) {
      default_wallpaper_image_.reset(
          new user_manager::UserImage(*large_wallpaper_image));
      default_wallpaper_image_->set_file_path(default_large_wallpaper_file);
    }
  }

  DoSetDefaultWallpaper(EmptyAccountId(), need_update_screen,
                        MovableOnDestroyCallbackHolder());
}

}  // namespace chromeos
