// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallpaper/wallpaper_manager_base.h"

#include <numeric>
#include <vector>
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
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
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/login/user_names.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "components/wallpaper/wallpaper_layout.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skia_util.h"

using content::BrowserThread;

namespace wallpaper {

namespace {

// Default quality for encoding wallpaper.
const int kDefaultEncodingQuality = 90;

// Maximum number of wallpapers cached by CacheUsersWallpapers().
const int kMaxWallpapersToCache = 3;

// Maximum number of entries in WallpaperManagerBase::last_load_times_ .
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

bool MoveCustomWallpaperDirectory(const char* sub_dir,
                                  const std::string& user_id,
                                  const std::string& user_id_hash) {
  base::FilePath base_path =
      WallpaperManagerBase::GetCustomWallpaperDir(sub_dir);
  base::FilePath to_path = base_path.Append(user_id_hash);
  base::FilePath from_path = base_path.Append(user_id);
  if (base::PathExists(from_path))
    return base::Move(from_path, to_path);
  return false;
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
  dir = WallpaperManagerBase::GetCustomWallpaperDir(kSmallWallpaperSubDir);
  dir = dir.Append(user_id_hash);
  if (!base::PathExists(dir))
    base::CreateDirectory(dir);
  dir = WallpaperManagerBase::GetCustomWallpaperDir(kLargeWallpaperSubDir);
  dir = dir.Append(user_id_hash);
  if (!base::PathExists(dir))
    base::CreateDirectory(dir);
  dir = WallpaperManagerBase::GetCustomWallpaperDir(kOriginalWallpaperSubDir);
  dir = dir.Append(user_id_hash);
  if (!base::PathExists(dir))
    base::CreateDirectory(dir);
  dir = WallpaperManagerBase::GetCustomWallpaperDir(kThumbnailWallpaperSubDir);
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

}  // namespace

MovableOnDestroyCallback::MovableOnDestroyCallback(
    const base::Closure& callback)
    : callback_(callback) {
}

MovableOnDestroyCallback::~MovableOnDestroyCallback() {
  if (!callback_.is_null())
    callback_.Run();
}

WallpaperInfo::WallpaperInfo()
    : layout(WALLPAPER_LAYOUT_CENTER),
      type(user_manager::User::WALLPAPER_TYPE_COUNT) {
}

WallpaperInfo::WallpaperInfo(const std::string& in_location,
                             WallpaperLayout in_layout,
                             user_manager::User::WallpaperType in_type,
                             const base::Time& in_date)
    : location(in_location),
      layout(in_layout),
      type(in_type),
      date(in_date) {
}

WallpaperInfo::~WallpaperInfo() {
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

const char kUserWallpapers[] = "UserWallpapers";
const char kUserWallpapersProperties[] = "UserWallpapersProperties";

const base::FilePath&
WallpaperManagerBase::CustomizedWallpaperRescaledFiles::path_downloaded()
    const {
  return path_downloaded_;
}

const base::FilePath&
WallpaperManagerBase::CustomizedWallpaperRescaledFiles::path_rescaled_small()
    const {
  return path_rescaled_small_;
}

const base::FilePath&
WallpaperManagerBase::CustomizedWallpaperRescaledFiles::path_rescaled_large()
    const {
  return path_rescaled_large_;
}

bool WallpaperManagerBase::CustomizedWallpaperRescaledFiles::downloaded_exists()
    const {
  return downloaded_exists_;
}

bool WallpaperManagerBase::CustomizedWallpaperRescaledFiles::
    rescaled_small_exists() const {
  return rescaled_small_exists_;
}

bool WallpaperManagerBase::CustomizedWallpaperRescaledFiles::
    rescaled_large_exists() const {
  return rescaled_large_exists_;
}

WallpaperManagerBase::CustomizedWallpaperRescaledFiles::
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
WallpaperManagerBase::CustomizedWallpaperRescaledFiles::CreateCheckerClosure() {
  return base::Bind(&WallpaperManagerBase::CustomizedWallpaperRescaledFiles::
                        CheckCustomizedWallpaperFilesExist,
                    base::Unretained(this));
}

void WallpaperManagerBase::CustomizedWallpaperRescaledFiles::
    CheckCustomizedWallpaperFilesExist() {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  downloaded_exists_ = base::PathExists(path_downloaded_);
  rescaled_small_exists_ = base::PathExists(path_rescaled_small_);
  rescaled_large_exists_ = base::PathExists(path_rescaled_large_);
}

bool WallpaperManagerBase::CustomizedWallpaperRescaledFiles::AllSizesExist()
    const {
  return rescaled_small_exists_ && rescaled_large_exists_;
}

// WallpaperManagerBase, public:

// TestApi. For testing purpose
WallpaperManagerBase::TestApi::TestApi(WallpaperManagerBase* wallpaper_manager)
    : wallpaper_manager_(wallpaper_manager) {
}

WallpaperManagerBase::TestApi::~TestApi() {
}

bool WallpaperManagerBase::TestApi::GetWallpaperFromCache(
    const std::string& user_id,
    gfx::ImageSkia* image) {
  return wallpaper_manager_->GetWallpaperFromCache(user_id, image);
}

bool WallpaperManagerBase::TestApi::GetPathFromCache(
    const std::string& user_id,
    base::FilePath* path) {
  return wallpaper_manager_->GetPathFromCache(user_id, path);
}

void WallpaperManagerBase::TestApi::SetWallpaperCache(
    const std::string& user_id,
    const base::FilePath& path,
    const gfx::ImageSkia& image) {
  DCHECK(!image.isNull());
  wallpaper_manager_->wallpaper_cache_[user_id] =
      CustomWallpaperElement(path, image);
}

void WallpaperManagerBase::TestApi::ClearDisposableWallpaperCache() {
  wallpaper_manager_->ClearDisposableWallpaperCache();
}

// static
void WallpaperManagerBase::SetPathIds(
    int dir_user_data_enum,
    int dir_chromeos_wallpapers_enum,
    int dir_chromeos_custom_wallpapers_enum) {
  dir_user_data_path_id = dir_user_data_enum;
  dir_chromeos_wallpapers_path_id = dir_chromeos_wallpapers_enum;
  dir_chromeos_custom_wallpapers_path_id = dir_chromeos_custom_wallpapers_enum;
}

// static
base::FilePath WallpaperManagerBase::GetCustomWallpaperDir(
    const char* sub_dir) {
  base::FilePath custom_wallpaper_dir;
  DCHECK(dir_chromeos_custom_wallpapers_path_id != -1);
  CHECK(PathService::Get(dir_chromeos_custom_wallpapers_path_id,
                         &custom_wallpaper_dir));
  return custom_wallpaper_dir.Append(sub_dir);
}

// static
void WallpaperManagerBase::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kUsersWallpaperInfo);
  registry->RegisterDictionaryPref(kUserWallpapers);
  registry->RegisterDictionaryPref(kUserWallpapersProperties);
}

void WallpaperManagerBase::EnsureLoggedInUserWallpaperLoaded() {
  WallpaperInfo info;
  if (GetLoggedInUserWallpaperInfo(&info)) {
    UMA_HISTOGRAM_ENUMERATION("Ash.Wallpaper.Type", info.type,
                              user_manager::User::WALLPAPER_TYPE_COUNT);
    if (info == current_user_wallpaper_info_)
      return;
  }
  SetUserWallpaperNow(
      user_manager::UserManager::Get()->GetLoggedInUser()->email());
}

void WallpaperManagerBase::ClearDisposableWallpaperCache() {
  // Cancel callback for previous cache requests.
  weak_factory_.InvalidateWeakPtrs();
  // Keep the wallpaper of logged in users in cache at multi-profile mode.
  std::set<std::string> logged_in_users_names;
  const user_manager::UserList& logged_users =
      user_manager::UserManager::Get()->GetLoggedInUsers();
  for (user_manager::UserList::const_iterator it = logged_users.begin();
       it != logged_users.end(); ++it) {
    logged_in_users_names.insert((*it)->email());
  }

  CustomWallpaperMap logged_in_users_cache;
  for (CustomWallpaperMap::iterator it = wallpaper_cache_.begin();
       it != wallpaper_cache_.end(); ++it) {
    if (logged_in_users_names.find(it->first) != logged_in_users_names.end()) {
      logged_in_users_cache.insert(*it);
    }
  }
  wallpaper_cache_ = logged_in_users_cache;
}

bool WallpaperManagerBase::GetLoggedInUserWallpaperInfo(WallpaperInfo* info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (user_manager::UserManager::Get()->IsLoggedInAsStub()) {
    info->location = current_user_wallpaper_info_.location = "";
    info->layout = current_user_wallpaper_info_.layout =
        WALLPAPER_LAYOUT_CENTER_CROPPED;
    info->type = current_user_wallpaper_info_.type =
        user_manager::User::DEFAULT;
    info->date = current_user_wallpaper_info_.date =
        base::Time::Now().LocalMidnight();
    return true;
  }

  return GetUserWallpaperInfo(
      user_manager::UserManager::Get()->GetLoggedInUser()->email(), info);
}

// static
bool WallpaperManagerBase::ResizeImage(
    const gfx::ImageSkia& image,
    WallpaperLayout layout,
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

  if (layout == WALLPAPER_LAYOUT_CENTER_CROPPED) {
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
  } else if (layout == WALLPAPER_LAYOUT_STRETCH) {
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
  SkAutoLockPixels lock_input(bitmap);
  gfx::JPEGCodec::Encode(
      reinterpret_cast<unsigned char*>(bitmap.getAddr32(0, 0)),
      gfx::JPEGCodec::FORMAT_SkBitmap, bitmap.width(), bitmap.height(),
      bitmap.width() * bitmap.bytesPerPixel(), kDefaultEncodingQuality,
      &(*output)->data());

  if (output_skia) {
    resized_image.MakeThreadSafe();
    *output_skia = resized_image;
  }

  return true;
}

// static
bool WallpaperManagerBase::ResizeAndSaveWallpaper(const gfx::ImageSkia& image,
                                                  const base::FilePath& path,
                                                  WallpaperLayout layout,
                                                  int preferred_width,
                                                  int preferred_height,
                                                  gfx::ImageSkia* output_skia) {
  if (layout == WALLPAPER_LAYOUT_CENTER) {
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

bool WallpaperManagerBase::IsPolicyControlled(
    const std::string& user_id) const {
  WallpaperInfo info;
  if (!GetUserWallpaperInfo(user_id, &info))
    return false;
  return info.type == user_manager::User::POLICY;
}

void WallpaperManagerBase::OnPolicySet(const std::string& policy,
                                       const std::string& user_id) {
  WallpaperInfo info;
  GetUserWallpaperInfo(user_id, &info);
  info.type = user_manager::User::POLICY;
  SetUserWallpaperInfo(user_id, info, true /* is_persistent */);
}

void WallpaperManagerBase::OnPolicyCleared(const std::string& policy,
                                           const std::string& user_id) {
  WallpaperInfo info;
  GetUserWallpaperInfo(user_id, &info);
  info.type = user_manager::User::DEFAULT;
  SetUserWallpaperInfo(user_id, info, true /* is_persistent */);
  SetDefaultWallpaperNow(user_id);
}

// static
base::FilePath WallpaperManagerBase::GetCustomWallpaperPath(
    const char* sub_dir,
    const std::string& user_id_hash,
    const std::string& file) {
  base::FilePath custom_wallpaper_path = GetCustomWallpaperDir(sub_dir);
  return custom_wallpaper_path.Append(user_id_hash).Append(file);
}

WallpaperManagerBase::WallpaperManagerBase()
    : loaded_wallpapers_for_test_(0),
      command_line_for_testing_(NULL),
      should_cache_wallpaper_(false),
      weak_factory_(this) {
  SetDefaultWallpaperPathsFromCommandLine(
      base::CommandLine::ForCurrentProcess());
  sequence_token_ = BrowserThread::GetBlockingPool()->GetNamedSequenceToken(
      kWallpaperSequenceTokenName);
  task_runner_ =
      BrowserThread::GetBlockingPool()
          ->GetSequencedTaskRunnerWithShutdownBehavior(
              sequence_token_, base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
}

WallpaperManagerBase::~WallpaperManagerBase() {
  // TODO(bshe): Lifetime of WallpaperManagerBase needs more consideration.
  // http://crbug.com/171694
  weak_factory_.InvalidateWeakPtrs();
}

void WallpaperManagerBase::SetPolicyControlledWallpaper(
    const std::string& user_id,
    const user_manager::UserImage& user_image) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(user_id);
  if (!user) {
    NOTREACHED() << "Unknown user.";
    return;
  }

  if (user->username_hash().empty()) {
    cryptohome::AsyncMethodCaller::GetInstance()->AsyncGetSanitizedUsername(
        user_id,
        base::Bind(&WallpaperManagerBase::SetCustomWallpaperOnSanitizedUsername,
                   weak_factory_.GetWeakPtr(), user_id, user_image.image(),
                   true /* update wallpaper */));
  } else {
    SetCustomWallpaper(user_id, user->username_hash(), "policy-controlled.jpeg",
                       WALLPAPER_LAYOUT_CENTER_CROPPED,
                       user_manager::User::POLICY, user_image.image(),
                       true /* update wallpaper */);
  }
}

void WallpaperManagerBase::SetCustomWallpaperOnSanitizedUsername(
    const std::string& user_id,
    const gfx::ImageSkia& image,
    bool update_wallpaper,
    bool cryptohome_success,
    const std::string& user_id_hash) {
  if (!cryptohome_success)
    return;
  SetCustomWallpaper(user_id, user_id_hash, "policy-controlled.jpeg",
                     WALLPAPER_LAYOUT_CENTER_CROPPED,
                     user_manager::User::POLICY, image, update_wallpaper);
}

// static
void WallpaperManagerBase::SaveCustomWallpaper(
    const std::string& user_id_hash,
    const base::FilePath& original_path,
    WallpaperLayout layout,
    scoped_ptr<gfx::ImageSkia> image) {
  base::DeleteFile(
      GetCustomWallpaperDir(kOriginalWallpaperSubDir).Append(user_id_hash),
      true /* recursive */);
  base::DeleteFile(
      GetCustomWallpaperDir(kSmallWallpaperSubDir).Append(user_id_hash),
      true /* recursive */);
  base::DeleteFile(
      GetCustomWallpaperDir(kLargeWallpaperSubDir).Append(user_id_hash),
      true /* recursive */);
  EnsureCustomWallpaperDirectories(user_id_hash);
  std::string file_name = original_path.BaseName().value();
  base::FilePath small_wallpaper_path =
      GetCustomWallpaperPath(kSmallWallpaperSubDir, user_id_hash, file_name);
  base::FilePath large_wallpaper_path =
      GetCustomWallpaperPath(kLargeWallpaperSubDir, user_id_hash, file_name);

  // Re-encode orginal file to jpeg format and saves the result in case that
  // resized wallpaper is not generated (i.e. chrome shutdown before resized
  // wallpaper is saved).
  ResizeAndSaveWallpaper(*image, original_path, WALLPAPER_LAYOUT_STRETCH,
                         image->width(), image->height(), NULL);
  ResizeAndSaveWallpaper(*image, small_wallpaper_path, layout,
                         kSmallWallpaperMaxWidth, kSmallWallpaperMaxHeight,
                         NULL);
  ResizeAndSaveWallpaper(*image, large_wallpaper_path, layout,
                         kLargeWallpaperMaxWidth, kLargeWallpaperMaxHeight,
                         NULL);
}

// static
void WallpaperManagerBase::MoveCustomWallpapersOnWorker(
    const std::string& user_id,
    const std::string& user_id_hash,
    base::WeakPtr<WallpaperManagerBase> weak_ptr) {
  if (MoveCustomWallpaperDirectory(kOriginalWallpaperSubDir, user_id,
                                   user_id_hash)) {
    // Consider success if the original wallpaper is moved to the new directory.
    // Original wallpaper is the fallback if the correct resolution wallpaper
    // can not be found.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperManagerBase::MoveCustomWallpapersSuccess, weak_ptr,
                   user_id, user_id_hash));
  }
  MoveCustomWallpaperDirectory(kLargeWallpaperSubDir, user_id, user_id_hash);
  MoveCustomWallpaperDirectory(kSmallWallpaperSubDir, user_id, user_id_hash);
  MoveCustomWallpaperDirectory(kThumbnailWallpaperSubDir, user_id,
                               user_id_hash);
}

// static
void WallpaperManagerBase::GetCustomWallpaperInternal(
    const std::string& user_id,
    const WallpaperInfo& info,
    const base::FilePath& wallpaper_path,
    bool update_wallpaper,
    MovableOnDestroyCallbackHolder on_finish,
    base::WeakPtr<WallpaperManagerBase> weak_ptr) {
  base::FilePath valid_path = wallpaper_path;
  if (!base::PathExists(wallpaper_path)) {
    // Falls back on original file if the correct resolution file does not
    // exist. This may happen when the original custom wallpaper is small or
    // browser shutdown before resized wallpaper saved.
    valid_path = GetCustomWallpaperDir(kOriginalWallpaperSubDir);
    valid_path = valid_path.Append(info.location);
  }

  if (!base::PathExists(valid_path)) {
    // Falls back to custom wallpaper that uses email as part of its file path.
    // Note that email is used instead of user_id_hash here.
    valid_path = GetCustomWallpaperPath(kOriginalWallpaperSubDir, user_id,
                                        info.location);
  }

  if (!base::PathExists(valid_path)) {
    LOG(ERROR) << "Failed to load previously selected custom wallpaper. "
               << "Fallback to default wallpaper";
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperManagerBase::DoSetDefaultWallpaper, weak_ptr,
                   user_id, base::Passed(on_finish.Pass())));
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperManagerBase::StartLoad, weak_ptr, user_id, info,
                   update_wallpaper, valid_path,
                   base::Passed(on_finish.Pass())));
  }
}

