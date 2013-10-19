// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wallpaper_private_api.h"

#include <vector>

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_image.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wallpaper_manager.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "grit/app_locale_settings.h"
#include "grit/generated_resources.h"
#include "grit/platform_locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

using base::BinaryValue;
using content::BrowserThread;
namespace wallpaper_private = extensions::api::wallpaper_private;
namespace set_wallpaper_if_exists = wallpaper_private::SetWallpaperIfExists;
namespace set_wallpaper = wallpaper_private::SetWallpaper;
namespace set_custom_wallpaper = wallpaper_private::SetCustomWallpaper;
namespace set_custom_wallpaper_layout =
    wallpaper_private::SetCustomWallpaperLayout;
namespace get_thumbnail = wallpaper_private::GetThumbnail;
namespace save_thumbnail = wallpaper_private::SaveThumbnail;
namespace get_offline_wallpaper_list =
    wallpaper_private::GetOfflineWallpaperList;

namespace {

#if defined(GOOGLE_CHROME_BUILD)
const char kWallpaperManifestBaseURL[] = "https://commondatastorage.googleapis."
    "com/chromeos-wallpaper-public/manifest_";
#endif

// Saves |data| as |file_name| to directory with |key|. Return false if the
// directory can not be found/created or failed to write file.
bool SaveData(int key, const std::string& file_name, const std::string& data) {
  base::FilePath data_dir;
  CHECK(PathService::Get(key, &data_dir));
  if (!base::DirectoryExists(data_dir) &&
      !file_util::CreateDirectory(data_dir)) {
    return false;
  }
  base::FilePath file_path = data_dir.Append(file_name);

  return base::PathExists(file_path) ||
         (file_util::WriteFile(file_path, data.c_str(),
                               data.size()) != -1);
}

// Gets |file_name| from directory with |key|. Return false if the directory can
// not be found or failed to read file to string |data|. Note if the |file_name|
// can not be found in the directory, return true with empty |data|. It is
// expected that we may try to access file which did not saved yet.
bool GetData(const base::FilePath& path, std::string* data) {
  base::FilePath data_dir = path.DirName();
  if (!base::DirectoryExists(data_dir) &&
      !file_util::CreateDirectory(data_dir))
    return false;

  return !base::PathExists(path) ||
         base::ReadFileToString(path, data);
}

class WindowStateManager;

// static
WindowStateManager* g_window_state_manager = NULL;

// WindowStateManager remembers which windows have been minimized in order to
// restore them when the wallpaper viewer is hidden.
class WindowStateManager : public aura::WindowObserver {
 public:

  // Minimizes all windows except the active window.
  static void MinimizeInactiveWindows() {
    if (g_window_state_manager)
      delete g_window_state_manager;
    g_window_state_manager = new WindowStateManager();
    g_window_state_manager->BuildWindowListAndMinimizeInactive(
        ash::wm::GetActiveWindow());
  }

  // Activates all minimized windows restoring them to their previous state.
  // This should only be called after calling MinimizeInactiveWindows.
  static void RestoreWindows() {
    DCHECK(g_window_state_manager);
    g_window_state_manager->RestoreMinimizedWindows();
    delete g_window_state_manager;
    g_window_state_manager = NULL;
  }

 private:
  WindowStateManager() {}

  virtual ~WindowStateManager() {
    for (std::vector<aura::Window*>::iterator iter = windows_.begin();
         iter != windows_.end(); ++iter) {
      (*iter)->RemoveObserver(this);
    }
  }

  void BuildWindowListAndMinimizeInactive(aura::Window* active_window) {
    windows_ = ash::MruWindowTracker::BuildWindowList(false);
    // Remove active window.
    std::vector<aura::Window*>::iterator last =
        std::remove(windows_.begin(), windows_.end(), active_window);
    // Removes unfocusable windows.
    last = std::remove_if(
        windows_.begin(),
        last,
        std::ptr_fun(ash::wm::IsWindowMinimized));
    windows_.erase(last, windows_.end());

    for (std::vector<aura::Window*>::iterator iter = windows_.begin();
         iter != windows_.end(); ++iter) {
      (*iter)->AddObserver(this);
      ash::wm::GetWindowState(*iter)->Minimize();
    }
  }

  void RestoreMinimizedWindows() {
    for (std::vector<aura::Window*>::iterator iter = windows_.begin();
         iter != windows_.end(); ++iter) {
      ash::wm::ActivateWindow(*iter);
    }
  }

