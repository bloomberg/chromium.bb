// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user_image/user_image.h"

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace user_manager {

namespace {

// Default quality for encoding user images.
const int kDefaultEncodingQuality = 90;

}  // namespace

// static
scoped_refptr<base::RefCountedBytes> UserImage::Encode(
    const SkBitmap& bitmap) {
  TRACE_EVENT2("oobe", "UserImage::Encode",
               "width", bitmap.width(), "height", bitmap.height());
  SkAutoLockPixels lock_bitmap(bitmap);
  std::vector<unsigned char> output;
  if (gfx::JPEGCodec::Encode(
          reinterpret_cast<unsigned char*>(bitmap.getAddr32(0, 0)),
          gfx::JPEGCodec::FORMAT_SkBitmap,
          bitmap.width(),
          bitmap.height(),
          bitmap.width() * bitmap.bytesPerPixel(),
          kDefaultEncodingQuality, &output)) {
    return base::RefCountedBytes::TakeVector(&output);
  } else {
    return nullptr;
  }
}

// static
std::unique_ptr<UserImage> UserImage::CreateAndEncode(
    const gfx::ImageSkia& image) {
  if (image.isNull())
    return base::WrapUnique(new UserImage);

  scoped_refptr<base::RefCountedBytes> image_bytes = Encode(*image.bitmap());
  if (image_bytes) {
    std::unique_ptr<UserImage> result(new UserImage(image, image_bytes));
    result->MarkAsSafe();
    return result;
  }
  return base::WrapUnique(new UserImage(image));
}

UserImage::UserImage() {
}

UserImage::UserImage(const gfx::ImageSkia& image)
    : image_(image) {
}

UserImage::UserImage(const gfx::ImageSkia& image,
                     scoped_refptr<base::RefCountedBytes> image_bytes)
    : image_(image),
      image_bytes_(image_bytes) {
}

UserImage::~UserImage() {}

void UserImage::MarkAsSafe() {
  is_safe_format_ = true;
}

}  // namespace user_manager
