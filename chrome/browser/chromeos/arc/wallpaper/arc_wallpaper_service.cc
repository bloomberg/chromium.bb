// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/wallpaper/arc_wallpaper_service.h"

#include <stdlib.h>

#include <deque>

#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/image_decoder.h"
#include "components/arc/arc_bridge_service.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#include "components/wallpaper/wallpaper_files_id.h"
#include "components/wallpaper/wallpaper_layout.h"
#include "components/wallpaper/wallpaper_resizer.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_util.h"

using user_manager::UserManager;

namespace arc {
namespace {

constexpr char kAndroidWallpaperFilename[] = "android.jpg";

// The structure must not go out from a method invoked on BrowserThread::UI
// because the primary account referenced by the class will be released.
struct PrimaryAccount {
  const AccountId& id;
  const bool is_active;
};

PrimaryAccount GetPrimaryAccount() {
  UserManager* const user_manager = UserManager::Get();
  const AccountId& account_id = user_manager->GetPrimaryUser()->GetAccountId();
  return {account_id,
          account_id == user_manager->GetActiveUser()->GetAccountId()};
}

std::vector<uint8_t> EncodeImagePng(const gfx::ImageSkia image) {
  std::vector<uint8_t> result;
  gfx::PNGCodec::FastEncodeBGRASkBitmap(*image.bitmap(), true, &result);
  return result;
}

ash::WallpaperController* GetWallpaperController() {
  if (!ash::Shell::HasInstance())
    return nullptr;
  return ash::Shell::Get()->wallpaper_controller();
}

}  // namespace

// Store for mapping between ImageSkia ID and Android wallpaper ID.
// Skia image ID is used to keep track of the wallpaper on the Chrome side, and
// we need to map it to the Android's wallpaper ID in ARC++. Because setting
// wallpaper is async operation, the caller stores the pair of ImageSkia ID and
// wallpaper ID when it requests setting a wallpaper, then it obtains the
// corresponding Andorid ID when the completion of setting a wallpaper is
// notified.
class ArcWallpaperService::AndroidIdStore {
 public:
  AndroidIdStore() = default;

  // Remembers a pair of image and Android ID.
  void Push(const gfx::ImageSkia& image, int32_t android_id) {
    pairs_.push_back(
        {wallpaper::WallpaperResizer::GetImageId(image), android_id});
  }

  // Gets Android ID for |image_id|. Returns -1 if it does not found Android ID
  // corresponding to |image_id|. Sometimes the corresonpding |Pop| is not
  // called after |Push| is invoked, thus |Pop| removes older pairs in addition
  // to the pair of given |image_id| when the pair is found.
  int32_t Pop(uint32_t image_id) {
    for (size_t i = 0; i < pairs_.size(); ++i) {
      if (pairs_[i].image_id == image_id) {
        const int32_t result = pairs_[i].android_id;
        pairs_.erase(pairs_.begin(), pairs_.begin() + i + 1);
        return result;
      }
    }
    return -1;
  }

 private:
  struct Pair {
    uint32_t image_id;
    int32_t android_id;
  };
  std::deque<Pair> pairs_;

  DISALLOW_COPY_AND_ASSIGN(AndroidIdStore);
};

class ArcWallpaperService::DecodeRequest : public ImageDecoder::ImageRequest {
 public:
  DecodeRequest(ArcWallpaperService* service, int32_t android_id)
      : service_(service), android_id_(android_id) {}
  ~DecodeRequest() override = default;

  // ImageDecoder::ImageRequest overrides.
  void OnImageDecoded(const SkBitmap& bitmap) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // Make the SkBitmap immutable as we won't modify it. This is important
    // because otherwise it gets duplicated during painting, wasting memory.
    SkBitmap immutable_bitmap(bitmap);
    immutable_bitmap.setImmutable();
    gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(immutable_bitmap);
    image.MakeThreadSafe();
    service_->android_id_store()->Push(image, android_id_);