  // aura::WindowObserver overrides.
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE {
    window->RemoveObserver(this);
    std::vector<aura::Window*>::iterator i = std::find(windows_.begin(),
        windows_.end(), window);
    DCHECK(i != windows_.end());
    windows_.erase(i);
  }

  // List of minimized windows.
  std::vector<aura::Window*> windows_;
};

}  // namespace

bool WallpaperPrivateGetStringsFunction::RunImpl() {
  DictionaryValue* dict = new DictionaryValue();
  SetResult(dict);

#define SET_STRING(id, idr) \
  dict->SetString(id, l10n_util::GetStringUTF16(idr))
  SET_STRING("webFontFamily", IDS_WEB_FONT_FAMILY);
  SET_STRING("webFontSize", IDS_WEB_FONT_SIZE);
  SET_STRING("allCategoryLabel", IDS_WALLPAPER_MANAGER_ALL_CATEGORY_LABEL);
  SET_STRING("deleteCommandLabel", IDS_WALLPAPER_MANAGER_DELETE_COMMAND_LABEL);
  SET_STRING("customCategoryLabel",
             IDS_WALLPAPER_MANAGER_CUSTOM_CATEGORY_LABEL);
  SET_STRING("selectCustomLabel",
             IDS_WALLPAPER_MANAGER_SELECT_CUSTOM_LABEL);
  SET_STRING("positionLabel", IDS_WALLPAPER_MANAGER_POSITION_LABEL);
  SET_STRING("colorLabel", IDS_WALLPAPER_MANAGER_COLOR_LABEL);
  SET_STRING("centerCroppedLayout",
             IDS_OPTIONS_WALLPAPER_CENTER_CROPPED_LAYOUT);
  SET_STRING("centerLayout", IDS_OPTIONS_WALLPAPER_CENTER_LAYOUT);
  SET_STRING("stretchLayout", IDS_OPTIONS_WALLPAPER_STRETCH_LAYOUT);
  SET_STRING("connectionFailed", IDS_WALLPAPER_MANAGER_ACCESS_FAIL);
  SET_STRING("downloadFailed", IDS_WALLPAPER_MANAGER_DOWNLOAD_FAIL);
  SET_STRING("downloadCanceled", IDS_WALLPAPER_MANAGER_DOWNLOAD_CANCEL);
  SET_STRING("customWallpaperWarning",
             IDS_WALLPAPER_MANAGER_SHOW_CUSTOM_WALLPAPER_ON_START_WARNING);
  SET_STRING("accessFileFailure", IDS_WALLPAPER_MANAGER_ACCESS_FILE_FAILURE);
  SET_STRING("invalidWallpaper", IDS_WALLPAPER_MANAGER_INVALID_WALLPAPER);
  SET_STRING("surpriseMeLabel", IDS_WALLPAPER_MANAGER_SURPRISE_ME_LABEL);
  SET_STRING("learnMore", IDS_LEARN_MORE);
#undef SET_STRING

  webui::SetFontAndTextDirection(dict);

  chromeos::WallpaperManager* wallpaper_manager =
      chromeos::WallpaperManager::Get();
  chromeos::WallpaperInfo info;

  if (wallpaper_manager->GetLoggedInUserWallpaperInfo(&info))
    dict->SetString("currentWallpaper", info.file);

#if defined(GOOGLE_CHROME_BUILD)
  dict->SetString("manifestBaseURL", kWallpaperManifestBaseURL);
#endif

  return true;
}

WallpaperPrivateSetWallpaperIfExistsFunction::
    WallpaperPrivateSetWallpaperIfExistsFunction() {}

WallpaperPrivateSetWallpaperIfExistsFunction::
    ~WallpaperPrivateSetWallpaperIfExistsFunction() {}

