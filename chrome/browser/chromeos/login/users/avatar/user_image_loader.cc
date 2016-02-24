// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/avatar/user_image_loader.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "components/user_manager/user_image/user_image.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/skbitmap_operations.h"

namespace chromeos {

// Contains attributes we need to know about each image we decode.
struct UserImageLoader::ImageInfo {
  ImageInfo(const base::FilePath& file_path,
            int pixels_per_side,
            ImageDecoder::ImageCodec image_codec,
            const LoadedCallback& loaded_cb)
      : file_path(file_path),
        pixels_per_side(pixels_per_side),
        image_codec(image_codec),
        loaded_cb(loaded_cb) {}
  ~ImageInfo() {}

  const base::FilePath file_path;
  const int pixels_per_side;
  const ImageDecoder::ImageCodec image_codec;
  const LoadedCallback loaded_cb;
};

// Handles the decoded image returned from ImageDecoder through the
// ImageRequest interface.
class UserImageLoader::UserImageRequest : public ImageDecoder::ImageRequest {
 public:
  UserImageRequest(const ImageInfo& image_info,
                   const std::string& image_data,
                   const scoped_refptr<UserImageLoader>& user_image_loader)
      : ImageRequest(user_image_loader->background_task_runner_),
        image_info_(image_info),
        image_data_(image_data.begin(), image_data.end()),
        user_image_loader_(user_image_loader) {}
  ~UserImageRequest() override {}

  // ImageDecoder::ImageRequest implementation. These callbacks will only be
  // invoked via user_image_loader_'s background_task_runner_.
  void OnImageDecoded(const SkBitmap& decoded_image) override;
  void OnDecodeImageFailed() override;

 private:
  const ImageInfo image_info_;
  std::vector<unsigned char> image_data_;
  scoped_refptr<UserImageLoader> user_image_loader_;
};

void UserImageLoader::UserImageRequest::OnImageDecoded(
    const SkBitmap& decoded_image) {
  DCHECK(task_runner()->RunsTasksOnCurrentThread());

  const int target_size = image_info_.pixels_per_side;
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
  user_manager::UserImage user_image(final_image_skia, image_data_);
  user_image.set_file_path(image_info_.file_path);
  if (image_info_.image_codec == ImageDecoder::ROBUST_JPEG_CODEC)
    user_image.MarkAsSafe();
  // TODO(satorux): Remove the foreground_task_runner_ stuff.
  user_image_loader_->foreground_task_runner_->PostTask(
      FROM_HERE, base::Bind(image_info_.loaded_cb, user_image));
  user_image_loader_->foreground_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(base::Bind(&base::DeletePointer<UserImageRequest>, this)));
}

void UserImageLoader::UserImageRequest::OnDecodeImageFailed() {
  DCHECK(task_runner()->RunsTasksOnCurrentThread());
  // TODO(satorux): Remove the foreground_task_runner_ stuff.
  user_image_loader_->foreground_task_runner_->PostTask(
      FROM_HERE, base::Bind(image_info_.loaded_cb, user_manager::UserImage()));
  user_image_loader_->foreground_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(base::Bind(&base::DeletePointer<UserImageRequest>, this)));
}

UserImageLoader::UserImageLoader(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : foreground_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      background_task_runner_(background_task_runner) {}

UserImageLoader::~UserImageLoader() {}

void UserImageLoader::StartWithFilePath(const base::FilePath& file_path,
                                        ImageDecoder::ImageCodec image_codec,
                                        int pixels_per_side,
                                        const LoadedCallback& loaded_cb) {
  std::string* data = new std::string;
  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::Bind(&base::ReadFileToString, file_path, data),
      base::Bind(&UserImageLoader::DecodeImage, this,
                 ImageInfo(file_path, pixels_per_side, image_codec, loaded_cb),
                 base::Owned(data)));
}

void UserImageLoader::StartWithData(scoped_ptr<std::string> data,
                                    ImageDecoder::ImageCodec image_codec,
                                    int pixels_per_side,
                                    const LoadedCallback& loaded_cb) {
  DecodeImage(
      ImageInfo(base::FilePath(), pixels_per_side, image_codec, loaded_cb),
      data.get(), true /* data_is_ready */);
}

void UserImageLoader::DecodeImage(const ImageInfo& image_info,
                                  const std::string* data,
                                  bool data_is_ready) {
  if (!data_is_ready) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(image_info.loaded_cb, user_manager::UserImage()));
    return;
  }

  UserImageRequest* image_request =
      new UserImageRequest(image_info, *data, this);
  ImageDecoder::StartWithOptions(image_request, *data, image_info.image_codec,
                                 false);
}

}  // namespace chromeos
