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
#include "ash/wallpaper/wallpaper_decoder.h"
#include "ash/wallpaper/wallpaper_window_state_manager.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
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
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/account_id/account_id.h"
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

// Call |closure| when HashWallpaperFilesIdStr will not assert().
void CallWhenCanGetFilesId(const base::Closure& closure) {
  SystemSaltGetter::Get()->AddOnSystemSaltReady(closure);
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

// Creates all new custom wallpaper directories for |wallpaper_files_id| if not
// exist.
void EnsureCustomWallpaperDirectories(
    const wallpaper::WallpaperFilesId& wallpaper_files_id) {
  base::FilePath dir;
  dir = WallpaperManager::GetCustomWallpaperDir(
      ash::WallpaperController::kSmallWallpaperSubDir);
  dir = dir.Append(wallpaper_files_id.id());
  if (!base::PathExists(dir))
    base::CreateDirectory(dir);
  dir = WallpaperManager::GetCustomWallpaperDir(
      ash::WallpaperController::kLargeWallpaperSubDir);
  dir = dir.Append(wallpaper_files_id.id());
  if (!base::PathExists(dir))
    base::CreateDirectory(dir);
  dir = WallpaperManager::GetCustomWallpaperDir(
      ash::WallpaperController::kOriginalWallpaperSubDir);
  dir = dir.Append(wallpaper_files_id.id());
  if (!base::PathExists(dir))
    base::CreateDirectory(dir);
  dir = WallpaperManager::GetCustomWallpaperDir(
      ash::WallpaperController::kThumbnailWallpaperSubDir);
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

// This is "wallpaper either scheduled to load, or loading right now".
//
// While enqueued, it defines moment in the future, when it will be loaded.
// Enqueued but not started request might be updated by subsequent load
// request. Therefore it's created empty, and updated being enqueued.
//
// PendingWallpaper is owned by WallpaperManager.
class WallpaperManager::PendingWallpaper {
 public:
  explicit PendingWallpaper(const base::TimeDelta delay) : weak_factory_(this) {
    timer.Start(FROM_HERE, delay,
                base::Bind(&WallpaperManager::PendingWallpaper::ProcessRequest,
                           weak_factory_.GetWeakPtr()));
  }

  ~PendingWallpaper() { weak_factory_.InvalidateWeakPtrs(); }

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
    // The only known case for this check to fail is global destruction during
    // wallpaper load. It should never happen.
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI))
      return;

    // Erase reference to self.
    timer.Stop();

    WallpaperManager* manager = WallpaperManager::Get();
    if (manager->pending_inactive_ == this)
      manager->pending_inactive_ = NULL;

    // This is "on destroy" callback that will call OnWallpaperSet() when
    // image is loaded.
    MovableOnDestroyCallbackHolder on_finish =
        std::make_unique<ash::WallpaperController::MovableOnDestroyCallback>(
            base::Bind(&WallpaperManager::PendingWallpaper::OnWallpaperSet,
                       weak_factory_.GetWeakPtr()));

    started_load_at_ = base::Time::Now();

    if (default_) {
      // The most recent request is |SetDefaultWallpaper|.
      manager->SetDefaultWallpaperImpl(account_id_, true /* update_wallpaper */,
                                       std::move(on_finish));
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
                         base::Passed(std::move(on_finish)),
                         manager->weak_factory_.GetWeakPtr()));
    } else if (!info_.location.empty()) {
      // The most recent request is |SetWallpaperFromInfo|.
      manager->LoadWallpaper(account_id_, info_, true /* update_wallpaper */,
                             std::move(on_finish));
    } else {
      // PendingWallpaper was created but none of the four methods was called.
      // This should never happen. Do not record time in this case.
      NOTREACHED();
      started_load_at_ = base::Time();
    }
  }

  // This method is called by callback, when load request is finished.
  void OnWallpaperSet() {
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

  base::OneShotTimer timer;

  // Load start time to calculate duration.
  base::Time started_load_at_;

  base::WeakPtrFactory<PendingWallpaper> weak_factory_;

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
  (*wallpaper_manager_->GetWallpaperCacheMap())[account_id] =
      ash::CustomWallpaperElement(path, image);
}

void WallpaperManager::TestApi::ClearDisposableWallpaperCache() {
  wallpaper_manager_->ClearDisposableWallpaperCache();
}