bool WallpaperPrivateSetWallpaperIfExistsFunction::RunImpl() {
  params = set_wallpaper_if_exists::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string email = chromeos::UserManager::Get()->GetLoggedInUser()->email();

  base::FilePath wallpaper_path;
  base::FilePath fallback_path;
  ash::WallpaperResolution resolution = ash::Shell::GetInstance()->
      desktop_background_controller()->GetAppropriateResolution();

  std::string file_name = GURL(params->url).ExtractFileName();
  CHECK(PathService::Get(chrome::DIR_CHROMEOS_WALLPAPERS,
                         &wallpaper_path));
  fallback_path = wallpaper_path.Append(file_name);
  if (params->layout != wallpaper_private::WALLPAPER_LAYOUT_STRETCH &&
      resolution == ash::WALLPAPER_RESOLUTION_SMALL) {
    file_name = base::FilePath(file_name).InsertBeforeExtension(
        chromeos::kSmallWallpaperSuffix).value();
  }
  wallpaper_path = wallpaper_path.Append(file_name);

  sequence_token_ = BrowserThread::GetBlockingPool()->
      GetNamedSequenceToken(chromeos::kWallpaperSequenceTokenName);
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      BrowserThread::GetBlockingPool()->
          GetSequencedTaskRunnerWithShutdownBehavior(sequence_token_,
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);

  task_runner->PostTask(FROM_HERE,
      base::Bind(
          &WallpaperPrivateSetWallpaperIfExistsFunction::
              ReadFileAndInitiateStartDecode,
          this, wallpaper_path, fallback_path));
  return true;
}

void WallpaperPrivateSetWallpaperIfExistsFunction::
    ReadFileAndInitiateStartDecode(const base::FilePath& file_path,
                                   const base::FilePath& fallback_path) {
  DCHECK(BrowserThread::GetBlockingPool()->IsRunningSequenceOnCurrentThread(
      sequence_token_));
  std::string data;
  base::FilePath path = file_path;

  if (!base::PathExists(file_path))
    path = fallback_path;

  if (base::PathExists(path) &&
      base::ReadFileToString(path, &data)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperPrivateSetWallpaperIfExistsFunction::StartDecode,
                   this, data));
    return;
  }
  std::string error = base::StringPrintf(
        "Failed to set wallpaper %s from file system.",
        path.BaseName().value().c_str());
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WallpaperPrivateSetWallpaperIfExistsFunction::OnFileNotExists,
                 this, error));
}

void WallpaperPrivateSetWallpaperIfExistsFunction::OnWallpaperDecoded(
    const gfx::ImageSkia& wallpaper) {
  // Set unsafe_wallpaper_decoder_ to null since the decoding already finished.
  unsafe_wallpaper_decoder_ = NULL;

  chromeos::WallpaperManager* wallpaper_manager =
      chromeos::WallpaperManager::Get();
  ash::WallpaperLayout layout = wallpaper_api_util::GetLayoutEnum(
      wallpaper_private::ToString(params->layout));

  wallpaper_manager->SetWallpaperFromImageSkia(wallpaper, layout);
  bool is_persistent =
      !chromeos::UserManager::Get()->IsCurrentUserNonCryptohomeDataEphemeral();
  chromeos::WallpaperInfo info = {
      params->url,
      layout,
      chromeos::User::ONLINE,
      base::Time::Now().LocalMidnight()
  };
  std::string email = chromeos::UserManager::Get()->GetLoggedInUser()->email();
  wallpaper_manager->SetUserWallpaperInfo(email, info, is_persistent);
  SetResult(base::Value::CreateBooleanValue(true));
  SendResponse(true);
}

void WallpaperPrivateSetWallpaperIfExistsFunction::OnFileNotExists(
    const std::string& error) {
  SetResult(base::Value::CreateBooleanValue(false));
  OnFailure(error);
};

WallpaperPrivateSetWallpaperFunction::WallpaperPrivateSetWallpaperFunction() {
}

WallpaperPrivateSetWallpaperFunction::~WallpaperPrivateSetWallpaperFunction() {
}

bool WallpaperPrivateSetWallpaperFunction::RunImpl() {
  params = set_wallpaper::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  // Gets email address while at UI thread.
  email_ = chromeos::UserManager::Get()->GetLoggedInUser()->email();

  StartDecode(params->wallpaper);

  return true;
}

void WallpaperPrivateSetWallpaperFunction::OnWallpaperDecoded(
    const gfx::ImageSkia& wallpaper) {
  wallpaper_ = wallpaper;
  // Set unsafe_wallpaper_decoder_ to null since the decoding already finished.
  unsafe_wallpaper_decoder_ = NULL;

  sequence_token_ = BrowserThread::GetBlockingPool()->
      GetNamedSequenceToken(chromeos::kWallpaperSequenceTokenName);
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      BrowserThread::GetBlockingPool()->
          GetSequencedTaskRunnerWithShutdownBehavior(sequence_token_,
              base::SequencedWorkerPool::BLOCK_SHUTDOWN);

  task_runner->PostTask(FROM_HERE,
      base::Bind(&WallpaperPrivateSetWallpaperFunction::SaveToFile, this));
}

