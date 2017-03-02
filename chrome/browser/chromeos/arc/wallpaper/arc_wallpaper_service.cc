// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/wallpaper/arc_wallpaper_service.h"

#include "ash/common/wallpaper/wallpaper_controller.h"
#include "ash/common/wm_shell.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "components/arc/arc_bridge_service.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#include "components/wallpaper/wallpaper_files_id.h"
#include "components/wallpaper/wallpaper_layout.h"
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
      UserManager::Get()->GetPrimaryUser()->GetAccountId();
  wallpaper::WallpaperFilesId wallpaper_files_id =
      wallpaper_manager->GetFilesId(account_id);
  const bool update_wallpaper =
      account_id == UserManager::Get()->GetActiveUser()->GetAccountId();
  // TODO(crbug.com/618922): Allow specifying layout.
  wallpaper_manager->SetCustomWallpaper(
      account_id, wallpaper_files_id, kAndroidWallpaperFilename,
      wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED,
      user_manager::User::CUSTOMIZED, image, update_wallpaper);

  // TODO(crbug.com/618922): Register the wallpaper to Chrome OS wallpaper
  // picker. Currently the new wallpaper does not appear there. The best way to
  // make this happen seems to do the same things as wallpaper_api.cc and
  // wallpaper_private_api.cc.
}

std::vector<uint8_t> EncodeImagePng(const gfx::ImageSkia image) {
  std::vector<uint8_t> result;
  gfx::PNGCodec::FastEncodeBGRASkBitmap(*image.bitmap(), true, &result);
  return result;
}

ash::WallpaperController* GetWallpaperController() {
  ash::WmShell* wm_shell = ash::WmShell::Get();
  if (!wm_shell)
    return nullptr;
  return wm_shell->wallpaper_controller();
}

}  // namespace

ArcWallpaperService::ArcWallpaperService(ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this) {
  arc_bridge_service()->wallpaper()->AddObserver(this);
}

ArcWallpaperService::~ArcWallpaperService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Make sure the callback is never called after destruction. It is safe to
  // call Cancel() even when there is no in-flight request.
  ImageDecoder::Cancel(this);
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
  ash::WmShell::Get()->wallpaper_controller()->AddObserver(this);
}

void ArcWallpaperService::OnInstanceClosed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ash::WallpaperController* wc = GetWallpaperController();
  if (wc)
    wc->RemoveObserver(this);
}

void ArcWallpaperService::SetWallpaper(const std::vector<uint8_t>& data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ImageDecoder::Cancel(this);
  ImageDecoder::StartWithOptions(this, data, ImageDecoder::DEFAULT_CODEC, true,
                                 gfx::Size());
}

void ArcWallpaperService::GetWallpaper(const GetWallpaperCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ash::WallpaperController* wc = ash::WmShell::Get()->wallpaper_controller();
  gfx::ImageSkia wallpaper = wc->GetWallpaper();
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, base::TaskTraits().MayBlock().WithPriority(
                     base::TaskPriority::BACKGROUND),
      base::Bind(&EncodeImagePng, wallpaper), callback);
}

void ArcWallpaperService::OnImageDecoded(const SkBitmap& bitmap) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SetBitmapAsWallpaper(bitmap);
}

void ArcWallpaperService::OnDecodeImageFailed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  LOG(ERROR) << "Failed to decode wallpaper image.";
}

void ArcWallpaperService::OnWallpaperDataChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* wallpaper_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service()->wallpaper(), OnWallpaperChanged);
  if (!wallpaper_instance)
    return;
  wallpaper_instance->OnWallpaperChanged();
}

}  // namespace arc
