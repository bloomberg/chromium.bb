// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wallpaper_private_api.h"

#include <vector>

#include "ash/shell.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_util.h"
#include "base/file_util.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_image.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wallpaper_manager.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "googleurl/src/gurl.h"
#include "grit/app_locale_settings.h"
#include "grit/generated_resources.h"
#include "grit/platform_locale_settings.h"
#include "ui/aura/window_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/web_ui_util.h"

using base::BinaryValue;
using content::BrowserThread;

namespace {

// Keeps in sync (same order) with WallpaperLayout enum in header file.
const char* kWallpaperLayoutArrays[] = {
    "CENTER",
    "CENTER_CROPPED",
    "STRETCH",
    "TILE"
};

const int kWallpaperLayoutCount = arraysize(kWallpaperLayoutArrays);

ash::WallpaperLayout GetLayoutEnum(const std::string& layout) {
  for (int i = 0; i < kWallpaperLayoutCount; i++) {
    if (layout.compare(kWallpaperLayoutArrays[i]) == 0)
      return static_cast<ash::WallpaperLayout>(i);
  }
  // Default to use CENTER layout.
  return ash::WALLPAPER_LAYOUT_CENTER;
}

// Saves |data| as |file_name| to directory with |key|. Return false if the
// directory can not be found/created or failed to write file.
bool SaveData(int key, const std::string& file_name, const std::string& data) {
  FilePath data_dir;
  CHECK(PathService::Get(key, &data_dir));
  if (!file_util::DirectoryExists(data_dir) &&
      !file_util::CreateDirectory(data_dir)) {
    return false;
  }
  FilePath file_path = data_dir.Append(file_name);

  return file_util::PathExists(file_path) ||
         (file_util::WriteFile(file_path, data.c_str(),
                               data.size()) != -1);
}

// Gets |file_name| from directory with |key|. Return false if the directory can
// not be found or failed to read file to string |data|. Note if the |file_name|
// can not be found in the directory, return true with empty |data|. It is
// expected that we may try to access file which did not saved yet.
bool GetData(int key, const std::string& file_name, std::string* data) {
  FilePath data_dir;
  CHECK(PathService::Get(key, &data_dir));
  if (!file_util::DirectoryExists(data_dir) &&
      !file_util::CreateDirectory(data_dir))
    return false;

  FilePath file_path = data_dir.Append(file_name);

  return !file_util::PathExists(file_path) ||
         file_util::ReadFileToString(file_path, data);
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

  ~WindowStateManager() {
    for (std::vector<aura::Window*>::iterator iter = windows_.begin();
         iter != windows_.end(); ++iter) {
      (*iter)->RemoveObserver(this);
    }
  }

  void BuildWindowListAndMinimizeInactive(aura::Window* active_window) {
    windows_ = ash::WindowCycleController::BuildWindowList(NULL);
    // Remove active window.
    std::vector<aura::Window*>::iterator last =
        std::remove(windows_.begin(), windows_.end(), active_window);
    // Removes unfocusable windows.
    last =
        std::remove_if(
            windows_.begin(),
            last,
            std::ptr_fun(ash::wm::IsWindowMinimized));
    windows_.erase(last, windows_.end());

    for (std::vector<aura::Window*>::iterator iter = windows_.begin();
         iter != windows_.end(); ++iter) {
      (*iter)->AddObserver(this);
      ash::wm::MinimizeWindow(*iter);
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

bool WallpaperStringsFunction::RunImpl() {
  DictionaryValue* dict = new DictionaryValue();
  SetResult(dict);

#define SET_STRING(id, idr) \
  dict->SetString(id, l10n_util::GetStringUTF16(idr))
  SET_STRING("webFontFamily", IDS_WEB_FONT_FAMILY);
  SET_STRING("webFontSize", IDS_WEB_FONT_SIZE);
  SET_STRING("searchTextLabel", IDS_WALLPAPER_MANAGER_SEARCH_TEXT_LABEL);
  SET_STRING("authorLabel", IDS_WALLPAPER_MANAGER_AUTHOR_LABEL);
  SET_STRING("customCategoryLabel",
             IDS_WALLPAPER_MANAGER_CUSTOM_CATEGORY_LABEL);
  SET_STRING("selectCustomLabel",
             IDS_WALLPAPER_MANAGER_SELECT_CUSTOM_LABEL);
  SET_STRING("positionLabel", IDS_WALLPAPER_MANAGER_POSITION_LABEL);
  SET_STRING("colorLabel", IDS_WALLPAPER_MANAGER_COLOR_LABEL);
  SET_STRING("previewLabel", IDS_WALLPAPER_MANAGER_PREVIEW_LABEL);
  SET_STRING("downloadingLabel", IDS_WALLPAPER_MANAGER_DOWNLOADING_LABEL);
  SET_STRING("setWallpaperDaily", IDS_OPTIONS_SET_WALLPAPER_DAILY);
  SET_STRING("searchTextLabel", IDS_WALLPAPER_MANAGER_SEARCH_TEXT_LABEL);
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
  SET_STRING("learnMore", IDS_LEARN_MORE);
#undef SET_STRING

  webui::SetFontAndTextDirection(dict);

  chromeos::WallpaperManager* wallpaper_manager =
      chromeos::WallpaperManager::Get();
  chromeos::WallpaperInfo info;

  if (wallpaper_manager->GetLoggedInUserWallpaperInfo(&info)) {
    if (info.type == chromeos::User::ONLINE)
      dict->SetString("currentWallpaper", info.file);
    else if (info.type == chromeos::User::CUSTOMIZED)
      dict->SetString("currentWallpaper", "CUSTOM");
  }

  return true;
}

class WallpaperFunctionBase::WallpaperDecoder : public ImageDecoder::Delegate {
 public:
  explicit WallpaperDecoder(scoped_refptr<WallpaperFunctionBase> function)
      : function_(function) {
  }

  void Start(const std::string& image_data) {
    image_decoder_ = new ImageDecoder(this, image_data,
                                      ImageDecoder::ROBUST_JPEG_CODEC);
    image_decoder_->Start();
  }

  void Cancel() {
    cancel_flag_.Set();
  }

  virtual void OnImageDecoded(const ImageDecoder* decoder,
                              const SkBitmap& decoded_image) OVERRIDE {
    gfx::ImageSkia final_image(decoded_image);
    final_image.MakeThreadSafe();
    if (cancel_flag_.IsSet()) {
      function_->OnFailureOrCancel("");
      delete this;
      return;
    }
    function_->OnWallpaperDecoded(final_image);
    delete this;
  }

  virtual void OnDecodeImageFailed(const ImageDecoder* decoder) OVERRIDE {
    function_->OnFailureOrCancel(
        l10n_util::GetStringUTF8(IDS_WALLPAPER_MANAGER_INVALID_WALLPAPER));
    delete this;
  }

 private:
  scoped_refptr<WallpaperFunctionBase> function_;
  scoped_refptr<ImageDecoder> image_decoder_;
  base::CancellationFlag cancel_flag_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperDecoder);
};

WallpaperFunctionBase::WallpaperDecoder*
    WallpaperFunctionBase::wallpaper_decoder_;

WallpaperFunctionBase::WallpaperFunctionBase() {
}

WallpaperFunctionBase::~WallpaperFunctionBase() {
}

void WallpaperFunctionBase::StartDecode(const std::string& data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (wallpaper_decoder_)
    wallpaper_decoder_->Cancel();
  wallpaper_decoder_ = new WallpaperDecoder(this);
  wallpaper_decoder_->Start(data);
}

void WallpaperFunctionBase::OnFailureOrCancel(const std::string& error) {
  wallpaper_decoder_ = NULL;
  if (!error.empty())
    SetError(error);
  SendResponse(false);
}

WallpaperSetWallpaperIfExistFunction::WallpaperSetWallpaperIfExistFunction() {
}

WallpaperSetWallpaperIfExistFunction::~WallpaperSetWallpaperIfExistFunction() {
}

bool WallpaperSetWallpaperIfExistFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &url_));
  EXTENSION_FUNCTION_VALIDATE(!url_.empty());
  std::string file_name = GURL(url_).ExtractFileName();