void WallpaperPrivateSetWallpaperFunction::SaveToFile() {
  DCHECK(BrowserThread::GetBlockingPool()->IsRunningSequenceOnCurrentThread(
      sequence_token_));
  std::string file_name = GURL(params->url).ExtractFileName();
  if (SaveData(chrome::DIR_CHROMEOS_WALLPAPERS, file_name, params->wallpaper)) {
    wallpaper_.EnsureRepsForSupportedScales();
    scoped_ptr<gfx::ImageSkia> deep_copy(wallpaper_.DeepCopy());
    // ImageSkia is not RefCountedThreadSafe. Use a deep copied ImageSkia if
    // post to another thread.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperPrivateSetWallpaperFunction::SetDecodedWallpaper,
                   this, base::Passed(&deep_copy)));
    chromeos::UserImage wallpaper(wallpaper_);

    base::FilePath wallpaper_dir;
    CHECK(PathService::Get(chrome::DIR_CHROMEOS_WALLPAPERS, &wallpaper_dir));
    base::FilePath file_path = wallpaper_dir.Append(
        file_name).InsertBeforeExtension(chromeos::kSmallWallpaperSuffix);
    if (base::PathExists(file_path))
      return;
    // Generates and saves small resolution wallpaper. Uses CENTER_CROPPED to
    // maintain the aspect ratio after resize.
    chromeos::WallpaperManager::Get()->ResizeAndSaveWallpaper(
        wallpaper,
        file_path,
        ash::WALLPAPER_LAYOUT_CENTER_CROPPED,
        ash::kSmallWallpaperMaxWidth,
        ash::kSmallWallpaperMaxHeight);
  } else {
    std::string error = base::StringPrintf(
        "Failed to create/write wallpaper to %s.", file_name.c_str());
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperPrivateSetWallpaperFunction::OnFailure,
                   this, error));
  }
}

void WallpaperPrivateSetWallpaperFunction::SetDecodedWallpaper(
    scoped_ptr<gfx::ImageSkia> wallpaper) {
  chromeos::WallpaperManager* wallpaper_manager =
      chromeos::WallpaperManager::Get();

  ash::WallpaperLayout layout = wallpaper_api_util::GetLayoutEnum(
      wallpaper_private::ToString(params->layout));

  wallpaper_manager->SetWallpaperFromImageSkia(*wallpaper.get(), layout);
  bool is_persistent =
      !chromeos::UserManager::Get()->IsCurrentUserNonCryptohomeDataEphemeral();
  chromeos::WallpaperInfo info = {
      params->url,
      layout,
      chromeos::User::ONLINE,
      base::Time::Now().LocalMidnight()
  };
  wallpaper_manager->SetUserWallpaperInfo(email_, info, is_persistent);
  SendResponse(true);
}

WallpaperPrivateResetWallpaperFunction::
    WallpaperPrivateResetWallpaperFunction() {}

WallpaperPrivateResetWallpaperFunction::
    ~WallpaperPrivateResetWallpaperFunction() {}

bool WallpaperPrivateResetWallpaperFunction::RunImpl() {
  chromeos::WallpaperManager* wallpaper_manager =
      chromeos::WallpaperManager::Get();
  chromeos::UserManager* user_manager = chromeos::UserManager::Get();

  std::string email = user_manager->GetLoggedInUser()->email();
  wallpaper_manager->RemoveUserWallpaperInfo(email);

  chromeos::WallpaperInfo info = {
      "",
      ash::WALLPAPER_LAYOUT_CENTER,
      chromeos::User::DEFAULT,
      base::Time::Now().LocalMidnight()
  };
  bool is_persistent =
      !user_manager->IsCurrentUserNonCryptohomeDataEphemeral();
  wallpaper_manager->SetUserWallpaperInfo(email, info, is_persistent);
  wallpaper_manager->SetDefaultWallpaper();
  return true;
}

WallpaperPrivateSetCustomWallpaperFunction::
    WallpaperPrivateSetCustomWallpaperFunction() {}

