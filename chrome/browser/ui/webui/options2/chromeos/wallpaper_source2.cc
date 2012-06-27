// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options2/chromeos/wallpaper_source2.h"

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/shell.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "grit/ui_resources.h"
#include "net/base/mime_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skia_util.h"

extern "C" {
#if defined(USE_SYSTEM_ZLIB)
#include <zlib.h>
#else
#include "third_party/zlib/zlib.h"
#endif
}

namespace chromeos {
namespace options2 {

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
    SkAutoLockPixels lock_input(image_);
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
    : DataSource(chrome::kChromeUIWallpaperImageHost, NULL),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

WallpaperImageSource::~WallpaperImageSource() {
}

void WallpaperImageSource::StartDataRequest(const std::string& email,
                                            bool is_incognito,
                                            int request_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
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
    SendResponse(wallpaper_encoding_op_->request_id(), NULL);
  }

  // Cancel reply callback for previous request.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void WallpaperImageSource::SendCurrentUserWallpaper(int request_id,
    scoped_refptr<base::RefCountedBytes> data) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  SendResponse(request_id, data);
}

}  // namespace options2
}  // namespace chromeos
