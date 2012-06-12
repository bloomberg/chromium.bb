// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_image.h"

namespace chromeos {

namespace {

bool IsAnimatedImage(const UserImage::RawImage& data) {
  const char kGIFStamp[] = "GIF";
  const size_t kGIFStampLength = sizeof(kGIFStamp) - 1;

  if (data.size() >= kGIFStampLength &&
      memcmp(&data[0], kGIFStamp, kGIFStampLength) == 0) {
    return true;
  }
  return false;
}

}  // namespace

UserImage::UserImage(const gfx::ImageSkia& image)
    : image_(image),
      has_raw_image_(false),
      has_animated_image_(false) {
}

UserImage::UserImage(const gfx::ImageSkia& image,
                     const RawImage& raw_image)
    : image_(image),
      has_raw_image_(true),
      has_animated_image_(IsAnimatedImage(raw_image)),
      raw_image_(raw_image) {
}

UserImage::~UserImage() {}

void UserImage::SetImage(const gfx::ImageSkia& image) {
  image_ = image;
  has_raw_image_ = false;
  has_animated_image_ = false;
  // Clear |raw_image_|.
  RawImage().swap(raw_image_);
}

}  // namespace chromeos
