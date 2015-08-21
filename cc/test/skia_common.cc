// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/skia_common.h"

#include "cc/playback/display_item_list.h"
#include "cc/playback/picture.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageGenerator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/skia_util.h"

namespace cc {

namespace {

class TestImageGenerator : public SkImageGenerator {
 public:
  explicit TestImageGenerator(const SkImageInfo& info)
      : SkImageGenerator(info) {}
};

}  // anonymous namespace

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

void DrawDisplayList(unsigned char* buffer,
                     const gfx::Rect& layer_rect,
                     scoped_refptr<DisplayItemList> list) {
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(layer_rect.width(), layer_rect.height());
  SkBitmap bitmap;
  bitmap.installPixels(info, buffer, info.minRowBytes());
  SkCanvas canvas(bitmap);
  canvas.clipRect(gfx::RectToSkRect(layer_rect));
  list->Raster(&canvas, NULL, gfx::Rect(), 1.0f);
}

void CreateDiscardableBitmap(const gfx::Size& size, SkBitmap* bitmap) {
  const SkImageInfo info =
      SkImageInfo::MakeN32Premul(size.width(), size.height());
  SkInstallDiscardablePixelRef(new TestImageGenerator(info), bitmap);
}

}  // namespace cc
