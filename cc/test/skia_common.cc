// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/skia_common.h"

#include "cc/resources/picture.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skia_util.h"

namespace cc {

void DrawPicture(unsigned char* buffer,
                 const gfx::Rect& layer_rect,
                 scoped_refptr<Picture> picture) {
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(layer_rect.width(), layer_rect.height());
  SkBitmap bitmap;
  bitmap.installPixels(info, buffer, info.minRowBytes());
  SkCanvas canvas(bitmap);
  canvas.clipRect(gfx::RectToSkRect(layer_rect));
  // We're drawing the entire canvas, so the negated content region is empty.
  gfx::Rect negated_content_region;
  picture->Raster(&canvas, NULL, negated_content_region, 1.0f);
}

void CreateBitmap(const gfx::Size& size, const char* uri, SkBitmap* bitmap) {
  SkImageInfo info = SkImageInfo::MakeN32Premul(size.width(), size.height());

  bitmap->allocPixels(info);
  bitmap->pixelRef()->setImmutable();
  bitmap->pixelRef()->setURI(uri);
}

}  // namespace cc
