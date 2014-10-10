// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/suggestions/image_encoder.h"

#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_skia.h"

namespace suggestions {

SkBitmap* DecodeJPEGToSkBitmap(const std::vector<unsigned char>& encoded_data) {
  return gfx::JPEGCodec::Decode(&encoded_data[0], encoded_data.size());
}

bool EncodeSkBitmapToJPEG(const SkBitmap& bitmap,
                          std::vector<unsigned char>* dest) {
  SkAutoLockPixels bitmap_lock(bitmap);
  if (!bitmap.readyToDraw() || bitmap.isNull()) {
    return false;
  }
  return gfx::JPEGCodec::Encode(
      reinterpret_cast<unsigned char*>(bitmap.getAddr32(0, 0)),
      gfx::JPEGCodec::FORMAT_SkBitmap, bitmap.width(), bitmap.height(),
      bitmap.rowBytes(), 100, dest);
}

}  // namespace suggestions
