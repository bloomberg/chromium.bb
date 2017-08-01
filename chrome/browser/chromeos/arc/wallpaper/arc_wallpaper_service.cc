// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/wallpaper/arc_wallpaper_service.h"

#include <stdlib.h>

#include <deque>
#include <utility>

#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/image_decoder.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
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

// Singleton factory for ArcWallpaperService.
class ArcWallpaperServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcWallpaperService,
          ArcWallpaperServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcWallpaperServiceFactory";

  static ArcWallpaperServiceFactory* GetInstance() {
    return base::Singleton<ArcWallpaperServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcWallpaperServiceFactory>;
  ArcWallpaperServiceFactory() = default;
  ~ArcWallpaperServiceFactory() override = default;
};

}  // namespace

struct ArcWallpaperService::WallpaperIdPair {
  // ID of wallpaper image which can be obtaind by
  // WallpaperResizer::GetImageId().
  uint32_t image_id;
  // ID of wallpaper generated in the container side.
  int32_t android_id;
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

    WallpaperIdPair pair;
    pair.image_id = wallpaper::WallpaperResizer::GetImageId(image);
    pair.android_id = android_id_;
    DCHECK_NE(pair.image_id, 0u)
        << "image_id should not be 0 as we succeeded to decode image here.";

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
    // When kiosk app is running, or wallpaper cannot be changed due to policy,
    // or we are running child profile, WallpaperManager don't submit wallpaper
    // change requests.
    if (wallpaper_manager->IsPendingWallpaper(pair.image_id))
      service_->id_pairs_.push_back(pair);
    else
      service_->NotifyWallpaperChangedAndReset(android_id_);

    // TODO(crbug.com/618922): Register the wallpaper to Chrome OS wallpaper
    // picker. Currently the new wallpaper does not appear there. The best way
    // to make this happen seems to do the same things as wallpaper_api.cc and
    // wallpaper_private_api.cc.
  }

  void OnDecodeImageFailed() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DLOG(ERROR) << "Failed to decode wallpaper image.";
    service_->NotifyWallpaperChangedAndReset(android_id_);
  }

 private:
  // ArcWallpaperService owns DecodeRequest, so it will outlive this.
  ArcWallpaperService* const service_;
  const int32_t android_id_;

  DISALLOW_COPY_AND_ASSIGN(DecodeRequest);
};

// static
ArcWallpaperService* ArcWallpaperService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcWallpaperServiceFactory::GetForBrowserContext(context);
}

ArcWallpaperService::ArcWallpaperService(content::BrowserContext* context,
                                         ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service), binding_(this) {
  arc_bridge_service_->wallpaper()->AddObserver(this);
}

ArcWallpaperService::~ArcWallpaperService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ash::WallpaperController* wc = GetWallpaperController();
  if (wc)
    wc->RemoveObserver(this);

  arc_bridge_service_->wallpaper()->RemoveObserver(this);
}

void ArcWallpaperService::OnInstanceReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojom::WallpaperInstance* wallpaper_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->wallpaper(), Init);
  DCHECK(wallpaper_instance);
  mojom::WallpaperHostPtr host_proxy;
  binding_.Bind(mojo::MakeRequest(&host_proxy));
  wallpaper_instance->Init(std::move(host_proxy));
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
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
      base::Bind(&EncodeImagePng, wallpaper), callback);
}

void ArcWallpaperService::OnWallpaperDataChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // OnWallpaperDataChanged is invoked from WallpaperController so
  // we should be able to get the pointer.
  ash::WallpaperController* const wallpaper_controller =
      ash::Shell::Get()->wallpaper_controller();
  CHECK(wallpaper_controller);
  const uint32_t current_image_id =
      wallpaper_controller->GetWallpaperOriginalImageId();

  chromeos::WallpaperManager* const wallpaper_manager =
      chromeos::WallpaperManager::Get();
  bool current_wallppaer_notified = false;
  for (auto it = id_pairs_.begin(); it != id_pairs_.end();) {
    int32_t const android_id = it->android_id;
    bool should_notify = false;
    if (it->image_id == current_image_id) {
      should_notify = true;
      current_wallppaer_notified = true;
      it = id_pairs_.erase(it);
    } else if (!wallpaper_manager->IsPendingWallpaper(it->image_id)) {
      should_notify = true;
      it = id_pairs_.erase(it);
    } else {
      ++it;
    }

    if (should_notify)
      NotifyWallpaperChanged(android_id);
  }

  if (!current_wallppaer_notified)
    NotifyWallpaperChanged(-1);
}

void ArcWallpaperService::NotifyWallpaperChanged(int android_id) {
  mojom::WallpaperInstance* const wallpaper_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->wallpaper(),
                                  OnWallpaperChanged);
  if (wallpaper_instance == nullptr)
    return;
  wallpaper_instance->OnWallpaperChanged(android_id);
}

void ArcWallpaperService::NotifyWallpaperChangedAndReset(int32_t android_id) {
  // Invoke NotifyWallpaperChanged so that setWallpaper completes in Android
  // side.
  NotifyWallpaperChanged(android_id);
  // Invoke NotifyWallpaperChanged with -1 so that Android side regards the
  // wallpaper of |android_id_| is no longer used at Chrome side.
  NotifyWallpaperChanged(-1);
}

}  // namespace arc
