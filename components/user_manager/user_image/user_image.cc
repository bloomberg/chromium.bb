// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user_image/user_image.h"

#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace user_manager {

namespace {

// Default quality for encoding user images.
const int kDefaultEncodingQuality = 90;

bool EncodeImageSkia(const gfx::ImageSkia& image,
                     UserImage::Bytes* output) {
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
  Bytes image_bytes;
  if (EncodeImageSkia(image, &image_bytes)) {
    UserImage result(image, image_bytes);
    result.MarkAsSafe();
    return result;
  }
  return UserImage(image);
}

UserImage::UserImage()
    : has_image_bytes_(false),
      is_safe_format_(false) {
}

UserImage::UserImage(const gfx::ImageSkia& image)
    : image_(image),
      has_image_bytes_(false),
      is_safe_format_(false) {
}

UserImage::UserImage(const gfx::ImageSkia& image,
                     const Bytes& image_bytes)
    : image_(image),
      has_image_bytes_(false),
      is_safe_format_(false) {
  has_image_bytes_ = true;
  image_bytes_ = image_bytes;
}

UserImage::~UserImage() {}

void UserImage::MarkAsSafe() {
  is_safe_format_ = true;
}

}  // namespace user_manager