// WallpaperManager, public: ---------------------------------------------------

WallpaperManager::~WallpaperManager() {
  show_user_name_on_signin_subscription_.reset();
  device_wallpaper_image_subscription_.reset();
  weak_factory_.InvalidateWeakPtrs();
  // In case there's wallpaper load request being processed.
  for (size_t i = 0; i < loading_.size(); ++i)
    delete loading_[i];
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
base::FilePath WallpaperManager::GetCustomWallpaperDir(
    const std::string& sub_dir) {
  if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash()) {
    // Some unit tests come here without a Shell instance.
    // TODO(crbug.com/776464): This is intended not to work under mash. Make it
    // work again after WallpaperManager is removed.
    return base::FilePath();
  }
  return ash::Shell::Get()->wallpaper_controller()->GetCustomWallpaperDir(
      sub_dir);
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
    const std::string& sub_dir,
    const wallpaper::WallpaperFilesId& wallpaper_files_id,
    const std::string& file_name) {
  base::FilePath custom_wallpaper_path = GetCustomWallpaperDir(sub_dir);
  return custom_wallpaper_path.Append(wallpaper_files_id.id())
      .Append(file_name);
}

void WallpaperManager::SetCustomWallpaper(
    const AccountId& account_id,
    const wallpaper::WallpaperFilesId& wallpaper_files_id,
    const std::string& file_name,
    wallpaper::WallpaperLayout layout,
    wallpaper::WallpaperType type,
    const gfx::ImageSkia& image,
    bool show_wallpaper) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // There is no visible wallpaper in kiosk mode.
  if (user_manager::UserManager::Get()->IsLoggedInAsKioskApp())
    return;

  // Don't allow custom wallpapers while policy is in effect.
  if (type != wallpaper::POLICY && IsPolicyControlled(account_id))
    return;

  // If decoded wallpaper is empty, we have probably failed to decode the file.
  // Use default wallpaper in this case.
  if (image.isNull()) {
    GetPendingWallpaper()->SetDefaultWallpaper(account_id);
    return;
  }

  base::FilePath wallpaper_path =
      GetCustomWallpaperPath(ash::WallpaperController::kOriginalWallpaperSubDir,
                             wallpaper_files_id, file_name);

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  CHECK(user);
  const bool is_persistent =
      !user_manager::UserManager::Get()->IsUserNonCryptohomeDataEphemeral(
          account_id) ||
      (type == wallpaper::POLICY &&
       user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT);

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
                                  wallpaper_files_id, wallpaper_path, layout,
                                  base::Passed(std::move(deep_copy))));
  }

  std::string relative_path =
      base::FilePath(wallpaper_files_id.id()).Append(file_name).value();
  // User's custom wallpaper path is determined by relative path and the
  // appropriate wallpaper resolution in GetCustomWallpaperInternal.
  WallpaperInfo info = {relative_path, layout, type,
                        base::Time::Now().LocalMidnight()};
  SetUserWallpaperInfo(account_id, info, is_persistent);
  if (show_wallpaper)
    GetPendingWallpaper()->SetWallpaperFromImage(account_id, image, info);

  (*GetWallpaperCacheMap())[account_id] =
      ash::CustomWallpaperElement(wallpaper_path, image);
}

void WallpaperManager::SetCustomizedDefaultWallpaper(
    const GURL& wallpaper_url,
    const base::FilePath& file_path,
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
  std::string downloaded_file_name = file_path.BaseName().value();
  std::unique_ptr<CustomizedWallpaperRescaledFiles> rescaled_files(
      new CustomizedWallpaperRescaledFiles(
          file_path,
          resized_directory.Append(downloaded_file_name +
                                   kSmallWallpaperSuffix),
          resized_directory.Append(downloaded_file_name +
                                   kLargeWallpaperSuffix)));

  base::Closure check_file_exists = rescaled_files->CreateCheckerClosure();
  base::Closure on_checked_closure =
      base::Bind(&WallpaperManager::SetCustomizedDefaultWallpaperAfterCheck,
                 weak_factory_.GetWeakPtr(), wallpaper_url, file_path,
                 base::Passed(std::move(rescaled_files)));
  if (!task_runner_->PostTaskAndReply(FROM_HERE, check_file_exists,
                                      on_checked_closure)) {
    LOG(WARNING) << "Failed to start check CheckCustomizedWallpaperFilesExist.";
  }
}

