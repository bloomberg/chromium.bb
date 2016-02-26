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
namespace user_image_loader {
namespace {

// Contains attributes we need to know about each image we decode.
struct ImageInfo {
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
class UserImageRequest : public ImageDecoder::ImageRequest {
 public:
  UserImageRequest(
      const ImageInfo& image_info,
      const std::string& image_data,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner)
      : ImageRequest(background_task_runner),
        image_info_(image_info),
        image_data_(image_data.begin(), image_data.end()),
        foreground_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}
  ~UserImageRequest() override {}

  // ImageDecoder::ImageRequest implementation. These callbacks will only be
  // invoked via background_task_runner passed to the constructor.
  void OnImageDecoded(const SkBitmap& decoded_image) override;
  void OnDecodeImageFailed() override;

 private:
  const ImageInfo image_info_;
  std::vector<unsigned char> image_data_;
  scoped_refptr<base::SequencedTaskRunner> foreground_task_runner_;
};

void UserImageRequest::OnImageDecoded(const SkBitmap& decoded_image) {
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
  foreground_task_runner_->PostTask(
      FROM_HERE, base::Bind(image_info_.loaded_cb, user_image));
  foreground_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(base::Bind(&base::DeletePointer<UserImageRequest>, this)));
}

void UserImageRequest::OnDecodeImageFailed() {
  DCHECK(task_runner()->RunsTasksOnCurrentThread());
  // TODO(satorux): Remove the foreground_task_runner_ stuff.
  foreground_task_runner_->PostTask(
      FROM_HERE, base::Bind(image_info_.loaded_cb, user_manager::UserImage()));
  foreground_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(base::Bind(&base::DeletePointer<UserImageRequest>, this)));
}

// Starts decoding the image with ImageDecoder for the image |data| if
// |data_is_ready| is true.
void DecodeImage(
    const ImageInfo& image_info,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const std::string* data,
    bool data_is_ready) {
  if (!data_is_ready) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(image_info.loaded_cb, user_manager::UserImage()));
    return;
  }

  UserImageRequest* image_request =
      new UserImageRequest(image_info, *data, background_task_runner);
  ImageDecoder::StartWithOptions(image_request, *data, image_info.image_codec,
                                 false);
}

}  // namespace

void StartWithFilePath(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const base::FilePath& file_path,
    ImageDecoder::ImageCodec image_codec,
    int pixels_per_side,
    const LoadedCallback& loaded_cb) {
  std::string* data = new std::string;
  base::PostTaskAndReplyWithResult(
      background_task_runner.get(), FROM_HERE,
      base::Bind(&base::ReadFileToString, file_path, data),
      base::Bind(&DecodeImage,
                 ImageInfo(file_path, pixels_per_side, image_codec, loaded_cb),
                 background_task_runner, base::Owned(data)));
}

void StartWithData(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    scoped_ptr<std::string> data,
    ImageDecoder::ImageCodec image_codec,
    int pixels_per_side,
    const LoadedCallback& loaded_cb) {
  DecodeImage(
      ImageInfo(base::FilePath(), pixels_per_side, image_codec, loaded_cb),
      background_task_runner, data.get(), true /* data_is_ready */);
}

}  // namespace user_image_loader
}  // namespace chromeos
