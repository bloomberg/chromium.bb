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
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                   layer_rect.width(),
                   layer_rect.height());
  bitmap.setPixels(buffer);
  SkCanvas canvas(bitmap);
  canvas.clipRect(gfx::RectToSkRect(layer_rect));
  picture->Raster(&canvas, NULL, layer_rect, 1.0f);
}

void CreateBitmap(const gfx::Size& size, const char* uri, SkBitmap* bitmap) {
  SkImageInfo info = {
    size.width(),
    size.height(),
    kPMColor_SkColorType,
    kPremul_SkAlphaType
  };

  bitmap->setConfig(info);
  bitmap->allocPixels();
  bitmap->pixelRef()->setImmutable();
  bitmap->pixelRef()->setURI(uri);
}

}  // namespace cc