  std::string layout_string;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &layout_string));
  EXTENSION_FUNCTION_VALIDATE(!layout_string.empty());
  layout_ = GetLayoutEnum(layout_string);

  sequence_token_ = BrowserThread::GetBlockingPool()->
      GetNamedSequenceToken(chromeos::kWallpaperSequenceTokenName);
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      BrowserThread::GetBlockingPool()->
          GetSequencedTaskRunnerWithShutdownBehavior(sequence_token_,
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);

  task_runner->PostTask(FROM_HERE,
      base::Bind(
          &WallpaperSetWallpaperIfExistFunction::ReadFileAndInitiateStartDecode,
          this,
          file_name));
  return true;
}

void WallpaperSetWallpaperIfExistFunction::ReadFileAndInitiateStartDecode(
    const std::string& file_name) {
  DCHECK(BrowserThread::GetBlockingPool()->IsRunningSequenceOnCurrentThread(
      sequence_token_));
  std::string data;
  FilePath data_dir;

  CHECK(PathService::Get(chrome::DIR_CHROMEOS_WALLPAPERS, &data_dir));
  FilePath file_path = data_dir.Append(file_name);

  if (file_util::PathExists(file_path) &&
      file_util::ReadFileToString(file_path, &data)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperSetWallpaperIfExistFunction::StartDecode, this,
                   data));
    return;
  }
  std::string error = base::StringPrintf(
        "Failed to set wallpaper %s from file system.", file_name.c_str());
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WallpaperSetWallpaperIfExistFunction::OnFailureOrCancel,
                 this, error));
}

