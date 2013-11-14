// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_image_loader.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/user_image.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/skbitmap_operations.h"

namespace chromeos {

UserImageLoader::ImageInfo::ImageInfo(int size,
                                      const LoadedCallback& loaded_cb)
    : size(size),
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
                            int size,
                            const LoadedCallback& loaded_cb) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UserImageLoader::ReadAndDecodeImage,
                 this,
                 filepath,
                 ImageInfo(size, loaded_cb)));
}

void UserImageLoader::Start(scoped_ptr<std::string> data,
                            int size,
                            const LoadedCallback& loaded_cb) {
  background_task_runner_->PostTask(FROM_HERE,
                                    base::Bind(&UserImageLoader::DecodeImage,
                                        this,
                                        base::Passed(&data),
                                        ImageInfo(size, loaded_cb)));
}

void UserImageLoader::ReadAndDecodeImage(const std::string& filepath,
                                         const ImageInfo& image_info) {
  DCHECK(background_task_runner_->RunsTasksOnCurrentThread());

  scoped_ptr<std::string> data(new std::string);
  base::ReadFileToString(base::FilePath(filepath), data.get());

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
  const int target_size = it->second.size;
  const LoadedCallback loaded_cb = it->second.loaded_cb;
  image_info_map_.erase(it);

  SkBitmap final_image = decoded_image;

  if (target_size > 0) {
    // Auto crop the image, taking the largest square in the center.
    int size = std::min(decoded_image.width(), decoded_image.height());
    int x = (decoded_image.width() - size) / 2;
    int y = (decoded_image.height() - size) / 2;
    SkBitmap cropped_image =
        SkBitmapOperations::CreateTiledBitmap(decoded_image, x, y, size, size);
    if (size > target_size) {
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
  UserImage user_image(final_image_skia, decoder->get_image_data());
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

  foreground_task_runner_->PostTask(FROM_HERE,
                                    base::Bind(loaded_cb, UserImage()));
}

}  // namespace chromeos
