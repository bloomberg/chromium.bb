// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wallpaper_api.h"

#include "ash/desktop_background/desktop_background_controller.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wallpaper_manager.h"
#include "chrome/common/chrome_paths.h"

using base::BinaryValue;
using content::BrowserThread;

namespace set_wallpaper = extensions::api::wallpaper::SetWallpaper;

WallpaperSetWallpaperFunction::WallpaperSetWallpaperFunction() {
}

WallpaperSetWallpaperFunction::~WallpaperSetWallpaperFunction() {
}

bool WallpaperSetWallpaperFunction::RunImpl() {
  params = set_wallpaper::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  // Gets email address and username hash while at UI thread.
  email_ = chromeos::UserManager::Get()->GetLoggedInUser()->email();
  user_id_hash_ =
      chromeos::UserManager::Get()->GetLoggedInUser()->username_hash();

  StartDecode(*(params->details.wallpaper_data));

  return true;
}

void WallpaperSetWallpaperFunction::OnWallpaperDecoded(
    const gfx::ImageSkia& wallpaper) {
  chromeos::WallpaperManager* wallpaper_manager =
      chromeos::WallpaperManager::Get();
  chromeos::UserImage::RawImage raw_image(
      params->details.wallpaper_data->begin(),
      params->details.wallpaper_data->end());
  chromeos::UserImage image(wallpaper, raw_image);
  base::FilePath thumbnail_path = wallpaper_manager->GetCustomWallpaperPath(
      chromeos::kThumbnailWallpaperSubDir, user_id_hash_, params->details.name);

  sequence_token_ = BrowserThread::GetBlockingPool()->
      GetNamedSequenceToken(chromeos::kWallpaperSequenceTokenName);
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      BrowserThread::GetBlockingPool()->
          GetSequencedTaskRunnerWithShutdownBehavior(sequence_token_,
              base::SequencedWorkerPool::BLOCK_SHUTDOWN);
  ash::WallpaperLayout layout = wallpaper_api_util::GetLayoutEnum(
      set_wallpaper::Params::Details::ToString(params->details.layout));
  wallpaper_manager->SetCustomWallpaper(email_,
                                        user_id_hash_,
                                        params->details.name,
                                        layout,
                                        chromeos::User::CUSTOMIZED,
                                        image);
  unsafe_wallpaper_decoder_ = NULL;

  if (params->details.thumbnail) {
    wallpaper.EnsureRepsForSupportedScales();
    scoped_ptr<gfx::ImageSkia> deep_copy(wallpaper.DeepCopy());
    // Generates thumbnail before call api function callback. We can then
    // request thumbnail in the javascript callback.
    task_runner->PostTask(FROM_HERE,
        base::Bind(
            &WallpaperSetWallpaperFunction::GenerateThumbnail,
            this, thumbnail_path, base::Passed(&deep_copy)));
  } else {
    SendResponse(true);
  }
}

void WallpaperSetWallpaperFunction::GenerateThumbnail(
    const base::FilePath& thumbnail_path, scoped_ptr<gfx::ImageSkia> image) {
  DCHECK(BrowserThread::GetBlockingPool()->IsRunningSequenceOnCurrentThread(
      sequence_token_));
  chromeos::UserImage wallpaper(*image.get());
  if (!base::PathExists(thumbnail_path.DirName()))
    file_util::CreateDirectory(thumbnail_path.DirName());

  scoped_refptr<base::RefCountedBytes> data;
  chromeos::WallpaperManager::Get()->ResizeWallpaper(
      wallpaper,
      ash::WALLPAPER_LAYOUT_STRETCH,
      ash::kWallpaperThumbnailWidth,
      ash::kWallpaperThumbnailHeight,
      &data);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &WallpaperSetWallpaperFunction::ThumbnailGenerated,
          this, data));
}

void WallpaperSetWallpaperFunction::ThumbnailGenerated(
    base::RefCountedBytes* data) {
  BinaryValue* result = BinaryValue::CreateWithCopiedBuffer(
      reinterpret_cast<const char*>(data->front()), data->size());
  SetResult(result);
  SendResponse(true);
}
