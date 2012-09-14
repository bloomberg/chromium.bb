// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wallpaper_private_api.h"

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/shell.h"
#include "base/file_util.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/synchronization/cancellation_flag.h"
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
#include "grit/generated_resources.h"
#include "grit/platform_locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

using base::BinaryValue;
using content::BrowserThread;

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
  SET_STRING("customWallpaperWarning",
             IDS_WALLPAPER_MANAGER_SHOW_CUSTOM_WALLPAPER_ON_START_WARNING);
#undef SET_STRING

  ChromeURLDataManager::DataSource::SetFontAndTextDirection(dict);

  chromeos::WallpaperManager* wallpaper_manager =
      chromeos::WallpaperManager::Get();
  chromeos::WallpaperInfo info;

  if (wallpaper_manager->GetLoggedInUserWallpaperInfo(&info)) {
    if (info.type == chromeos::User::ONLINE)
      dict->SetString("selectedWallpaper", info.file);
    else if (info.type == chromeos::User::CUSTOMIZED)
      dict->SetString("selectedWallpaper", "CUSTOM");
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
    function_->SendResponse(false);
  }

  virtual void OnImageDecoded(const ImageDecoder* decoder,
                              const SkBitmap& decoded_image) OVERRIDE {
    gfx::ImageSkia final_image(decoded_image);
    if (cancel_flag_.IsSet()) {
      delete this;
      return;
    }
    function_->OnWallpaperDecoded(final_image);
    delete this;
  }

  virtual void OnDecodeImageFailed(const ImageDecoder* decoder) OVERRIDE {
    if (cancel_flag_.IsSet()) {
      delete this;
      return;
    }
    function_->OnFailure();
    // TODO(bshe): Dispatches an encoding error event.
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
  layout_ = ash::GetLayoutEnum(layout_string);
  if (!args_->GetString(2, &url_) || url_.empty())
    return false;

  // Gets email address while at UI thread.
  email_ = chromeos::UserManager::Get()->GetLoggedInUser().email();

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
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&WallpaperSetWallpaperFunction::SaveToFile,
                 this));
}

void WallpaperSetWallpaperFunction::OnFailure() {
  wallpaper_decoder_ = NULL;
  SendResponse(false);
}

void WallpaperSetWallpaperFunction::SaveToFile() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  FilePath wallpaper_dir;
  CHECK(PathService::Get(chrome::DIR_CHROMEOS_WALLPAPERS, &wallpaper_dir));
  if (!file_util::DirectoryExists(wallpaper_dir) &&
      !file_util::CreateDirectory(wallpaper_dir)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperSetWallpaperFunction::OnFailure,
                   this));
    return;
  }
  std::string file_name = GURL(url_).ExtractFileName();
  FilePath file_path = wallpaper_dir.Append(file_name);
  if (file_util::PathExists(file_path) ||
      file_util::WriteFile(file_path, image_data_.c_str(),
                           image_data_.size()) != -1 ) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperSetWallpaperFunction::SetDecodedWallpaper,
                   this));
    chromeos::UserImage wallpaper(wallpaper_);

    // Generates and saves small resolution wallpaper. Uses CENTER_CROPPED to
    // maintain the aspect ratio after resize.
    chromeos::WallpaperManager::Get()->ResizeAndSaveWallpaper(
        wallpaper,
        file_path.InsertBeforeExtension(chromeos::kSmallWallpaperSuffix),
        ash::CENTER_CROPPED,
        ash::kSmallWallpaperMaxWidth,
        ash::kSmallWallpaperMaxHeight);
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperSetWallpaperFunction::OnFailure,
                   this));
  }
}

void WallpaperSetWallpaperFunction::SetDecodedWallpaper() {
  chromeos::WallpaperManager* wallpaper_manager =
      chromeos::WallpaperManager::Get();
  wallpaper_manager->SetWallpaperFromImageSkia(wallpaper_, layout_);
  bool is_persistent =
      !chromeos::UserManager::Get()->IsCurrentUserEphemeral();
  chromeos::WallpaperInfo info = {
      url_,
      layout_,
      chromeos::User::ONLINE,
      base::Time::Now().LocalMidnight()
  };
  wallpaper_manager->SetUserWallpaperInfo(email_, info, is_persistent);
  wallpaper_decoder_ = NULL;
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
  layout_ = ash::GetLayoutEnum(layout_string);

  // Gets email address while at UI thread.
  email_ = chromeos::UserManager::Get()->GetLoggedInUser().email();

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
      email_, layout_, chromeos::User::CUSTOMIZED,
      base::WeakPtr<chromeos::WallpaperDelegate>(), image);
  wallpaper_decoder_ = NULL;
  SendResponse(true);
}

void WallpaperSetCustomWallpaperFunction::OnFailure() {
  wallpaper_decoder_ = NULL;
  SendResponse(false);
}
