// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_image_loader.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/user_image.h"
#include "content/public/browser/browser_thread.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/skbitmap_operations.h"

using content::BrowserThread;

namespace chromeos {

UserImageLoader::ImageInfo::ImageInfo(int size,
                                      const LoadedCallback& loaded_cb)
    : size(size),
      loaded_cb(loaded_cb) {
}

UserImageLoader::ImageInfo::~ImageInfo() {
}

UserImageLoader::UserImageLoader(ImageDecoder::ImageCodec image_codec)
    : target_message_loop_(NULL),
      image_codec_(image_codec) {
}

UserImageLoader::~UserImageLoader() {
}

void UserImageLoader::Start(const std::string& filepath,
                            int size,
                            const LoadedCallback& loaded_cb) {
  target_message_loop_ = MessageLoop::current();

  ImageInfo image_info(size, loaded_cb);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&UserImageLoader::LoadImage, this, filepath, image_info));
}

void UserImageLoader::LoadImage(const std::string& filepath,
                                const ImageInfo& image_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::string image_data;
  file_util::ReadFileToString(FilePath(filepath), &image_data);

  scoped_refptr<ImageDecoder> image_decoder =
      new ImageDecoder(this, image_data, image_codec_);
  image_info_map_.insert(std::make_pair(image_decoder.get(), image_info));
  image_decoder->Start();
}

void UserImageLoader::OnImageDecoded(const ImageDecoder* decoder,
                                     const SkBitmap& decoded_image) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  ImageInfoMap::iterator info_it = image_info_map_.find(decoder);
  if (info_it == image_info_map_.end()) {
    NOTREACHED();
    return;
  }

  const ImageInfo& image_info = info_it->second;
  SkBitmap final_image = decoded_image;

  if (image_info.size > 0) {
    // Auto crop the image, taking the largest square in the center.
    int size = std::min(decoded_image.width(), decoded_image.height());
    int x = (decoded_image.width() - size) / 2;
    int y = (decoded_image.height() - size) / 2;
    SkBitmap cropped_image =
        SkBitmapOperations::CreateTiledBitmap(decoded_image, x, y, size, size);
    if (size > image_info.size) {
      // Also downsize the image to save space and memory.
      final_image =
          skia::ImageOperations::Resize(cropped_image,
                                        skia::ImageOperations::RESIZE_LANCZOS3,
                                        image_info.size,
                                        image_info.size);
    } else {
      final_image = cropped_image;
    }
  }
  gfx::ImageSkia final_image_skia =
      gfx::ImageSkia::CreateFrom1xBitmap(final_image);
  final_image_skia.MakeThreadSafe();
  UserImage user_image(final_image_skia, decoder->get_image_data());
  if (image_codec_ == ImageDecoder::ROBUST_JPEG_CODEC)
    user_image.MarkAsSafe();
  target_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(image_info.loaded_cb, user_image));

  image_info_map_.erase(info_it);
}

void UserImageLoader::OnDecodeImageFailed(const ImageDecoder* decoder) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  ImageInfoMap::iterator info_it = image_info_map_.find(decoder);
  if (info_it == image_info_map_.end()) {
    NOTREACHED();
    return;
  }

  const ImageInfo& image_info = info_it->second;
  target_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(image_info.loaded_cb, UserImage()));
  image_info_map_.erase(decoder);
}

}  // namespace chromeos