void WallpaperManager::ShowUserWallpaper(const AccountId& account_id) {
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
    InitializeUserWallpaperInfo(account_id);
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
    InitializeUserWallpaperInfo(account_id);
    GetUserWallpaperInfo(account_id, &info);
  }

  gfx::ImageSkia user_wallpaper;
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
          sub_dir = ash::WallpaperController::kOriginalWallpaperSubDir;
        wallpaper_path = GetCustomWallpaperDir(sub_dir);
        wallpaper_path = wallpaper_path.Append(info.location);
      } else {
        wallpaper_path = GetDeviceWallpaperFilePath();
      }

      ash::CustomWallpaperMap* wallpaper_cache_map = GetWallpaperCacheMap();
      ash::CustomWallpaperMap::iterator it =
          wallpaper_cache_map->find(account_id);
      // Do not try to load the wallpaper if the path is the same. Since loading
      // could still be in progress, we ignore the existence of the image.
      if (it != wallpaper_cache_map->end() &&
          it->second.first == wallpaper_path)
        return;

      // Set the new path and reset the existing image - the image will be
      // added once it becomes available.
      (*wallpaper_cache_map)[account_id] =
          ash::CustomWallpaperElement(wallpaper_path, gfx::ImageSkia());
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

void WallpaperManager::ShowSigninWallpaper() {
  if (SetDeviceWallpaperIfApplicable(user_manager::SignInAccountId()))
    return;
  WallpaperControllerClient::Get()->ShowSigninWallpaper();
}

void WallpaperManager::SetUserWallpaperInfo(const AccountId& account_id,
                                            const WallpaperInfo& info,
                                            bool is_persistent) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash()) {
    // Some unit tests come here without a Shell instance.
    // TODO(crbug.com/776464): This is intended not to work under mash. Make it
    // work again after WallpaperManager is removed.
    return;
  }
  ash::Shell::Get()->wallpaper_controller()->SetUserWallpaperInfo(
      account_id, info, is_persistent);
}