    chromeos::WallpaperManager* const wallpaper_manager =
        chromeos::WallpaperManager::Get();
    const PrimaryAccount& account = GetPrimaryAccount();
    wallpaper::WallpaperFilesId wallpaper_files_id =
        wallpaper_manager->GetFilesId(account.id);
    // TODO(crbug.com/618922): Allow specifying layout.
    wallpaper_manager->SetCustomWallpaper(
        account.id, wallpaper_files_id, kAndroidWallpaperFilename,
        wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED,
        user_manager::User::CUSTOMIZED, image,
        account.is_active /*update_wallpaper*/);

    // TODO(crbug.com/618922): Register the wallpaper to Chrome OS wallpaper
    // picker. Currently the new wallpaper does not appear there. The best way
    // to make this happen seems to do the same things as wallpaper_api.cc and
    // wallpaper_private_api.cc.
  }

  void OnDecodeImageFailed() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DLOG(ERROR) << "Failed to decode wallpaper image.";

    // Android regards the new wallpaper was set to Chrome. Invoke
    // OnWallpaperDataChanged to notify that the previous wallpaper is still
    // used in chrome. Note that |wallpaper_id| is not passed back to ARC in
    // this case.
    service_->OnWallpaperDataChanged();
  }

 private:
  // ArcWallpaperService owns DecodeRequest, so it will outlive this.
  ArcWallpaperService* const service_;
  const int32_t android_id_;

  DISALLOW_COPY_AND_ASSIGN(DecodeRequest);
};

ArcWallpaperService::ArcWallpaperService(ArcBridgeService* bridge_service)
    : ArcService(bridge_service),
      binding_(this),
      android_id_store_(new AndroidIdStore()) {
  arc_bridge_service()->wallpaper()->AddObserver(this);
}

ArcWallpaperService::~ArcWallpaperService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ash::WallpaperController* wc = GetWallpaperController();
  if (wc)
    wc->RemoveObserver(this);
  arc_bridge_service()->wallpaper()->RemoveObserver(this);
}

void ArcWallpaperService::OnInstanceReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojom::WallpaperInstance* wallpaper_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service()->wallpaper(), Init);
  DCHECK(wallpaper_instance);
  wallpaper_instance->Init(binding_.CreateInterfacePtrAndBind());
  ash::WallpaperController* wc = GetWallpaperController();
  DCHECK(wc);
  wc->AddObserver(this);
}

void ArcWallpaperService::OnInstanceClosed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ash::WallpaperController* wc = GetWallpaperController();
  if (wc)
    wc->RemoveObserver(this);
}

void ArcWallpaperService::SetWallpaper(const std::vector<uint8_t>& data,
                                       int32_t wallpaper_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (wallpaper_id == 0)
    wallpaper_id = -1;
  // Previous request will be cancelled at destructor of
  // ImageDecoder::ImageRequest.
  decode_request_ = base::MakeUnique<DecodeRequest>(this, wallpaper_id);
  ImageDecoder::StartWithOptions(decode_request_.get(), data,
                                 ImageDecoder::DEFAULT_CODEC, true,
                                 gfx::Size());
}

void ArcWallpaperService::SetDefaultWallpaper() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Previous request will be cancelled at destructor of
  // ImageDecoder::ImageRequest.
  decode_request_.reset();
  const PrimaryAccount& account = GetPrimaryAccount();
  chromeos::WallpaperManager::Get()->SetDefaultWallpaper(account.id,
                                                         account.is_active);
}

void ArcWallpaperService::GetWallpaper(const GetWallpaperCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ash::WallpaperController* wc = ash::Shell::Get()->wallpaper_controller();
  gfx::ImageSkia wallpaper = wc->GetWallpaper();
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, base::TaskTraits().MayBlock().WithPriority(
                     base::TaskPriority::BACKGROUND),
      base::Bind(&EncodeImagePng, wallpaper), callback);
}

void ArcWallpaperService::OnWallpaperDataChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* const wallpaper_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service()->wallpaper(), OnWallpaperChanged);
  const ash::WallpaperController* const wc = GetWallpaperController();
  const int32_t android_id =
      wc ? android_id_store_->Pop(wc->GetWallpaperOriginalImageId()) : -1;
  if (!wallpaper_instance)
    return;
  wallpaper_instance->OnWallpaperChanged(android_id);
}

}  // namespace arc
