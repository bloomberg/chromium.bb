// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wallpaper_api.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wallpaper_manager.h"
#include "chrome/common/chrome_paths.h"

using base::BinaryValue;
using content::BrowserThread;

WallpaperSetWallpaperFunction::WallpaperSetWallpaperFunction() {
}

WallpaperSetWallpaperFunction::~WallpaperSetWallpaperFunction() {
}

bool WallpaperSetWallpaperFunction::RunImpl() {
  DictionaryValue* details = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &details));

  BinaryValue* input = NULL;
  details->GetBinary("wallpaperData", &input);

  std::string layout_string;
  details->GetString("layout", &layout_string);
  EXTENSION_FUNCTION_VALIDATE(!layout_string.empty());
  layout_ = wallpaper_api_util::GetLayoutEnum(layout_string);

  details->GetBoolean("thumbnail", &generate_thumbnail_);
  details->GetString("name", &file_name_);

  EXTENSION_FUNCTION_VALIDATE(!file_name_.empty());

  // Gets email address while at UI thread.
  email_ = chromeos::UserManager::Get()->GetLoggedInUser()->email();

  image_data_.assign(input->GetBuffer(), input->GetSize());
  StartDecode(image_data_);

  return true;
}

void WallpaperSetWallpaperFunction::OnWallpaperDecoded(
    const gfx::ImageSkia& wallpaper) {
  chromeos::WallpaperManager* wallpaper_manager =
      chromeos::WallpaperManager::Get();
  chromeos::UserImage::RawImage raw_image(image_data_.begin(),
                                          image_data_.end());
  chromeos::UserImage image(wallpaper, raw_image);
  base::FilePath thumbnail_path = wallpaper_manager->GetCustomWallpaperPath(
      chromeos::kThumbnailWallpaperSubDir, email_, file_name_);

  sequence_token_ = BrowserThread::GetBlockingPool()->
      GetNamedSequenceToken(chromeos::kWallpaperSequenceTokenName);
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      BrowserThread::GetBlockingPool()->
          GetSequencedTaskRunnerWithShutdownBehavior(sequence_token_,
              base::SequencedWorkerPool::BLOCK_SHUTDOWN);

  // In the new wallpaper picker UI, we do not depend on WallpaperDelegate
  // to refresh thumbnail. Uses a null delegate here.
  wallpaper_manager->SetCustomWallpaper(email_, file_name_, layout_,
                                        chromeos::User::CUSTOMIZED,
                                        image);
  unsafe_wallpaper_decoder_ = NULL;

  if (generate_thumbnail_) {
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