void WallpaperSetWallpaperIfExistFunction::OnWallpaperDecoded(
    const gfx::ImageSkia& wallpaper) {
  // Set wallpaper_decoder_ to null since the decoding already finished.
  wallpaper_decoder_ = NULL;

  chromeos::WallpaperManager* wallpaper_manager =
      chromeos::WallpaperManager::Get();
  wallpaper_manager->SetWallpaperFromImageSkia(wallpaper, layout_);
  bool is_persistent =
      !chromeos::UserManager::Get()->IsCurrentUserNonCryptohomeDataEphemeral();
  chromeos::WallpaperInfo info = {
      url_,
      layout_,
      chromeos::User::ONLINE,
      base::Time::Now().LocalMidnight()
  };
  std::string email = chromeos::UserManager::Get()->GetLoggedInUser()->email();
  wallpaper_manager->SetUserWallpaperInfo(email, info, is_persistent);
  SendResponse(true);
}

WallpaperSetWallpaperFunction::WallpaperSetWallpaperFunction() {
}

WallpaperSetWallpaperFunction::~WallpaperSetWallpaperFunction() {
}

bool WallpaperSetWallpaperFunction::RunImpl() {
  BinaryValue* input = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBinary(0, &input));

  std::string layout_string;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &layout_string));
  EXTENSION_FUNCTION_VALIDATE(!layout_string.empty());
  layout_ = GetLayoutEnum(layout_string);

  EXTENSION_FUNCTION_VALIDATE(args_->GetString(2, &url_));
  EXTENSION_FUNCTION_VALIDATE(!url_.empty());

  // Gets email address while at UI thread.
  email_ = chromeos::UserManager::Get()->GetLoggedInUser()->email();

  image_data_.assign(input->GetBuffer(), input->GetSize());
  StartDecode(image_data_);

  return true;
}

void WallpaperSetWallpaperFunction::OnWallpaperDecoded(
    const gfx::ImageSkia& wallpaper) {
  wallpaper_ = wallpaper;
  // Set wallpaper_decoder_ to null since the decoding already finished.
  wallpaper_decoder_ = NULL;

  sequence_token_ = BrowserThread::GetBlockingPool()->
      GetNamedSequenceToken(chromeos::kWallpaperSequenceTokenName);
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      BrowserThread::GetBlockingPool()->
          GetSequencedTaskRunnerWithShutdownBehavior(sequence_token_,
              base::SequencedWorkerPool::BLOCK_SHUTDOWN);

  task_runner->PostTask(FROM_HERE,
      base::Bind(&WallpaperSetWallpaperFunction::SaveToFile, this));
}

void WallpaperSetWallpaperFunction::SaveToFile() {
  DCHECK(BrowserThread::GetBlockingPool()->IsRunningSequenceOnCurrentThread(
      sequence_token_));
  std::string file_name = GURL(url_).ExtractFileName();
  if (SaveData(chrome::DIR_CHROMEOS_WALLPAPERS, file_name, image_data_)) {
    wallpaper_.EnsureRepsForSupportedScaleFactors();
    scoped_ptr<gfx::ImageSkia> deep_copy(wallpaper_.DeepCopy());
    // ImageSkia is not RefCountedThreadSafe. Use a deep copied ImageSkia if
    // post to another thread.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperSetWallpaperFunction::SetDecodedWallpaper,
                   this, base::Passed(&deep_copy)));
    chromeos::UserImage wallpaper(wallpaper_);

    FilePath wallpaper_dir;
    CHECK(PathService::Get(chrome::DIR_CHROMEOS_WALLPAPERS, &wallpaper_dir));
    FilePath file_path = wallpaper_dir.Append(file_name).InsertBeforeExtension(
        chromeos::kSmallWallpaperSuffix);
    if (file_util::PathExists(file_path))
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
        base::Bind(&WallpaperSetWallpaperFunction::OnFailureOrCancel,
                   this, error));
  }
}

