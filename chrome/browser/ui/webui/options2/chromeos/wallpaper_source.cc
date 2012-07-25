// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options2/chromeos/wallpaper_source.h"

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/shell.h"
#include "base/debug/trace_event.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/ui/webui/options2/chromeos/simple_png_encoder.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "grit/ui_resources.h"
#include "net/base/mime_util.h"

namespace chromeos {
namespace options2 {

WallpaperImageSource::WallpaperImageSource()
    : DataSource(chrome::kChromeUIWallpaperImageHost, NULL),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

WallpaperImageSource::~WallpaperImageSource() {
}

void WallpaperImageSource::StartDataRequest(const std::string& email,
                                            bool is_incognito,
                                            int request_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  TRACE_EVENT_ASYNC_BEGIN0("SCREEN_LOCK", "GetUserWallpaperDataRequest",
                           request_id);
  CancelPendingEncodingOperation();
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&WallpaperImageSource::GetCurrentUserWallpaper, this,
                 request_id));
}

std::string WallpaperImageSource::GetMimeType(const std::string&) const {
  return "images/png";
}

// Get current background image and store it to |data|.
void WallpaperImageSource::GetCurrentUserWallpaper(int request_id) {
  SkBitmap image;
  TRACE_EVENT0("LOCK_SCREEN", "GetCurrentUserWallpaper");
  if (chromeos::UserManager::Get()->IsUserLoggedIn()) {
      SkBitmap wallpaper = ash::Shell::GetInstance()->
          desktop_background_controller()->
              GetCurrentWallpaperImage();
    SkBitmap copy;
    if (wallpaper.deepCopyTo(&copy, wallpaper.config()))
      image = copy;
  }
  content::BrowserThread::PostTask(
    content::BrowserThread::IO,
    FROM_HERE,
    base::Bind(&WallpaperImageSource::ImageAcquired, this, image, request_id));
}


void WallpaperImageSource::ImageAcquired(SkBitmap image,
    int request_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  CancelPendingEncodingOperation();
  scoped_refptr<base::RefCountedBytes> data = new base::RefCountedBytes();

  png_encoder_ = new SimplePngEncoder(
      data,
      image,
      base::Bind(&WallpaperImageSource::CancelCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 request_id));

  TRACE_EVENT0("LOCK_SCREEN", "imageEncoding");
  png_encoder_->Run(
      base::Bind(&WallpaperImageSource::SendCurrentUserWallpaper,
                 weak_ptr_factory_.GetWeakPtr(),
                 request_id));
};

void WallpaperImageSource::CancelPendingEncodingOperation() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  // Set canceled flag of previous request to skip unneeded encoding.
  if (png_encoder_.get()) {
    png_encoder_->Cancel();
  }

  // Cancel the callback for the previous request.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void WallpaperImageSource::CancelCallback(int request_id) {
    SendResponse(request_id, NULL);
    TRACE_EVENT_ASYNC_END0("SCREEN_LOCK", "GetUserWallpaper", request_id);
}

void WallpaperImageSource::SendCurrentUserWallpaper(int request_id,
    scoped_refptr<base::RefCountedBytes> data) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  SendResponse(request_id, data);
  TRACE_EVENT_ASYNC_END0("SCREEN_LOCK", "GetUserWallpaper", request_id);
}

}  // namespace options2
}  // namespace chromeos
