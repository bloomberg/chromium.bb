// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_decoder.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/sequenced_task_runner.h"
#include "components/user_manager/user_image/user_image.h"
#include "ipc/ipc_channel.h"
#include "services/data_decoder/public/cpp/decode_image.h"

namespace ash {
namespace {

const int64_t kMaxImageSizeInBytes =
    static_cast<int64_t>(IPC::Channel::kMaximumMessageSize);

// TODO(crbug.com/776464): This should be changed to |ConvertToImageSkia| after
// the use of |UserImage| in |WallpaperManager| is deprecated.
void ConvertToUserImage(OnWallpaperDecoded callback, const SkBitmap& image) {
  SkBitmap final_image = image;
  final_image.setImmutable();
  gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(final_image);
  image_skia.MakeThreadSafe();
  auto user_image = std::make_unique<user_manager::UserImage>(
      image_skia, new base::RefCountedBytes() /* unused */,
      user_manager::UserImage::FORMAT_JPEG /* unused */);

  std::move(callback).Run(std::move(user_image));
}

}  // namespace

void DecodeWallpaper(std::unique_ptr<std::string> image_data,
                     OnWallpaperDecoded callback) {
  std::vector<uint8_t> image_bytes(image_data.get()->begin(),
                                   image_data.get()->end());
  data_decoder::DecodeImage(
      Shell::Get()->shell_delegate()->GetShellConnector(),
      std::move(image_bytes), data_decoder::mojom::ImageCodec::ROBUST_JPEG,
      false /* shrink_to_fit */, kMaxImageSizeInBytes,
      gfx::Size() /* desired_image_frame_size */,
      base::BindOnce(&ConvertToUserImage, std::move(callback)));
}

}  // namespace ash