// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_image_loader.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "content/browser/browser_thread.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

namespace chromeos {

UserImageLoader::UserImageLoader(Delegate* delegate)
    : target_message_loop_(NULL),
      delegate_(delegate) {
}

UserImageLoader::~UserImageLoader() {
}

void UserImageLoader::Start(const std::string& username,
                            const std::string& filename) {
  target_message_loop_ = MessageLoop::current();

  BrowserThread::PostTask(BrowserThread::FILE,
                          FROM_HERE,
                          NewRunnableMethod(this,
                                            &UserImageLoader::LoadImage,
                                            username,
                                            filename));
}

void UserImageLoader::LoadImage(const std::string& username,
                                const std::string& filepath) {
  std::string image_data;
  file_util::ReadFileToString(FilePath(filepath), &image_data);
  SkBitmap image;
  if (!gfx::PNGCodec::Decode(
          reinterpret_cast<const unsigned char*>(image_data.data()),
          image_data.size(),
          &image))
    return;
  target_message_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this,
                        &UserImageLoader::NotifyDelegate,
                        username,
                        image));
}

void UserImageLoader::NotifyDelegate(const std::string& username,
                                     const SkBitmap& image) {
  if (delegate_)
    delegate_->OnImageLoaded(username, image);
}

}  // namespace chromeos