WallpaperPrivateSetCustomWallpaperFunction::
    ~WallpaperPrivateSetCustomWallpaperFunction() {}

bool WallpaperPrivateSetCustomWallpaperFunction::RunImpl() {
  params = set_custom_wallpaper::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  // Gets email address and username hash while at UI thread.
  email_ = chromeos::UserManager::Get()->GetLoggedInUser()->email();
  user_id_hash_ =
      chromeos::UserManager::Get()->GetLoggedInUser()->username_hash();

  StartDecode(params->wallpaper);

  return true;
}

void WallpaperPrivateSetCustomWallpaperFunction::OnWallpaperDecoded(
    const gfx::ImageSkia& wallpaper) {
  chromeos::WallpaperManager* wallpaper_manager =
      chromeos::WallpaperManager::Get();
  chromeos::UserImage::RawImage raw_image(params->wallpaper.begin(),
                                          params->wallpaper.end());
  chromeos::UserImage image(wallpaper, raw_image);
  base::FilePath thumbnail_path = wallpaper_manager->GetCustomWallpaperPath(
      chromeos::kThumbnailWallpaperSubDir, user_id_hash_, params->file_name);

  sequence_token_ = BrowserThread::GetBlockingPool()->
      GetNamedSequenceToken(chromeos::kWallpaperSequenceTokenName);
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      BrowserThread::GetBlockingPool()->
          GetSequencedTaskRunnerWithShutdownBehavior(sequence_token_,
              base::SequencedWorkerPool::BLOCK_SHUTDOWN);

  ash::WallpaperLayout layout = wallpaper_api_util::GetLayoutEnum(
      wallpaper_private::ToString(params->layout));

  wallpaper_manager->SetCustomWallpaper(email_,
                                        user_id_hash_,
                                        params->file_name,
                                        layout,
                                        chromeos::User::CUSTOMIZED,
                                        image);
  unsafe_wallpaper_decoder_ = NULL;

  if (params->generate_thumbnail) {
    wallpaper.EnsureRepsForSupportedScales();
    scoped_ptr<gfx::ImageSkia> deep_copy(wallpaper.DeepCopy());
    // Generates thumbnail before call api function callback. We can then
    // request thumbnail in the javascript callback.
    task_runner->PostTask(FROM_HERE,
        base::Bind(
            &WallpaperPrivateSetCustomWallpaperFunction::GenerateThumbnail,
            this, thumbnail_path, base::Passed(&deep_copy)));
  } else {
    SendResponse(true);
  }
}

void WallpaperPrivateSetCustomWallpaperFunction::GenerateThumbnail(
    const base::FilePath& thumbnail_path, scoped_ptr<gfx::ImageSkia> image) {
  DCHECK(BrowserThread::GetBlockingPool()->IsRunningSequenceOnCurrentThread(
      sequence_token_));
  chromeos::UserImage wallpaper(*image.get());
  if (!base::PathExists(thumbnail_path.DirName()))
    file_util::CreateDirectory(thumbnail_path.DirName());

  scoped_refptr<base::RefCountedBytes> data;
  chromeos::WallpaperManager::Get()->ResizeWallpaper(
      wallpaper,
      ash::WALLPAPER_LAYOUT_STRETCH,
      ash::kWallpaperThumbnailWidth,
      ash::kWallpaperThumbnailHeight,
      &data);
  BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &WallpaperPrivateSetCustomWallpaperFunction::ThumbnailGenerated,
            this, data));
}

void WallpaperPrivateSetCustomWallpaperFunction::ThumbnailGenerated(
    base::RefCountedBytes* data) {
  BinaryValue* result = BinaryValue::CreateWithCopiedBuffer(
      reinterpret_cast<const char*>(data->front()), data->size());
  SetResult(result);
  SendResponse(true);
}

WallpaperPrivateSetCustomWallpaperLayoutFunction::
    WallpaperPrivateSetCustomWallpaperLayoutFunction() {}

WallpaperPrivateSetCustomWallpaperLayoutFunction::
    ~WallpaperPrivateSetCustomWallpaperLayoutFunction() {}

