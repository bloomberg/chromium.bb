// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/wallpaper_source.h"

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/shell.h"
#include "base/debug/trace_event.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "grit/ui_resources.h"
#include "net/base/mime_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skia_util.h"

extern "C" {
#include "third_party/zlib/zlib.h"
}

namespace chromeos {
namespace options {

// Operation class that encodes existing in-memory image as PNG.
// It uses NO-COMPRESSION to save time.
class WallpaperImageSource::WallpaperEncodingOperation
    : public base::RefCountedThreadSafe<
          WallpaperImageSource::WallpaperEncodingOperation> {
 public:
  WallpaperEncodingOperation(
      int request_id,
      scoped_refptr<base::RefCountedBytes> data,
      SkBitmap image)
      : request_id_(request_id),
        data_(data),
        image_(image) {
  }

  static void Run(scoped_refptr<WallpaperEncodingOperation> weo) {
    weo->EncodeWallpaper();
  }

  int request_id() {
    return request_id_;
  }

  void EncodeWallpaper() {
    if (cancel_flag_.IsSet())
      return;
    TRACE_EVENT0("LOCK_SCREEN", "imageEncoding");
    SkAutoLockPixels lock_input(image_);

    if (!image_.readyToDraw())
      return;

    // Avoid compression to make things faster.
    gfx::PNGCodec::EncodeWithCompressionLevel(
        reinterpret_cast<unsigned char*>(image_.getAddr32(0, 0)),
        gfx::PNGCodec::FORMAT_SkBitmap,
        gfx::Size(image_.width(), image_.height()),
        image_.width() * image_.bytesPerPixel(),
        false,
        std::vector<gfx::PNGCodec::Comment>(),
        Z_NO_COMPRESSION,
        &data_->data());
    if (cancel_flag_.IsSet())
      return;
  }

  void Cancel() {
    cancel_flag_.Set();
  }

 private:
  friend class base::RefCountedThreadSafe<
      WallpaperImageSource::WallpaperEncodingOperation>;

  ~WallpaperEncodingOperation() {}

  base::CancellationFlag cancel_flag_;

  // ID of original request.
  int request_id_;
  // Buffer to store encoded image.
  scoped_refptr<base::RefCountedBytes> data_;
  // Original image to encode.
  SkBitmap image_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperEncodingOperation);
};

WallpaperImageSource::WallpaperImageSource()
    : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

WallpaperImageSource::~WallpaperImageSource() {
}

std::string WallpaperImageSource::GetSource() {
  return chrome::kChromeUIWallpaperImageHost;
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
      base::Bind(&WallpaperImageSource::GetCurrentUserWallpaper,
                 weak_ptr_factory_.GetWeakPtr(), request_id));
}

std::string WallpaperImageSource::GetMimeType(const std::string&) const {
  return "images/png";
}

// Get current background image and store it to |data|.
void WallpaperImageSource::GetCurrentUserWallpaper(
    const base::WeakPtr<WallpaperImageSource>& this_object, int request_id) {
  SkBitmap image;
  TRACE_EVENT0("LOCK_SCREEN", "GetCurrentUserWallpaper");
  if (chromeos::UserManager::Get()->IsUserLoggedIn()) {
    // TODO(sad|bshe): It maybe necessary to include the scale factor in the
    // request (as is done for user-image and wallpaper-thumbnails).
    SkBitmap wallpaper;
    gfx::ImageSkia wallpaper_skia = ash::Shell::GetInstance()->
        desktop_background_controller()->GetCurrentWallpaperImage();
    if (!wallpaper_skia.isNull())
      wallpaper = *wallpaper_skia.bitmap();
    SkBitmap copy;
    if (wallpaper.deepCopyTo(&copy, wallpaper.config()))
      image = copy;
  }
  content::BrowserThread::PostTask(
    content::BrowserThread::IO,
    FROM_HERE,
    base::Bind(&WallpaperImageSource::ImageAcquired, this_object,
               image, request_id));
}


void WallpaperImageSource::ImageAcquired(SkBitmap image, int request_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  CancelPendingEncodingOperation();
  scoped_refptr<base::RefCountedBytes> data = new base::RefCountedBytes();
  wallpaper_encoding_op_ = new WallpaperEncodingOperation(request_id,
                                                          data,
                                                          image);
  base::WorkerPool::PostTaskAndReply(
      FROM_HERE,
      base::Bind(&WallpaperEncodingOperation::Run, wallpaper_encoding_op_),
      base::Bind(&WallpaperImageSource::SendCurrentUserWallpaper,
                 weak_ptr_factory_.GetWeakPtr(), request_id, data),
      true /* task_is_slow */);
};

void WallpaperImageSource::CancelPendingEncodingOperation() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  // Set canceled flag of previous request to skip unneeded encoding.
  if (wallpaper_encoding_op_.get()) {
    wallpaper_encoding_op_->Cancel();
    url_data_source()->SendResponse(wallpaper_encoding_op_->request_id(), NULL);
    TRACE_EVENT_ASYNC_END0("SCREEN_LOCK", "GetUserWallpaper",
                           wallpaper_encoding_op_->request_id());
  }

  // Cancel reply callback for previous request.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void WallpaperImageSource::SendCurrentUserWallpaper(int request_id,
    scoped_refptr<base::RefCountedBytes> data) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  url_data_source()->SendResponse(request_id, data);
  TRACE_EVENT_ASYNC_END0("SCREEN_LOCK", "GetUserWallpaper", request_id);
}

}  // namespace options
}  // namespace chromeos