void WallpaperSetWallpaperFunction::SetDecodedWallpaper(
    scoped_ptr<gfx::ImageSkia> wallpaper) {
  chromeos::WallpaperManager* wallpaper_manager =
      chromeos::WallpaperManager::Get();
  wallpaper_manager->SetWallpaperFromImageSkia(*wallpaper.get(), layout_);
  bool is_persistent =
      !chromeos::UserManager::Get()->IsCurrentUserNonCryptohomeDataEphemeral();
  chromeos::WallpaperInfo info = {
      url_,
      layout_,
      chromeos::User::ONLINE,
      base::Time::Now().LocalMidnight()
  };
  wallpaper_manager->SetUserWallpaperInfo(email_, info, is_persistent);
  SendResponse(true);
}

WallpaperSetCustomWallpaperFunction::WallpaperSetCustomWallpaperFunction() {
}

WallpaperSetCustomWallpaperFunction::~WallpaperSetCustomWallpaperFunction() {
}

bool WallpaperSetCustomWallpaperFunction::RunImpl() {
  BinaryValue* input = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBinary(0, &input));

  std::string layout_string;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &layout_string));
  EXTENSION_FUNCTION_VALIDATE(!layout_string.empty());
  layout_ = GetLayoutEnum(layout_string);

  // Gets email address while at UI thread.
  email_ = chromeos::UserManager::Get()->GetLoggedInUser()->email();

  image_data_.assign(input->GetBuffer(), input->GetSize());
  StartDecode(image_data_);

  return true;
}

void WallpaperSetCustomWallpaperFunction::OnWallpaperDecoded(
    const gfx::ImageSkia& wallpaper) {
  chromeos::UserImage::RawImage raw_image(image_data_.begin(),
                                          image_data_.end());
  chromeos::UserImage image(wallpaper, raw_image);
  // In the new wallpaper picker UI, we do not depend on WallpaperDelegate
  // to refresh thumbnail. Uses a null delegate here.
  chromeos::WallpaperManager::Get()->SetCustomWallpaper(
      email_, layout_, chromeos::User::CUSTOMIZED, image);
  wallpaper_decoder_ = NULL;
  SendResponse(true);
}

WallpaperMinimizeInactiveWindowsFunction::
    WallpaperMinimizeInactiveWindowsFunction() {
}

WallpaperMinimizeInactiveWindowsFunction::
    ~WallpaperMinimizeInactiveWindowsFunction() {
}

bool WallpaperMinimizeInactiveWindowsFunction::RunImpl() {
  WindowStateManager::MinimizeInactiveWindows();
  return true;
}

WallpaperRestoreMinimizedWindowsFunction::
    WallpaperRestoreMinimizedWindowsFunction() {
}

WallpaperRestoreMinimizedWindowsFunction::
    ~WallpaperRestoreMinimizedWindowsFunction() {
}

bool WallpaperRestoreMinimizedWindowsFunction::RunImpl() {
  WindowStateManager::RestoreWindows();
  return true;
}

WallpaperGetThumbnailFunction::WallpaperGetThumbnailFunction() {
}

WallpaperGetThumbnailFunction::~WallpaperGetThumbnailFunction() {
}

bool WallpaperGetThumbnailFunction::RunImpl() {
  std::string url;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &url));
  EXTENSION_FUNCTION_VALIDATE(!url.empty());
  std::string file_name = GURL(url).ExtractFileName();
  sequence_token_ = BrowserThread::GetBlockingPool()->
      GetNamedSequenceToken(chromeos::kWallpaperSequenceTokenName);
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      BrowserThread::GetBlockingPool()->
          GetSequencedTaskRunnerWithShutdownBehavior(sequence_token_,
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);

  task_runner->PostTask(FROM_HERE,
      base::Bind(&WallpaperGetThumbnailFunction::Get, this, file_name));
  return true;
}

void WallpaperGetThumbnailFunction::Failure(const std::string& file_name) {
  SetError(base::StringPrintf("Failed to access wallpaper thumbnails for %s.",
                              file_name.c_str()));
  SendResponse(false);
}

void WallpaperGetThumbnailFunction::FileNotLoaded() {
  SendResponse(true);
}

void WallpaperGetThumbnailFunction::FileLoaded(const std::string& data) {
  BinaryValue* thumbnail = BinaryValue::CreateWithCopiedBuffer(data.c_str(),
                                                               data.size());
  SetResult(thumbnail);
  SendResponse(true);
}

