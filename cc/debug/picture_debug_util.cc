// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/picture_debug_util.h"

#include <stddef.h>

#include <limits>
#include <memory>
#include <vector>

#include "base/base64.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPixelSerializer.h"
#include "third_party/skia/include/core/SkStream.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"

namespace {

class BitmapSerializer : public SkPixelSerializer {
 protected:
  bool onUseEncodedData(const void* data, size_t len) override { return true; }

  SkData* onEncode(const SkPixmap& pixmap) override {
    std::vector<unsigned char> data;

    // If bitmap is opaque, encode as JPEG.
    // Otherwise encode as PNG.
    bool encoding_succeeded = false;
    if (pixmap.isOpaque()) {
      constexpr int kJpegQuality = 80;
      encoding_succeeded = gfx::JPEGCodec::Encode(pixmap, kJpegQuality, &data);
    } else {
      const SkImageInfo& info = pixmap.info();
      const void* pixels = pixmap.addr();
      size_t row_bytes = pixmap.rowBytes();

      SkBitmap bm;
      // The cast is ok, since we only read the bm.
      if (!bm.installPixels(info, const_cast<void*>(pixels), row_bytes)) {
        return nullptr;
      }
      encoding_succeeded = gfx::PNGCodec::EncodeBGRASkBitmap(bm, false, &data);
    }

    if (encoding_succeeded) {
      return SkData::MakeWithCopy(&data.front(), data.size()).release();
    }
    return nullptr;
  }
};

}  // namespace

namespace cc {

void PictureDebugUtil::SerializeAsBase64(const SkPicture* picture,
                                         std::string* output) {
  SkDynamicMemoryWStream stream;
  BitmapSerializer serializer;
  picture->serialize(&stream, &serializer);

  size_t serialized_size = stream.bytesWritten();
  std::unique_ptr<char[]> serialized_picture(new char[serialized_size]);
  stream.copyTo(serialized_picture.get());
  base::Base64Encode(
      base::StringPiece(serialized_picture.get(), serialized_size), output);
}

}  // namespace cc