bool WallpaperPrivateSetCustomWallpaperLayoutFunction::RunImpl() {
  scoped_ptr<set_custom_wallpaper_layout::Params> params(
      set_custom_wallpaper_layout::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  chromeos::WallpaperManager* wallpaper_manager =
      chromeos::WallpaperManager::Get();
  chromeos::WallpaperInfo info;
  wallpaper_manager->GetLoggedInUserWallpaperInfo(&info);
  if (info.type != chromeos::User::CUSTOMIZED) {
    SetError("Only custom wallpaper can change layout.");
    SendResponse(false);
    return false;
  }
  info.layout = wallpaper_api_util::GetLayoutEnum(
      wallpaper_private::ToString(params->layout));

  std::string email = chromeos::UserManager::Get()->GetLoggedInUser()->email();
  bool is_persistent =
      !chromeos::UserManager::Get()->IsCurrentUserNonCryptohomeDataEphemeral();
  wallpaper_manager->SetUserWallpaperInfo(email, info, is_persistent);
  wallpaper_manager->UpdateWallpaper();
  SendResponse(true);

  // Gets email address while at UI thread.
  return true;
}

WallpaperPrivateMinimizeInactiveWindowsFunction::
    WallpaperPrivateMinimizeInactiveWindowsFunction() {
}

WallpaperPrivateMinimizeInactiveWindowsFunction::
    ~WallpaperPrivateMinimizeInactiveWindowsFunction() {
}

bool WallpaperPrivateMinimizeInactiveWindowsFunction::RunImpl() {
  WindowStateManager::MinimizeInactiveWindows();
  return true;
}

WallpaperPrivateRestoreMinimizedWindowsFunction::
    WallpaperPrivateRestoreMinimizedWindowsFunction() {
}

WallpaperPrivateRestoreMinimizedWindowsFunction::
    ~WallpaperPrivateRestoreMinimizedWindowsFunction() {
}

bool WallpaperPrivateRestoreMinimizedWindowsFunction::RunImpl() {
  WindowStateManager::RestoreWindows();
  return true;
}

WallpaperPrivateGetThumbnailFunction::WallpaperPrivateGetThumbnailFunction() {
}

WallpaperPrivateGetThumbnailFunction::~WallpaperPrivateGetThumbnailFunction() {
}

bool WallpaperPrivateGetThumbnailFunction::RunImpl() {
  scoped_ptr<get_thumbnail::Params> params(
      get_thumbnail::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  base::FilePath thumbnail_path;
  std::string email = chromeos::UserManager::Get()->GetLoggedInUser()->email();
  if (params->source == get_thumbnail::Params::SOURCE_ONLINE) {
    std::string file_name = GURL(params->url_or_file).ExtractFileName();
    CHECK(PathService::Get(chrome::DIR_CHROMEOS_WALLPAPER_THUMBNAILS,
                           &thumbnail_path));
    thumbnail_path = thumbnail_path.Append(file_name);
  } else {
    std::string file_name = params->url_or_file;
    std::string username_hash =
        chromeos::UserManager::Get()->GetLoggedInUser()->username_hash();
    thumbnail_path = chromeos::WallpaperManager::Get()->
        GetCustomWallpaperPath(chromeos::kThumbnailWallpaperSubDir,
                               username_hash,
                               file_name);
  }

  sequence_token_ = BrowserThread::GetBlockingPool()->
      GetNamedSequenceToken(chromeos::kWallpaperSequenceTokenName);
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      BrowserThread::GetBlockingPool()->
          GetSequencedTaskRunnerWithShutdownBehavior(sequence_token_,
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);

  task_runner->PostTask(FROM_HERE,
      base::Bind(&WallpaperPrivateGetThumbnailFunction::Get, this,
                 thumbnail_path));
  return true;
}

void WallpaperPrivateGetThumbnailFunction::Failure(
    const std::string& file_name) {
  SetError(base::StringPrintf("Failed to access wallpaper thumbnails for %s.",
                              file_name.c_str()));
  SendResponse(false);
}

void WallpaperPrivateGetThumbnailFunction::FileNotLoaded() {
  SendResponse(true);
}

void WallpaperPrivateGetThumbnailFunction::FileLoaded(
    const std::string& data) {
  BinaryValue* thumbnail = BinaryValue::CreateWithCopiedBuffer(data.c_str(),
                                                               data.size());
  SetResult(thumbnail);
  SendResponse(true);
}

void WallpaperPrivateGetThumbnailFunction::Get(const base::FilePath& path) {
  DCHECK(BrowserThread::GetBlockingPool()->IsRunningSequenceOnCurrentThread(
      sequence_token_));
  std::string data;
  if (GetData(path, &data)) {
    if (data.empty()) {
      BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperPrivateGetThumbnailFunction::FileNotLoaded, this));
    } else {
      BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperPrivateGetThumbnailFunction::FileLoaded, this,
                   data));
    }
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperPrivateGetThumbnailFunction::Failure, this,
                   path.BaseName().value()));
  }
}

