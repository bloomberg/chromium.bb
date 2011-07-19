// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_image_loader.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/login/image_decoder.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "content/browser/browser_thread.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/skbitmap_operations.h"

namespace chromeos {

UserImageLoader::UserImageLoader(Delegate* delegate)
    : target_message_loop_(NULL),
      delegate_(delegate) {
}

UserImageLoader::~UserImageLoader() {
}

void UserImageLoader::Start(const std::string& username,
                            const std::string& filename,
                            bool should_save_image) {
  target_message_loop_ = MessageLoop::current();

  ImageInfo image_info(username, should_save_image);
  BrowserThread::PostTask(BrowserThread::FILE,
                          FROM_HERE,
                          NewRunnableMethod(this,
                                            &UserImageLoader::LoadImage,
                                            filename,
                                            image_info));
}

void UserImageLoader::LoadImage(const std::string& filepath,
                                const ImageInfo& image_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::string image_data;
  file_util::ReadFileToString(FilePath(filepath), &image_data);

  scoped_refptr<ImageDecoder> image_decoder =
      new ImageDecoder(this, image_data);
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
  ImageInfo image_info = info_it->second;
  SkBitmap final_image = decoded_image;
  if (image_info.should_save_image) {
    // Auto crop the image, taking the largest square in the center.
    // Also make the image smaller to save space and memory.
    int size = std::min(decoded_image.width(), decoded_image.height());
    int x = (decoded_image.width() - size) / 2;
    int y = (decoded_image.height() - size) / 2;
    SkBitmap cropped_image =
        SkBitmapOperations::CreateTiledBitmap(decoded_image, x, y, size, size);
    final_image =
        skia::ImageOperations::Resize(cropped_image,
                                      skia::ImageOperations::RESIZE_LANCZOS3,
                                      login::kUserImageSize,
                                      login::kUserImageSize);
  }
  target_message_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this,
                        &UserImageLoader::NotifyDelegate,
                        final_image,
                        image_info));
  image_info_map_.erase(info_it);
}

void UserImageLoader::OnDecodeImageFailed(const ImageDecoder* decoder) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  image_info_map_.erase(decoder);
}

void UserImageLoader::NotifyDelegate(const SkBitmap& image,
                                     const ImageInfo& image_info) {
  if (delegate_) {
    delegate_->OnImageLoaded(image_info.username,
                             image,
                             image_info.should_save_image);
  }
}

}  // namespace chromeos