void WallpaperManagerBase::InitInitialUserWallpaper(const std::string& user_id,
                                                    bool is_persistent) {
  current_user_wallpaper_info_.location = "";
  current_user_wallpaper_info_.layout = WALLPAPER_LAYOUT_CENTER_CROPPED;
  current_user_wallpaper_info_.type = user_manager::User::DEFAULT;
  current_user_wallpaper_info_.date = base::Time::Now().LocalMidnight();

  WallpaperInfo info = current_user_wallpaper_info_;
  SetUserWallpaperInfo(user_id, info, is_persistent);
}

void WallpaperManagerBase::SetUserWallpaperDelayed(const std::string& user_id) {
  ScheduleSetUserWallpaper(user_id, true);
}

void WallpaperManagerBase::SetUserWallpaperNow(const std::string& user_id) {
  ScheduleSetUserWallpaper(user_id, false);
}

void WallpaperManagerBase::UpdateWallpaper(bool clear_cache) {
  FOR_EACH_OBSERVER(Observer, observers_, OnUpdateWallpaperForTesting());
  if (clear_cache)
    wallpaper_cache_.clear();
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

void WallpaperManagerBase::AddObserver(
    WallpaperManagerBase::Observer* observer) {
  observers_.AddObserver(observer);
}

void WallpaperManagerBase::RemoveObserver(
    WallpaperManagerBase::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void WallpaperManagerBase::NotifyAnimationFinished() {
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnWallpaperAnimationFinished(last_selected_user_));
}

// WallpaperManager, protected: -----------------------------------------------

bool WallpaperManagerBase::GetWallpaperFromCache(const std::string& user_id,
                                                 gfx::ImageSkia* image) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CustomWallpaperMap::const_iterator it = wallpaper_cache_.find(user_id);
  if (it != wallpaper_cache_.end() && !(*it).second.second.isNull()) {
    *image = (*it).second.second;
    return true;
  }
  return false;
}