WallpaperPrivateSaveThumbnailFunction::WallpaperPrivateSaveThumbnailFunction() {
}

WallpaperPrivateSaveThumbnailFunction::
    ~WallpaperPrivateSaveThumbnailFunction() {}

bool WallpaperPrivateSaveThumbnailFunction::RunImpl() {
  scoped_ptr<save_thumbnail::Params> params(
      save_thumbnail::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  sequence_token_ = BrowserThread::GetBlockingPool()->
      GetNamedSequenceToken(chromeos::kWallpaperSequenceTokenName);
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      BrowserThread::GetBlockingPool()->
          GetSequencedTaskRunnerWithShutdownBehavior(sequence_token_,
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);

  task_runner->PostTask(FROM_HERE,
      base::Bind(&WallpaperPrivateSaveThumbnailFunction::Save,
                 this, params->data, GURL(params->url).ExtractFileName()));
  return true;
}

void WallpaperPrivateSaveThumbnailFunction::Failure(
    const std::string& file_name) {
  SetError(base::StringPrintf("Failed to create/write thumbnail of %s.",
                              file_name.c_str()));
  SendResponse(false);
}

void WallpaperPrivateSaveThumbnailFunction::Success() {
  SendResponse(true);
}

void WallpaperPrivateSaveThumbnailFunction::Save(const std::string& data,
                                          const std::string& file_name) {
  DCHECK(BrowserThread::GetBlockingPool()->IsRunningSequenceOnCurrentThread(
      sequence_token_));
  if (SaveData(chrome::DIR_CHROMEOS_WALLPAPER_THUMBNAILS, file_name, data)) {
    BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WallpaperPrivateSaveThumbnailFunction::Success, this));
  } else {
    BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&WallpaperPrivateSaveThumbnailFunction::Failure,
                     this, file_name));
  }
}

WallpaperPrivateGetOfflineWallpaperListFunction::
    WallpaperPrivateGetOfflineWallpaperListFunction() {
}

WallpaperPrivateGetOfflineWallpaperListFunction::
    ~WallpaperPrivateGetOfflineWallpaperListFunction() {
}

bool WallpaperPrivateGetOfflineWallpaperListFunction::RunImpl() {
  sequence_token_ = BrowserThread::GetBlockingPool()->
      GetNamedSequenceToken(chromeos::kWallpaperSequenceTokenName);
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      BrowserThread::GetBlockingPool()->
          GetSequencedTaskRunnerWithShutdownBehavior(sequence_token_,
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);

  task_runner->PostTask(FROM_HERE,
      base::Bind(&WallpaperPrivateGetOfflineWallpaperListFunction::GetList,
                 this));
  return true;
}

void WallpaperPrivateGetOfflineWallpaperListFunction::GetList() {
  DCHECK(BrowserThread::GetBlockingPool()->IsRunningSequenceOnCurrentThread(
      sequence_token_));
  std::vector<std::string> file_list;
  base::FilePath wallpaper_dir;
  CHECK(PathService::Get(chrome::DIR_CHROMEOS_WALLPAPERS, &wallpaper_dir));
  if (base::DirectoryExists(wallpaper_dir)) {
    base::FileEnumerator files(wallpaper_dir, false,
                               base::FileEnumerator::FILES);
    for (base::FilePath current = files.Next(); !current.empty();
         current = files.Next()) {
      std::string file_name = current.BaseName().RemoveExtension().value();
      // Do not add file name of small resolution wallpaper to the list.
      if (!EndsWith(file_name, chromeos::kSmallWallpaperSuffix, true))
        file_list.push_back(current.BaseName().value());
    }
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WallpaperPrivateGetOfflineWallpaperListFunction::OnComplete,
                 this, file_list));
}

void WallpaperPrivateGetOfflineWallpaperListFunction::OnComplete(
    const std::vector<std::string>& file_list) {
  ListValue* results = new ListValue();
  results->AppendStrings(file_list);
  SetResult(results);
  SendResponse(true);
}