void WallpaperManager::InitializeWallpaper() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Apply device customization.
  if (ShouldUseCustomizedDefaultWallpaper()) {
    SetCustomizedDefaultWallpaperImpl(
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

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (!user_manager->IsUserLoggedIn()) {
    if (!StartupUtils::IsDeviceRegistered())
      ShowSigninWallpaper();
    else
      InitializeRegisteredDeviceWallpaper();
    return;
  }
  ShowUserWallpaper(user_manager->GetActiveUser()->GetAccountId());
}

void WallpaperManager::UpdateWallpaper(bool clear_cache) {
  for (auto& observer : observers_)
    observer.OnUpdateWallpaperForTesting();
  if (clear_cache)
    GetWallpaperCacheMap()->clear();

  // For GAIA login flow, the last_selected_user_ may not be set before user
  // login. If UpdateWallpaper is called at GAIA login screen, no wallpaper will
  // be set. It could result a black screen on external monitors.
  // See http://crbug.com/265689 for detail.
  if (last_selected_user_.empty())
    ShowSigninWallpaper();
  else
    ShowUserWallpaper(last_selected_user_);
}

bool WallpaperManager::IsPendingWallpaper(uint32_t image_id) {
  for (size_t i = 0; i < loading_.size(); ++i) {
    if (loading_[i]->GetImageId() == image_id) {
      return true;
    }
  }
  return false;
}

bool WallpaperManager::GetLoggedInUserWallpaperInfo(WallpaperInfo* info) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (user_manager::UserManager::Get()->IsLoggedInAsStub()) {
    info->location = "";
    info->layout = wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED;
    info->type = wallpaper::DEFAULT;
    info->date = base::Time::Now().LocalMidnight();
    *GetCachedWallpaperInfo() = *info;
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

void WallpaperManager::EnsureLoggedInUserWallpaperLoaded() {
  WallpaperInfo info;
  if (GetLoggedInUserWallpaperInfo(&info)) {
    UMA_HISTOGRAM_ENUMERATION("Ash.Wallpaper.Type", info.type,
                              wallpaper::WALLPAPER_TYPE_COUNT);
    RecordWallpaperAppType();
    if (info == *GetCachedWallpaperInfo())
      return;
  }
  ShowUserWallpaper(
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());
}

void WallpaperManager::OnPolicyFetched(const std::string& policy,
                                       const AccountId& account_id,
                                       std::unique_ptr<std::string> data) {
  if (!data)
    return;

  if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash()) {
    user_image_loader::StartWithData(
        task_runner_, std::move(data), ImageDecoder::ROBUST_JPEG_CODEC,
        0,  // Do not crop.
        base::Bind(&WallpaperManager::SetPolicyControlledWallpaper,
                   weak_factory_.GetWeakPtr(), account_id));
  } else {
    ash::WallpaperController::DecodeWallpaperIfApplicable(
        base::Bind(&WallpaperManager::SetPolicyControlledWallpaper,
                   weak_factory_.GetWeakPtr(), account_id),
        std::move(data), true /* data_is_ready */);
  }
}

size_t WallpaperManager::GetPendingListSizeForTesting() const {
  return loading_.size();
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

void WallpaperManager::AddObserver(WallpaperManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void WallpaperManager::RemoveObserver(WallpaperManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void WallpaperManager::OpenWallpaperPicker() {
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
    : activation_client_observer_(this),
      window_observer_(this),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  registrar_.Add(this, chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_WALLPAPER_ANIMATION_FINISHED,
                 content::NotificationService::AllSources());
  task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
}

// static
void WallpaperManager::SaveCustomWallpaper(
    const wallpaper::WallpaperFilesId& wallpaper_files_id,
    const base::FilePath& original_path,
    wallpaper::WallpaperLayout layout,
    std::unique_ptr<gfx::ImageSkia> image) {
  base::DeleteFile(
      GetCustomWallpaperDir(ash::WallpaperController::kOriginalWallpaperSubDir)
          .Append(wallpaper_files_id.id()),
      true /* recursive */);
  base::DeleteFile(
      GetCustomWallpaperDir(ash::WallpaperController::kSmallWallpaperSubDir)
          .Append(wallpaper_files_id.id()),
      true /* recursive */);
  base::DeleteFile(
      GetCustomWallpaperDir(ash::WallpaperController::kLargeWallpaperSubDir)
          .Append(wallpaper_files_id.id()),
      true /* recursive */);
  EnsureCustomWallpaperDirectories(wallpaper_files_id);
  std::string file_name = original_path.BaseName().value();
  base::FilePath small_wallpaper_path =
      GetCustomWallpaperPath(ash::WallpaperController::kSmallWallpaperSubDir,
                             wallpaper_files_id, file_name);
  base::FilePath large_wallpaper_path =
      GetCustomWallpaperPath(ash::WallpaperController::kLargeWallpaperSubDir,
                             wallpaper_files_id, file_name);

  // Re-encode orginal file to jpeg format and saves the result in case that
  // resized wallpaper is not generated (i.e. chrome shutdown before resized
  // wallpaper is saved).
  ResizeAndSaveWallpaper(*image, original_path,
                         wallpaper::WALLPAPER_LAYOUT_STRETCH, image->width(),
                         image->height(), nullptr);
  ResizeAndSaveWallpaper(*image, small_wallpaper_path, layout,
                         ash::WallpaperController::kSmallWallpaperMaxWidth,
                         ash::WallpaperController::kSmallWallpaperMaxHeight,
                         nullptr);
  ResizeAndSaveWallpaper(*image, large_wallpaper_path, layout,
                         ash::WallpaperController::kLargeWallpaperMaxWidth,
                         ash::WallpaperController::kLargeWallpaperMaxHeight,
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
  if (MoveCustomWallpaperDirectory(
          ash::WallpaperController::kOriginalWallpaperSubDir,
          temporary_wallpaper_dir, wallpaper_files_id.id())) {
    // Consider success if the original wallpaper is moved to the new directory.
    // Original wallpaper is the fallback if the correct resolution wallpaper
    // can not be found.
    reply_task_runner->PostTask(
        FROM_HERE, base::Bind(&WallpaperManager::MoveCustomWallpapersSuccess,
                              weak_ptr, account_id, wallpaper_files_id));
  }
  MoveCustomWallpaperDirectory(ash::WallpaperController::kLargeWallpaperSubDir,
                               temporary_wallpaper_dir,
                               wallpaper_files_id.id());
  MoveCustomWallpaperDirectory(ash::WallpaperController::kSmallWallpaperSubDir,
                               temporary_wallpaper_dir,
                               wallpaper_files_id.id());
  MoveCustomWallpaperDirectory(
      ash::WallpaperController::kThumbnailWallpaperSubDir,
      temporary_wallpaper_dir, wallpaper_files_id.id());
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
    valid_path = GetCustomWallpaperDir(
        ash::WallpaperController::kOriginalWallpaperSubDir);
    valid_path = valid_path.Append(info.location);
  }

  if (!base::PathExists(valid_path)) {
    // Falls back to custom wallpaper that uses AccountId as part of its file
    // path.
    // Note that account id is used instead of wallpaper_files_id here.
    const std::string& old_path = account_id.GetUserEmail();  // Migrated
    valid_path = GetCustomWallpaperPath(
        ash::WallpaperController::kOriginalWallpaperSubDir,
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

  *success &=
      ResizeAndSaveWallpaper(*image, rescaled_files->path_rescaled_small(),
                             wallpaper::WALLPAPER_LAYOUT_STRETCH,
                             ash::WallpaperController::kSmallWallpaperMaxWidth,
                             ash::WallpaperController::kSmallWallpaperMaxHeight,
                             small_wallpaper_image);

  *success &=
      ResizeAndSaveWallpaper(*image, rescaled_files->path_rescaled_large(),
                             wallpaper::WALLPAPER_LAYOUT_STRETCH,
                             ash::WallpaperController::kLargeWallpaperMaxWidth,
                             ash::WallpaperController::kLargeWallpaperMaxHeight,
                             large_wallpaper_image);
}

void WallpaperManager::InitializeUserWallpaperInfo(
    const AccountId& account_id) {
  if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash()) {
    // Some unit tests come here without a Shell instance.
    // TODO(crbug.com/776464): This is intended not to work under mash. Make it
    // work again after WallpaperManager is removed.
    return;
  }
  bool is_persistent =
      !user_manager::UserManager::Get()->IsUserNonCryptohomeDataEphemeral(
          account_id);
  ash::Shell::Get()->wallpaper_controller()->InitializeUserWallpaperInfo(
      account_id, is_persistent);
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

bool WallpaperManager::GetWallpaperFromCache(const AccountId& account_id,
                                             gfx::ImageSkia* image) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash()) {
    // Some unit tests come here without a Shell instance.
    // TODO(crbug.com/776464): This is intended not to work under mash. Make it
    // work again after WallpaperManager is removed.
    return false;
  }
  return ash::Shell::Get()->wallpaper_controller()->GetWallpaperFromCache(
      account_id, image);
}

bool WallpaperManager::GetPathFromCache(const AccountId& account_id,
                                        base::FilePath* path) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash()) {
    // Some unit tests come here without a Shell instance.
    // TODO(crbug.com/776464): This is intended not to work under mash. Make it
    // work again after WallpaperManager is removed.
    return false;
  }
  return ash::Shell::Get()->wallpaper_controller()->GetPathFromCache(account_id,
                                                                     path);
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
  ash::CustomWallpaperMap* wallpaper_cache_map = GetWallpaperCacheMap();
  ash::CustomWallpaperMap::iterator it = wallpaper_cache_map->find(account_id);
  if (it != wallpaper_cache_map->end() && !it->second.second.isNull())
    return;
  WallpaperInfo info;
  if (GetUserWallpaperInfo(account_id, &info)) {
    if (info.location.empty())
      return;

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
      (*wallpaper_cache_map)[account_id] =
          ash::CustomWallpaperElement(wallpaper_path, gfx::ImageSkia());
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

  ash::CustomWallpaperMap logged_in_users_cache;
  ash::CustomWallpaperMap* wallpaper_cache_map = GetWallpaperCacheMap();
  for (ash::CustomWallpaperMap::iterator it = wallpaper_cache_map->begin();
       it != wallpaper_cache_map->end(); ++it) {
    if (logged_in_user_account_ids.find(it->first) !=
        logged_in_user_account_ids.end()) {
      logged_in_users_cache.insert(*it);
    }
  }
  *wallpaper_cache_map = logged_in_users_cache;
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
    // Boot into the sign in screen.
    ShowSigninWallpaper();
    return;
  }

  if (!disable_boot_animation) {
    int index = public_session_user_index != -1 ? public_session_user_index : 0;
    // Normal boot, load user wallpaper.
    // If normal boot animation is disabled wallpaper would be set
    // asynchronously once user pods are loaded.
    ShowUserWallpaper(users[index]->GetAccountId());
  }
}

void WallpaperManager::LoadWallpaper(const AccountId& account_id,
                                     const WallpaperInfo& info,
                                     bool update_wallpaper,
                                     MovableOnDestroyCallbackHolder on_finish) {
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
    ash::WallpaperController::WallpaperResolution resolution =
        ash::WallpaperController::GetAppropriateResolution();
    // Only solid color wallpapers have stretch layout and they have only one
    // resolution.
    if (info.layout != wallpaper::WALLPAPER_LAYOUT_STRETCH &&
        resolution == ash::WallpaperController::WALLPAPER_RESOLUTION_SMALL) {
      file_name = base::FilePath(file_name)
                      .InsertBeforeExtension(kSmallWallpaperSuffix)
                      .value();
    }
    DCHECK(!ash::WallpaperController::dir_chrome_os_wallpapers_path_.empty());
    wallpaper_path =
        ash::WallpaperController::dir_chrome_os_wallpapers_path_.Append(
            file_name);

    // If the wallpaper exists and it contains already the correct image we can
    // return immediately.
    ash::CustomWallpaperMap* wallpaper_cache_map = GetWallpaperCacheMap();
    ash::CustomWallpaperMap::iterator it =
        wallpaper_cache_map->find(account_id);
    if (it != wallpaper_cache_map->end() &&
        it->second.first == wallpaper_path && !it->second.second.isNull())
      return;

    loaded_wallpapers_for_test_++;
    StartLoad(account_id, info, update_wallpaper, wallpaper_path,
              std::move(on_finish));
  } else if (info.type == wallpaper::DEFAULT) {
    // Default wallpapers are migrated from M21 user profiles. A code refactor
    // overlooked that case and caused these wallpapers not being loaded at all.
    // On some slow devices, it caused login webui not visible after upgrade to
    // M26 from M21. See crosbug.com/38429 for details.
    DCHECK(!ash::WallpaperController::dir_user_data_path_.empty());
    wallpaper_path =
        ash::WallpaperController::dir_user_data_path_.Append(info.location);
    StartLoad(account_id, info, update_wallpaper, wallpaper_path,
              std::move(on_finish));
  } else {
    // In unexpected cases, revert to default wallpaper to fail safely. See
    // crosbug.com/38429.
    LOG(ERROR) << "Wallpaper reverts to default unexpected.";
    SetDefaultWallpaperImpl(account_id, update_wallpaper, std::move(on_finish));
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
                              WallpaperControllerClient::Get()->GetFilesId(
                                  logged_in_user->GetAccountId()),
                              base::ThreadTaskRunnerHandle::Get(),
                              weak_factory_.GetWeakPtr()));
  }
}

