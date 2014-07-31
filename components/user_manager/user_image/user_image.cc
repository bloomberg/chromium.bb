// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user_image/user_image.h"

#include "base/debug/trace_event.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace user_manager {

namespace {

// Default quality for encoding user images.
const int kDefaultEncodingQuality = 90;

bool EncodeImageSkia(const gfx::ImageSkia& image,
                     std::vector<unsigned char>* output) {
  TRACE_EVENT2("oobe", "EncodeImageSkia",
               "width", image.width(), "height", image.height());
  if (image.isNull())
    return false;
  const SkBitmap& bitmap = *image.bitmap();
  SkAutoLockPixels lock_image(bitmap);
  return gfx::JPEGCodec::Encode(
      reinterpret_cast<unsigned char*>(bitmap.getAddr32(0, 0)),
      gfx::JPEGCodec::FORMAT_SkBitmap,
      bitmap.width(),
      bitmap.height(),
      bitmap.width() * bitmap.bytesPerPixel(),
      kDefaultEncodingQuality, output);
}

}  // namespace

// static
UserImage UserImage::CreateAndEncode(const gfx::ImageSkia& image) {
  RawImage raw_image;
  if (EncodeImageSkia(image, &raw_image)) {
    UserImage result(image, raw_image);
    result.MarkAsSafe();
    return result;
  }
  return UserImage(image);
}

UserImage::UserImage()
    : has_raw_image_(false),
      is_safe_format_(false) {
}

UserImage::UserImage(const gfx::ImageSkia& image)
    : image_(image),
      has_raw_image_(false),
      is_safe_format_(false) {
}

UserImage::UserImage(const gfx::ImageSkia& image,
                     const RawImage& raw_image)
    : image_(image),
      has_raw_image_(false),
      is_safe_format_(false) {
  has_raw_image_ = true;
  raw_image_ = raw_image;
}

UserImage::~UserImage() {}

void UserImage::DiscardRawImage() {
  RawImage().swap(raw_image_);  // Clear |raw_image_|.
}

void UserImage::MarkAsSafe() {
  is_safe_format_ = true;
}

}  // namespace user_manager