void WallpaperGetThumbnailFunction::Get(const std::string& file_name) {
  DCHECK(BrowserThread::GetBlockingPool()->IsRunningSequenceOnCurrentThread(
      sequence_token_));
  std::string data;
  if (GetData(chrome::DIR_CHROMEOS_WALLPAPER_THUMBNAILS, file_name, &data)) {
    if (data.empty()) {
      BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperGetThumbnailFunction::FileNotLoaded, this));
    } else {
      BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperGetThumbnailFunction::FileLoaded, this, data));
    }
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperGetThumbnailFunction::Failure, this, file_name));
  }
}

WallpaperSaveThumbnailFunction::WallpaperSaveThumbnailFunction() {
}

WallpaperSaveThumbnailFunction::~WallpaperSaveThumbnailFunction() {
}

bool WallpaperSaveThumbnailFunction::RunImpl() {
  std::string url;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &url));
  EXTENSION_FUNCTION_VALIDATE(!url.empty());

  BinaryValue* input = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBinary(1, &input));

  std::string file_name = GURL(url).ExtractFileName();
  std::string data(input->GetBuffer(), input->GetSize());

  sequence_token_ = BrowserThread::GetBlockingPool()->
      GetNamedSequenceToken(chromeos::kWallpaperSequenceTokenName);
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      BrowserThread::GetBlockingPool()->
          GetSequencedTaskRunnerWithShutdownBehavior(sequence_token_,
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);

  task_runner->PostTask(FROM_HERE,
      base::Bind(&WallpaperSaveThumbnailFunction::Save,
                 this, data, file_name));
  return true;
}

void WallpaperSaveThumbnailFunction::Failure(const std::string& file_name) {
  SetError(base::StringPrintf("Failed to create/write thumbnail of %s.",
                              file_name.c_str()));
  SendResponse(false);
}

void WallpaperSaveThumbnailFunction::Success() {
  SendResponse(true);
}

void WallpaperSaveThumbnailFunction::Save(const std::string& data,
                                          const std::string& file_name) {
  DCHECK(BrowserThread::GetBlockingPool()->IsRunningSequenceOnCurrentThread(
      sequence_token_));
  if (SaveData(chrome::DIR_CHROMEOS_WALLPAPER_THUMBNAILS, file_name, data)) {
    BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WallpaperSaveThumbnailFunction::Success, this));
  } else {
    BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&WallpaperSaveThumbnailFunction::Failure,
                     this, file_name));
  }
}

WallpaperGetOfflineWallpaperListFunction::
    WallpaperGetOfflineWallpaperListFunction() {
}

WallpaperGetOfflineWallpaperListFunction::
    ~WallpaperGetOfflineWallpaperListFunction() {
}

bool WallpaperGetOfflineWallpaperListFunction::RunImpl() {
  sequence_token_ = BrowserThread::GetBlockingPool()->
      GetNamedSequenceToken(chromeos::kWallpaperSequenceTokenName);
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      BrowserThread::GetBlockingPool()->
          GetSequencedTaskRunnerWithShutdownBehavior(sequence_token_,
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);

  task_runner->PostTask(FROM_HERE,
      base::Bind(&WallpaperGetOfflineWallpaperListFunction::GetList, this));
  return true;
}

void WallpaperGetOfflineWallpaperListFunction::GetList() {
  DCHECK(BrowserThread::GetBlockingPool()->IsRunningSequenceOnCurrentThread(
      sequence_token_));
  FilePath wallpaper_dir;
  std::vector<std::string> file_list;
  CHECK(PathService::Get(chrome::DIR_CHROMEOS_WALLPAPERS, &wallpaper_dir));
  if (file_util::DirectoryExists(wallpaper_dir)) {
    file_util::FileEnumerator files(wallpaper_dir, false,
                                    file_util::FileEnumerator::FILES);
    for (FilePath current = files.Next(); !current.empty();
         current = files.Next()) {
      std::string file_name = current.BaseName().RemoveExtension().value();
      // Do not add file name of small resolution wallpaper to the list.
      if (!EndsWith(file_name, chromeos::kSmallWallpaperSuffix, true))
        file_list.push_back(current.BaseName().value());
    }
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WallpaperGetOfflineWallpaperListFunction::OnComplete,
                 this, file_list));
}

void WallpaperGetOfflineWallpaperListFunction::OnComplete(
    const std::vector<std::string>& file_list) {
  ListValue* results = new ListValue();
  results->AppendStrings(file_list);
  SetResult(results);
  SendResponse(true);
}
