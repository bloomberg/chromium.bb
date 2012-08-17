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

bool EncodeBGRAImageSkia(const gfx::ImageSkia& image,
                         bool discard_transparency,
                         std::vector<unsigned char>* output) {
  if (image.empty() || !image.bitmap())
    return false;
  return gfx::PNGCodec::EncodeBGRASkBitmap(*image.bitmap(),
      discard_transparency, output);
}

}  // namespace

// static
UserImage UserImage::CreateAndEncode(const gfx::ImageSkia& image) {
  RawImage raw_image;
  return EncodeBGRAImageSkia(image, false, &raw_image) ?
      UserImage(image, raw_image) : UserImage(image);
}

UserImage::UserImage()
    : has_raw_image_(false),
      has_animated_image_(false) {
}

UserImage::UserImage(const gfx::ImageSkia& image)
    : image_(image),
      has_raw_image_(false),
      has_animated_image_(false) {
}

UserImage::UserImage(const gfx::ImageSkia& image,
                     const RawImage& raw_image)
    : image_(image),
      has_raw_image_(false),
      has_animated_image_(false) {
  if (IsAnimatedImage(raw_image)) {
    has_animated_image_ = true;
    animated_image_ = raw_image;
    if (EncodeBGRAImageSkia(image_, false, &raw_image_))
      has_raw_image_ = true;
  } else {
    has_raw_image_ = true;
    raw_image_ = raw_image;
  }
}

UserImage::~UserImage() {}

void UserImage::DiscardRawImage() {
  RawImage().swap(raw_image_);  // Clear |raw_image_|.
}

}  // namespace chromeos
