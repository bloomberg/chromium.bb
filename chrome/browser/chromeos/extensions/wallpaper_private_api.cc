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
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/platform_locale_settings.h"
#include "ui/aura/window_observer.h"
#include "ui/base/l10n/l10n_util.h"

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

  ChromeURLDataManager::DataSource::SetFontAndTextDirection(dict);

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

void WallpaperFunctionBase::OnFailureOrCancel(const std::string& error) {
  wallpaper_decoder_ = NULL;
  if (!error.empty())
    SetError(error);
  SendResponse(false);
}

WallpaperSetWallpaperFunction::WallpaperSetWallpaperFunction() {
}

WallpaperSetWallpaperFunction::~WallpaperSetWallpaperFunction() {
}

bool WallpaperSetWallpaperFunction::RunImpl() {
  BinaryValue* input = NULL;
  if (args_ == NULL || !args_->GetBinary(0, &input)) {
    return false;
  }
  std::string layout_string;
  if (!args_->GetString(1, &layout_string) || layout_string.empty()) {
    return false;
  }
  layout_ = GetLayoutEnum(layout_string);
  if (!args_->GetString(2, &url_) || url_.empty())
    return false;

  // Gets email address while at UI thread.
  email_ = chromeos::UserManager::Get()->GetLoggedInUser()->email();

  image_data_.assign(input->GetBuffer(), input->GetSize());
  if (wallpaper_decoder_)
    wallpaper_decoder_->Cancel();
  wallpaper_decoder_ = new WallpaperDecoder(this);
  wallpaper_decoder_->Start(image_data_);

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
      base::Bind(&WallpaperSetWallpaperFunction::SaveToFile,
                 this));
}

void WallpaperSetWallpaperFunction::SaveToFile() {
  DCHECK(BrowserThread::GetBlockingPool()->IsRunningSequenceOnCurrentThread(
      sequence_token_));
  FilePath wallpaper_dir;
  CHECK(PathService::Get(chrome::DIR_CHROMEOS_WALLPAPERS, &wallpaper_dir));
  if (!file_util::DirectoryExists(wallpaper_dir) &&
      !file_util::CreateDirectory(wallpaper_dir)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperSetWallpaperFunction::OnFailureOrCancel,
                   this, ""));
    LOG(ERROR) << "Failed to create wallpaper directory.";
    return;
  }
  std::string file_name = GURL(url_).ExtractFileName();
  FilePath file_path = wallpaper_dir.Append(file_name);
  if (file_util::PathExists(file_path) ||
      file_util::WriteFile(file_path, image_data_.c_str(),
                           image_data_.size()) != -1 ) {
    wallpaper_.EnsureRepsForSupportedScaleFactors();
    scoped_ptr<gfx::ImageSkia> deep_copy(wallpaper_.DeepCopy());
    // ImageSkia is not RefCountedThreadSafe. Use a deep copied ImageSkia if
    // post to another thread.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperSetWallpaperFunction::SetDecodedWallpaper,
                   this, base::Passed(&deep_copy)));
    chromeos::UserImage wallpaper(wallpaper_);

    // Generates and saves small resolution wallpaper. Uses CENTER_CROPPED to
    // maintain the aspect ratio after resize.
    chromeos::WallpaperManager::Get()->ResizeAndSaveWallpaper(
        wallpaper,
        file_path.InsertBeforeExtension(chromeos::kSmallWallpaperSuffix),
        ash::WALLPAPER_LAYOUT_CENTER_CROPPED,
        ash::kSmallWallpaperMaxWidth,
        ash::kSmallWallpaperMaxHeight);
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperSetWallpaperFunction::OnFailureOrCancel,
                   this, ""));
    LOG(ERROR) << "Failed to save downloaded wallpaper.";
  }
}

void WallpaperSetWallpaperFunction::SetDecodedWallpaper(
    scoped_ptr<gfx::ImageSkia> wallpaper) {
  chromeos::WallpaperManager* wallpaper_manager =
      chromeos::WallpaperManager::Get();
  wallpaper_manager->SetWallpaperFromImageSkia(*wallpaper.get(), layout_);
  bool is_persistent =
      !chromeos::UserManager::Get()->IsCurrentUserEphemeral();
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
  if (args_ == NULL || !args_->GetBinary(0, &input)) {
    return false;
  }
  std::string layout_string;
  if (!args_->GetString(1, &layout_string) || layout_string.empty()) {
    return false;
  }
  layout_ = GetLayoutEnum(layout_string);

  // Gets email address while at UI thread.
  email_ = chromeos::UserManager::Get()->GetLoggedInUser()->email();

  image_data_.assign(input->GetBuffer(), input->GetSize());
  if (wallpaper_decoder_)
    wallpaper_decoder_->Cancel();
  wallpaper_decoder_ = new WallpaperDecoder(this);
  wallpaper_decoder_->Start(image_data_);

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