bool WallpaperManagerBase::GetPathFromCache(const std::string& user_id,
                                            base::FilePath* path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CustomWallpaperMap::const_iterator it = wallpaper_cache_.find(user_id);
  if (it != wallpaper_cache_.end()) {
    *path = (*it).second.first;
    return true;
  }
  return false;
}

int WallpaperManagerBase::loaded_wallpapers_for_test() const {
  return loaded_wallpapers_for_test_;
}

void WallpaperManagerBase::CacheUsersWallpapers() {
  // TODO(dpolukhin): crbug.com/408734.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  user_manager::UserList users = user_manager::UserManager::Get()->GetUsers();

  if (!users.empty()) {
    user_manager::UserList::const_iterator it = users.begin();
    // Skip the wallpaper of first user in the list. It should have been cached.
    it++;
    for (int cached = 0; it != users.end() && cached < kMaxWallpapersToCache;
         ++it, ++cached) {
      std::string user_id = (*it)->email();
      CacheUserWallpaper(user_id);
    }
  }
}

void WallpaperManagerBase::CacheUserWallpaper(const std::string& user_id) {
  CustomWallpaperMap::iterator it = wallpaper_cache_.find(user_id);
  if (it != wallpaper_cache_.end() && !it->second.second.isNull())
    return;
  WallpaperInfo info;
  if (GetUserWallpaperInfo(user_id, &info)) {
    if (info.location.empty())
      return;

    base::FilePath wallpaper_dir;
    base::FilePath wallpaper_path;
    if (info.type == user_manager::User::CUSTOMIZED ||
        info.type == user_manager::User::POLICY) {
      const char* sub_dir = GetCustomWallpaperSubdirForCurrentResolution();
      base::FilePath wallpaper_path = GetCustomWallpaperDir(sub_dir);
      wallpaper_path = wallpaper_path.Append(info.location);
      // Set the path to the cache.
      wallpaper_cache_[user_id] = CustomWallpaperElement(wallpaper_path,
                                                         gfx::ImageSkia());
      task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&WallpaperManagerBase::GetCustomWallpaperInternal, user_id,
                     info, wallpaper_path, false /* do not update wallpaper */,
                     base::Passed(MovableOnDestroyCallbackHolder()),
                     weak_factory_.GetWeakPtr()));
      return;
    }
    LoadWallpaper(user_id, info, false /* do not update wallpaper */,
                  MovableOnDestroyCallbackHolder().Pass());
  }
}