bool WallpaperManager::GetUserWallpaperInfo(const AccountId& account_id,
                                            WallpaperInfo* info) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash()) {
    // Some unit tests come here without a Shell instance.
    // TODO(crbug.com/776464): This is intended not to work under mash. Make it
    // work again after WallpaperManager is removed.
    return false;
  }
  bool is_persistent =
      !user_manager::UserManager::Get()->IsUserNonCryptohomeDataEphemeral(
          account_id);
  return ash::Shell::Get()->wallpaper_controller()->GetUserWallpaperInfo(
      account_id, info, is_persistent);
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
  DCHECK(!ash::WallpaperController::dir_chrome_os_wallpapers_path_.empty());
  return ash::WallpaperController::dir_chrome_os_wallpapers_path_.Append(
      kDeviceWallpaperDir);
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
    SetDefaultWallpaperImpl(account_id, update_wallpaper, std::move(on_finish));
    return;
  }

  // Update the image, but keep the path which was set earlier.
  (*GetWallpaperCacheMap())[account_id].second = user_image->image();

  if (update_wallpaper)
    SetWallpaper(user_image->image(), info);
}

void WallpaperManager::SetDefaultWallpaperImpl(
    const AccountId& account_id,
    bool show_wallpaper,
    MovableOnDestroyCallbackHolder on_finish) {
  if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash()) {
    // Some unit tests come here without a Shell instance.
    // TODO(crbug.com/776464): This is intended not to work under mash. Make it
    // work again after WallpaperManager is removed.
    WallpaperInfo info("", wallpaper::WALLPAPER_LAYOUT_STRETCH,
                       wallpaper::DEFAULT, base::Time::Now().LocalMidnight());
    SetWallpaper(ash::WallpaperController::CreateSolidColorWallpaper(), info);
    return;
  }
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  if (!user) {
    LOG(ERROR) << "User of " << account_id
               << " cannot be found. This should never happen"
               << " within WallpaperManager::SetDefaultWallpaperImpl.";
    return;
  }
  ash::Shell::Get()->wallpaper_controller()->SetDefaultWallpaperImpl(
      account_id, user->GetType(), show_wallpaper, std::move(on_finish));
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
    (*GetWallpaperCacheMap())[account_id] =
        ash::CustomWallpaperElement(wallpaper_path, gfx::ImageSkia());
  }

  if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash()) {
    user_image_loader::StartWithFilePath(
        task_runner_, wallpaper_path, ImageDecoder::ROBUST_JPEG_CODEC,
        0,  // Do not crop.
        base::Bind(&WallpaperManager::OnWallpaperDecoded,
                   weak_factory_.GetWeakPtr(), account_id, info,
                   update_wallpaper, base::Passed(std::move(on_finish))));
  } else {
    ash::WallpaperController::ReadAndDecodeWallpaper(
        base::Bind(&WallpaperManager::OnWallpaperDecoded,
                   weak_factory_.GetWeakPtr(), account_id, info,
                   update_wallpaper, base::Passed(std::move(on_finish))),
        task_runner_, wallpaper_path);
  }
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
    // Calculate the average loading time.
    delay = std::accumulate(last_load_times_.begin(), last_load_times_.end(),
                            base::TimeDelta(), std::plus<base::TimeDelta>()) /
            last_load_times_.size();

    if (delay < base::TimeDelta::FromMilliseconds(kLoadMinDelayMs))
      delay = base::TimeDelta::FromMilliseconds(kLoadMinDelayMs);
    else if (delay > base::TimeDelta::FromMilliseconds(kLoadMaxDelayMs))
      delay = base::TimeDelta::FromMilliseconds(kLoadMaxDelayMs);

    // Reduce the delay by the time passed after the last wallpaper load.
    DCHECK(!last_load_finished_at_.is_null());
    const base::TimeDelta interval = base::Time::Now() - last_load_finished_at_;
    if (interval > delay)
      delay = base::TimeDelta::FromMilliseconds(0);
    else
      delay -= interval;
  }
  return delay;
}

