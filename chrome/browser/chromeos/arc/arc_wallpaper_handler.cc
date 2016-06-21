// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_wallpaper_handler.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#include "components/wallpaper/wallpaper_files_id.h"
#include "components/wallpaper/wallpaper_layout.h"
#include "ui/gfx/image/image_skia.h"

namespace arc {

namespace {

constexpr char kAndroidWallpaperFilename[] = "android.jpg";

// Sets a decoded bitmap as the wallpaper.
void SetBitmapAsWallpaper(const SkBitmap& bitmap) {
  // Make the SkBitmap immutable as we won't modify it. This is important
  // because otherwise it gets duplicated during painting, wasting memory.
  SkBitmap immutable_bitmap(bitmap);
  immutable_bitmap.setImmutable();
  gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(immutable_bitmap);
  image.MakeThreadSafe();

  chromeos::WallpaperManager* wallpaper_manager =
      chromeos::WallpaperManager::Get();

  const AccountId& account_id =
      user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId();
  wallpaper::WallpaperFilesId wallpaper_files_id =
      wallpaper_manager->GetFilesId(account_id);
  bool update_wallpaper =
      account_id ==
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId();
  // TODO(crbug.com/618922): Allow specifying layout.
  wallpaper_manager->SetCustomWallpaper(
      account_id, wallpaper_files_id, kAndroidWallpaperFilename,
      wallpaper::WALLPAPER_LAYOUT_STRETCH, user_manager::User::CUSTOMIZED,
      image, update_wallpaper);

  // TODO(crbug.com/618922): Register the wallpaper to Chrome OS wallpaper
  // picker. Currently the new wallpaper does not appear there. The best way to
  // make this happen seems to do the same things as wallpaper_api.cc and
  // wallpaper_private_api.cc.
}

}  // namespace

// An implementation of ImageDecoder::ImageRequest that just calls back
// ArcWallpaperHandler.
class ArcWallpaperHandler::ImageRequestImpl
    : public ImageDecoder::ImageRequest {
 public:
  // |handler| outlives this instance.
  explicit ImageRequestImpl(ArcWallpaperHandler* handler) : handler_(handler) {}

  // ImageDecoder::ImageRequest implementation.
  void OnImageDecoded(const SkBitmap& bitmap) override {
    handler_->OnImageDecoded(this, bitmap);
  }
  void OnDecodeImageFailed() override { handler_->OnDecodeImageFailed(this); }

 private:
  ArcWallpaperHandler* handler_;

  DISALLOW_COPY_AND_ASSIGN(ImageRequestImpl);
};

ArcWallpaperHandler::ArcWallpaperHandler() = default;

ArcWallpaperHandler::~ArcWallpaperHandler() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Cancel in-flight requests.
  for (ImageRequestImpl* request : inflight_requests_) {
    ImageDecoder::Cancel(request);
    delete request;
  }
  inflight_requests_.clear();
}

void ArcWallpaperHandler::SetWallpaper(const std::vector<uint8_t>& jpeg_data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::unique_ptr<ImageRequestImpl> request =
      base::MakeUnique<ImageRequestImpl>(this);
  // TODO(nya): Improve ImageDecoder to minimize copy.
  std::string jpeg_data_as_string(
      reinterpret_cast<const char*>(jpeg_data.data()), jpeg_data.size());
  ImageDecoder::StartWithOptions(request.get(), jpeg_data_as_string,
                                 ImageDecoder::ROBUST_JPEG_CODEC, true);
  inflight_requests_.insert(request.release());
}

void ArcWallpaperHandler::OnImageDecoded(ImageRequestImpl* request,
                                         const SkBitmap& bitmap) {
  DCHECK(thread_checker_.CalledOnValidThread());
  inflight_requests_.erase(request);
  delete request;
  SetBitmapAsWallpaper(bitmap);
}

void ArcWallpaperHandler::OnDecodeImageFailed(ImageRequestImpl* request) {
  DCHECK(thread_checker_.CalledOnValidThread());
  inflight_requests_.erase(request);
  delete request;
  LOG(ERROR) << "Failed to decode wallpaper image.";
}

}  // namespace arc
