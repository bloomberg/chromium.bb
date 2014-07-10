// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/avatar/user_image_loader.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "components/user_manager/user_image/user_image.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/skbitmap_operations.h"

namespace chromeos {

UserImageLoader::ImageInfo::ImageInfo(const std::string& file_path,
                                      int pixels_per_side,
                                      const LoadedCallback& loaded_cb)
    : file_path(file_path),
      pixels_per_side(pixels_per_side),
      loaded_cb(loaded_cb) {
}

UserImageLoader::ImageInfo::~ImageInfo() {
}

UserImageLoader::UserImageLoader(
    ImageDecoder::ImageCodec image_codec,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : foreground_task_runner_(base::MessageLoopProxy::current()),
      background_task_runner_(background_task_runner),
      image_codec_(image_codec) {
}

UserImageLoader::~UserImageLoader() {
}

void UserImageLoader::Start(const std::string& filepath,
                            int pixels_per_side,
                            const LoadedCallback& loaded_cb) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UserImageLoader::ReadAndDecodeImage,
                 this,
                 ImageInfo(filepath, pixels_per_side, loaded_cb)));
}

void UserImageLoader::Start(scoped_ptr<std::string> data,
                            int pixels_per_side,
                            const LoadedCallback& loaded_cb) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UserImageLoader::DecodeImage,
                 this,
                 base::Passed(&data),
                 ImageInfo(std::string(), pixels_per_side, loaded_cb)));
}

void UserImageLoader::ReadAndDecodeImage(const ImageInfo& image_info) {
  DCHECK(background_task_runner_->RunsTasksOnCurrentThread());

  scoped_ptr<std::string> data(new std::string);
  if (!base::ReadFileToString(base::FilePath(image_info.file_path), data.get()))
    LOG(ERROR) << "Failed to read image " << image_info.file_path;

  // In case ReadFileToString() fails, |data| is empty and DecodeImage() calls
  // back to OnDecodeImageFailed().
  DecodeImage(data.Pass(), image_info);
}

void UserImageLoader::DecodeImage(const scoped_ptr<std::string> data,
                                  const ImageInfo& image_info) {
  DCHECK(background_task_runner_->RunsTasksOnCurrentThread());

  scoped_refptr<ImageDecoder> image_decoder =
      new ImageDecoder(this, *data, image_codec_);
  image_info_map_.insert(std::make_pair(image_decoder.get(), image_info));
  image_decoder->Start(background_task_runner_);
}

void UserImageLoader::OnImageDecoded(const ImageDecoder* decoder,
                                     const SkBitmap& decoded_image) {
  DCHECK(background_task_runner_->RunsTasksOnCurrentThread());

  ImageInfoMap::iterator it = image_info_map_.find(decoder);
  if (it == image_info_map_.end()) {
    NOTREACHED();
    return;
  }
  const std::string file_path = it->second.file_path;
  const int target_size = it->second.pixels_per_side;
  const LoadedCallback loaded_cb = it->second.loaded_cb;
  image_info_map_.erase(it);

  SkBitmap final_image = decoded_image;

  if (target_size > 0) {
    // Auto crop the image, taking the largest square in the center.
    int pixels_per_side =
        std::min(decoded_image.width(), decoded_image.height());
    int x = (decoded_image.width() - pixels_per_side) / 2;
    int y = (decoded_image.height() - pixels_per_side) / 2;
    SkBitmap cropped_image = SkBitmapOperations::CreateTiledBitmap(
        decoded_image, x, y, pixels_per_side, pixels_per_side);
    if (pixels_per_side > target_size) {
      // Also downsize the image to save space and memory.
      final_image =
          skia::ImageOperations::Resize(cropped_image,
                                        skia::ImageOperations::RESIZE_LANCZOS3,
                                        target_size,
                                        target_size);
    } else {
      final_image = cropped_image;
    }
  }
  // Make the SkBitmap immutable as we won't modify it. This is important
  // because otherwise it gets duplicated during painting, wasting memory.
  final_image.setImmutable();
  gfx::ImageSkia final_image_skia =
      gfx::ImageSkia::CreateFrom1xBitmap(final_image);
  final_image_skia.MakeThreadSafe();
  user_manager::UserImage user_image(final_image_skia,
                                     decoder->get_image_data());
  user_image.set_file_path(file_path);
  if (image_codec_ == ImageDecoder::ROBUST_JPEG_CODEC)
    user_image.MarkAsSafe();
  foreground_task_runner_->PostTask(FROM_HERE,
                                    base::Bind(loaded_cb, user_image));
}

void UserImageLoader::OnDecodeImageFailed(const ImageDecoder* decoder) {
  DCHECK(background_task_runner_->RunsTasksOnCurrentThread());

  ImageInfoMap::iterator it = image_info_map_.find(decoder);
  if (it == image_info_map_.end()) {
    NOTREACHED();
    return;
  }
  const LoadedCallback loaded_cb = it->second.loaded_cb;
  image_info_map_.erase(it);

  foreground_task_runner_->PostTask(
      FROM_HERE, base::Bind(loaded_cb, user_manager::UserImage()));
}

}  // namespace chromeos