void WallpaperManager::SetCustomizedDefaultWallpaperAfterCheck(
    const GURL& wallpaper_url,
    const base::FilePath& file_path,
    std::unique_ptr<CustomizedWallpaperRescaledFiles> rescaled_files) {
  PrefService* pref_service = g_browser_process->local_state();

  std::string current_url =
      pref_service->GetString(prefs::kCustomizationDefaultWallpaperURL);
  if (current_url != wallpaper_url.spec() || !rescaled_files->AllSizesExist()) {
    DCHECK(rescaled_files->downloaded_exists());

    // Either resized images do not exist or cached version is incorrect.
    // Need to start decoding again.
    if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash()) {
      user_image_loader::StartWithFilePath(
          task_runner_, file_path, ImageDecoder::ROBUST_JPEG_CODEC,
          0,  // Do not crop.
          base::Bind(&WallpaperManager::OnCustomizedDefaultWallpaperDecoded,
                     weak_factory_.GetWeakPtr(), wallpaper_url,
                     base::Passed(std::move(rescaled_files))));
    } else {
      ash::WallpaperController::ReadAndDecodeWallpaper(
          base::Bind(&WallpaperManager::OnCustomizedDefaultWallpaperDecoded,
                     weak_factory_.GetWeakPtr(), wallpaper_url,
                     base::Passed(std::move(rescaled_files))),
          task_runner_, file_path);
    }
  } else {
    SetCustomizedDefaultWallpaperImpl(rescaled_files->path_rescaled_small(),
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
  SetCustomizedDefaultWallpaperImpl(
      rescaled_files->path_rescaled_small(), std::move(small_wallpaper_image),
      rescaled_files->path_rescaled_large(), std::move(large_wallpaper_image));
  VLOG(1) << "Customized default wallpaper applied.";
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

const char* WallpaperManager::GetCustomWallpaperSubdirForCurrentResolution() {
  ash::WallpaperController::WallpaperResolution resolution =
      ash::WallpaperController::GetAppropriateResolution();
  return resolution == ash::WallpaperController::WALLPAPER_RESOLUTION_SMALL
             ? ash::WallpaperController::kSmallWallpaperSubDir
             : ash::WallpaperController::kLargeWallpaperSubDir;
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

  SetDefaultWallpaperImpl(account_id, update_wallpaper, std::move(on_finish));
}

WallpaperManager::PendingWallpaper* WallpaperManager::GetPendingWallpaper() {
  // If |pending_inactive_| already exists, return it directly. This allows the
  // pending request (whose timer is still running) to be overriden by a
  // subsequent request.
  if (!pending_inactive_) {
    loading_.push_back(
        new WallpaperManager::PendingWallpaper(GetWallpaperLoadDelay()));
    pending_inactive_ = loading_.back();
  }
  return pending_inactive_;
}

void WallpaperManager::RemovePendingWallpaperFromList(
    PendingWallpaper* finished_loading_request) {
  DCHECK(loading_.size() > 0);
  for (size_t i = 0; i < loading_.size(); ++i) {
    if (loading_[i] == finished_loading_request) {
      delete loading_[i];
      loading_.erase(loading_.begin() + i);
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
  if (!WallpaperControllerClient::Get()->CanGetWallpaperFilesId()) {
    CallWhenCanGetFilesId(
        base::Bind(&WallpaperManager::SetPolicyControlledWallpaper,
                   weak_factory_.GetWeakPtr(), account_id,
                   base::Passed(std::move(user_image))));
    return;
  }

  const wallpaper::WallpaperFilesId wallpaper_files_id =
      WallpaperControllerClient::Get()->GetFilesId(account_id);

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

  const base::FilePath file_path = GetDeviceWallpaperFilePath();
  if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash()) {
    user_image_loader::StartWithFilePath(
        task_runner_, GetDeviceWallpaperFilePath(),
        ImageDecoder::ROBUST_JPEG_CODEC,
        0,  // Do not crop.
        base::Bind(&WallpaperManager::OnDeviceWallpaperDecoded,
                   weak_factory_.GetWeakPtr(), account_id));
  } else {
    ash::WallpaperController::ReadAndDecodeWallpaper(
        base::Bind(&WallpaperManager::OnDeviceWallpaperDecoded,
                   weak_factory_.GetWeakPtr(), account_id),
        task_runner_, file_path);
  }
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

void WallpaperManager::SetCustomizedDefaultWallpaperImpl(
    const base::FilePath& default_small_wallpaper_file,
    std::unique_ptr<gfx::ImageSkia> small_wallpaper_image,
    const base::FilePath& default_large_wallpaper_file,
    std::unique_ptr<gfx::ImageSkia> large_wallpaper_image) {
  if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash())
    return;
  ash::Shell::Get()->wallpaper_controller()->SetCustomizedDefaultWallpaperImpl(
      default_small_wallpaper_file, std::move(small_wallpaper_image),
      default_large_wallpaper_file, std::move(large_wallpaper_image));
}

wallpaper::WallpaperInfo* WallpaperManager::GetCachedWallpaperInfo() {
  if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash())
    return &dummy_current_user_wallpaper_info_;
  return ash::Shell::Get()
      ->wallpaper_controller()
      ->GetCurrentUserWallpaperInfo();
}

ash::CustomWallpaperMap* WallpaperManager::GetWallpaperCacheMap() {
  if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash())
    return &dummy_wallpaper_cache_map_;
  return ash::Shell::Get()->wallpaper_controller()->GetWallpaperCacheMap();
}

}  // namespace chromeos
