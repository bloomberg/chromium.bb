// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_image.h"

#include "ui/gfx/codec/png_codec.h"

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
  if (gfx::PNGCodec::EncodeBGRASkBitmap(image_, false, &raw_image_))
    has_raw_image_ = true;
}

UserImage::UserImage(const gfx::ImageSkia& image,
                     const RawImage& raw_image)
    : image_(image),
      has_raw_image_(false),
      has_animated_image_(false) {
  if (IsAnimatedImage(raw_image)) {
    has_animated_image_ = true;
    animated_image_ = raw_image;
  }
  if (gfx::PNGCodec::EncodeBGRASkBitmap(image_, false, &raw_image_))
    has_raw_image_ = true;
}

UserImage::~UserImage() {}

void UserImage::SetImage(const gfx::ImageSkia& image) {
  image_ = image;
  if (gfx::PNGCodec::EncodeBGRASkBitmap(image_, false, &raw_image_)) {
    has_raw_image_ = true;
  } else {
    has_raw_image_ = false;
    RawImage().swap(raw_image_);  // Clear |raw_image_|.
  }

  has_animated_image_ = false;
  if (!animated_image_.empty())
    RawImage().swap(animated_image_);  // Clear |animated_image_|.
}

}  // namespace chromeos