void WallpaperManagerBase::DeleteUserWallpapers(
    const std::string& user_id,
    const std::string& path_to_file) {
  std::vector<base::FilePath> file_to_remove;
  // Remove small user wallpaper.
  base::FilePath wallpaper_path = GetCustomWallpaperDir(kSmallWallpaperSubDir);
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
      FROM_HERE, base::Bind(&DeleteWallpaperInList, file_to_remove), false);
}

void WallpaperManagerBase::SetCommandLineForTesting(
    base::CommandLine* command_line) {
  command_line_for_testing_ = command_line;
  SetDefaultWallpaperPathsFromCommandLine(command_line);
}

base::CommandLine* WallpaperManagerBase::GetCommandLine() {
  base::CommandLine* command_line =
      command_line_for_testing_ ? command_line_for_testing_
                                : base::CommandLine::ForCurrentProcess();
  return command_line;
}

void WallpaperManagerBase::LoadWallpaper(
    const std::string& user_id,
    const WallpaperInfo& info,
    bool update_wallpaper,
    MovableOnDestroyCallbackHolder on_finish) {
  base::FilePath wallpaper_dir;
  base::FilePath wallpaper_path;

  // Do a sanity check that file path information is not empty.
  if (info.type == user_manager::User::ONLINE ||
      info.type == user_manager::User::DEFAULT) {
    if (info.location.empty()) {
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
    std::string file_name = GURL(info.location).ExtractFileName();
    WallpaperResolution resolution = GetAppropriateResolution();
    // Only solid color wallpapers have stretch layout and they have only one
    // resolution.
    if (info.layout != WALLPAPER_LAYOUT_STRETCH &&
        resolution == WALLPAPER_RESOLUTION_SMALL) {
      file_name = base::FilePath(file_name)
                      .InsertBeforeExtension(kSmallWallpaperSuffix)
                      .value();
    }
    DCHECK(dir_chromeos_wallpapers_path_id != -1);
    CHECK(PathService::Get(dir_chromeos_wallpapers_path_id,
                           &wallpaper_dir));
    wallpaper_path = wallpaper_dir.Append(file_name);

    // If the wallpaper exists and it contains already the correct image we can
    // return immediately.
    CustomWallpaperMap::iterator it = wallpaper_cache_.find(user_id);
    if (it != wallpaper_cache_.end() &&
        it->second.first == wallpaper_path &&
        !it->second.second.isNull())
      return;

    loaded_wallpapers_for_test_++;
    StartLoad(user_id, info, update_wallpaper, wallpaper_path,
              on_finish.Pass());
  } else if (info.type == user_manager::User::DEFAULT) {
    // Default wallpapers are migrated from M21 user profiles. A code refactor
    // overlooked that case and caused these wallpapers not being loaded at all.
    // On some slow devices, it caused login webui not visible after upgrade to
    // M26 from M21. See crosbug.com/38429 for details.
    base::FilePath user_data_dir;
    DCHECK(dir_user_data_path_id != -1);
    PathService::Get(dir_user_data_path_id, &user_data_dir);
    wallpaper_path = user_data_dir.Append(info.location);
    StartLoad(user_id, info, update_wallpaper, wallpaper_path,
              on_finish.Pass());
  } else {
    // In unexpected cases, revert to default wallpaper to fail safely. See
    // crosbug.com/38429.
    LOG(ERROR) << "Wallpaper reverts to default unexpected.";
    DoSetDefaultWallpaper(user_id, on_finish.Pass());
  }
}

void WallpaperManagerBase::MoveCustomWallpapersSuccess(
    const std::string& user_id,
    const std::string& user_id_hash) {
  WallpaperInfo info;
  GetUserWallpaperInfo(user_id, &info);
  if (info.type == user_manager::User::CUSTOMIZED) {
    // New file field should include user id hash in addition to file name.
    // This is needed because at login screen, user id hash is not available.
    info.location = base::FilePath(user_id_hash).Append(info.location).value();
    bool is_persistent =
        !user_manager::UserManager::Get()->IsUserNonCryptohomeDataEphemeral(
            user_id);
    SetUserWallpaperInfo(user_id, info, is_persistent);
  }
}

void WallpaperManagerBase::MoveLoggedInUserCustomWallpaper() {
  const user_manager::User* logged_in_user =
      user_manager::UserManager::Get()->GetLoggedInUser();
  if (logged_in_user) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&WallpaperManagerBase::MoveCustomWallpapersOnWorker,
                   logged_in_user->email(), logged_in_user->username_hash(),
                   weak_factory_.GetWeakPtr()));
  }
}

