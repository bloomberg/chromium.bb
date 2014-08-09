// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wallpaper_function_base.h"

#include "base/synchronization/cancellation_flag.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/login/login_state.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace wallpaper_api_util {
namespace {

// Keeps in sync (same order) with WallpaperLayout enum in header file.
const char* kWallpaperLayoutArrays[] = {
  "CENTER",
  "CENTER_CROPPED",
  "STRETCH",
  "TILE"
};

const int kWallpaperLayoutCount = arraysize(kWallpaperLayoutArrays);

} // namespace

const char kCancelWallpaperMessage[] = "Set wallpaper was canceled.";

ash::WallpaperLayout GetLayoutEnum(const std::string& layout) {
  for (int i = 0; i < kWallpaperLayoutCount; i++) {
    if (layout.compare(kWallpaperLayoutArrays[i]) == 0)
      return static_cast<ash::WallpaperLayout>(i);
  }
  // Default to use CENTER layout.
  return ash::WALLPAPER_LAYOUT_CENTER;
}

}  // namespace wallpaper_api_util

class WallpaperFunctionBase::UnsafeWallpaperDecoder
    : public ImageDecoder::Delegate {
 public:
  explicit UnsafeWallpaperDecoder(scoped_refptr<WallpaperFunctionBase> function)
      : function_(function) {
  }

  void Start(const std::string& image_data) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    // This function can only be called after user login. It is fine to use
    // unsafe image decoder here. Before user login, a robust jpeg decoder will
    // be used.
    CHECK(chromeos::LoginState::Get()->IsUserLoggedIn());
    unsafe_image_decoder_ = new ImageDecoder(this, image_data,
                                             ImageDecoder::DEFAULT_CODEC);
    scoped_refptr<base::MessageLoopProxy> task_runner =
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
    unsafe_image_decoder_->Start(task_runner);
  }

  void Cancel() {
    cancel_flag_.Set();
  }

  virtual void OnImageDecoded(const ImageDecoder* decoder,
                              const SkBitmap& decoded_image) OVERRIDE {
    // Make the SkBitmap immutable as we won't modify it. This is important
    // because otherwise it gets duplicated during painting, wasting memory.
    SkBitmap immutable(decoded_image);
    immutable.setImmutable();
    gfx::ImageSkia final_image = gfx::ImageSkia::CreateFrom1xBitmap(immutable);
    final_image.MakeThreadSafe();
    if (cancel_flag_.IsSet()) {
      function_->OnCancel();
      delete this;
      return;
    }
    function_->OnWallpaperDecoded(final_image);
    delete this;
  }

  virtual void OnDecodeImageFailed(const ImageDecoder* decoder) OVERRIDE {
    function_->OnFailure(
        l10n_util::GetStringUTF8(IDS_WALLPAPER_MANAGER_INVALID_WALLPAPER));
    delete this;
  }

 private:
  scoped_refptr<WallpaperFunctionBase> function_;
  scoped_refptr<ImageDecoder> unsafe_image_decoder_;
  base::CancellationFlag cancel_flag_;

  DISALLOW_COPY_AND_ASSIGN(UnsafeWallpaperDecoder);
};

WallpaperFunctionBase::UnsafeWallpaperDecoder*
    WallpaperFunctionBase::unsafe_wallpaper_decoder_;

WallpaperFunctionBase::WallpaperFunctionBase() {
}

WallpaperFunctionBase::~WallpaperFunctionBase() {
}

void WallpaperFunctionBase::StartDecode(const std::string& data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (unsafe_wallpaper_decoder_)
    unsafe_wallpaper_decoder_->Cancel();
  unsafe_wallpaper_decoder_ = new UnsafeWallpaperDecoder(this);
  unsafe_wallpaper_decoder_->Start(data);
}

void WallpaperFunctionBase::OnCancel() {
  unsafe_wallpaper_decoder_ = NULL;
  SetError(wallpaper_api_util::kCancelWallpaperMessage);
  SendResponse(false);
}

void WallpaperFunctionBase::OnFailure(const std::string& error) {
  unsafe_wallpaper_decoder_ = NULL;
  SetError(error);
  SendResponse(false);
}