void WallpaperManagerBase::SaveLastLoadTime(const base::TimeDelta elapsed) {
  while (last_load_times_.size() >= kLastLoadsStatsMsMaxSize)
    last_load_times_.pop_front();

  if (elapsed > base::TimeDelta::FromMicroseconds(0)) {
    last_load_times_.push_back(elapsed);
    last_load_finished_at_ = base::Time::Now();
  }
}

base::TimeDelta WallpaperManagerBase::GetWallpaperLoadDelay() const {
  base::TimeDelta delay;

  if (last_load_times_.size() == 0) {
    delay = base::TimeDelta::FromMilliseconds(kLoadDefaultDelayMs);
  } else {
    delay = std::accumulate(last_load_times_.begin(), last_load_times_.end(),
                            base::TimeDelta(), std::plus<base::TimeDelta>()) /
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

void WallpaperManagerBase::OnCustomizedDefaultWallpaperDecoded(
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
  base::Closure resize_closure = base::Bind(
      &WallpaperManagerBase::ResizeCustomizedDefaultWallpaper,
      base::Passed(&deep_copy), wallpaper.raw_image(),
      base::Unretained(rescaled_files.get()), base::Unretained(success.get()),
      base::Unretained(small_wallpaper_image.get()),
      base::Unretained(large_wallpaper_image.get()));
  base::Closure on_resized_closure = base::Bind(
      &WallpaperManagerBase::OnCustomizedDefaultWallpaperResized,
      weak_factory_.GetWeakPtr(), wallpaper_url,
      base::Passed(rescaled_files.Pass()), base::Passed(success.Pass()),
      base::Passed(small_wallpaper_image.Pass()),
      base::Passed(large_wallpaper_image.Pass()));

  if (!task_runner_->PostTaskAndReply(FROM_HERE, resize_closure,
                                      on_resized_closure)) {
    LOG(WARNING) << "Failed to start Customized Wallpaper resize.";
  }
}

void WallpaperManagerBase::ResizeCustomizedDefaultWallpaper(
    scoped_ptr<gfx::ImageSkia> image,
    const user_manager::UserImage::RawImage& raw_image,
    const CustomizedWallpaperRescaledFiles* rescaled_files,
    bool* success,
    gfx::ImageSkia* small_wallpaper_image,
    gfx::ImageSkia* large_wallpaper_image) {
  *success = true;

  *success &= ResizeAndSaveWallpaper(
      *image, rescaled_files->path_rescaled_small(), WALLPAPER_LAYOUT_STRETCH,
      kSmallWallpaperMaxWidth, kSmallWallpaperMaxHeight, small_wallpaper_image);

  *success &= ResizeAndSaveWallpaper(
      *image, rescaled_files->path_rescaled_large(), WALLPAPER_LAYOUT_STRETCH,
      kLargeWallpaperMaxWidth, kLargeWallpaperMaxHeight, large_wallpaper_image);
}

void WallpaperManagerBase::SetCustomizedDefaultWallpaper(
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
          downloaded_file, resized_directory.Append(downloaded_file_name +
                                                    kSmallWallpaperSuffix),
          resized_directory.Append(downloaded_file_name +
                                   kLargeWallpaperSuffix)));

  base::Closure check_file_exists = rescaled_files->CreateCheckerClosure();
  base::Closure on_checked_closure =
      base::Bind(&WallpaperManagerBase::SetCustomizedDefaultWallpaperAfterCheck,
                 weak_factory_.GetWeakPtr(), wallpaper_url, downloaded_file,
                 base::Passed(rescaled_files.Pass()));
  if (!BrowserThread::PostBlockingPoolTaskAndReply(FROM_HERE, check_file_exists,
                                                   on_checked_closure)) {
    LOG(WARNING) << "Failed to start check CheckCustomizedWallpaperFilesExist.";
  }
}

void WallpaperManagerBase::SetDefaultWallpaperPathsFromCommandLine(
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

const char*
WallpaperManagerBase::GetCustomWallpaperSubdirForCurrentResolution() {
  WallpaperResolution resolution = GetAppropriateResolution();
  return resolution == WALLPAPER_RESOLUTION_SMALL ? kSmallWallpaperSubDir
                                                  : kLargeWallpaperSubDir;
}

void WallpaperManagerBase::CreateSolidDefaultWallpaper() {
  loaded_wallpapers_for_test_++;
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(kDefaultWallpaperColor);
  const gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  default_wallpaper_image_.reset(new user_manager::UserImage(image));
}

}  // namespace wallpaper
