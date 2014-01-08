// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/skia_common.h"

#include "cc/resources/picture.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkBitmapDevice.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skia_util.h"

namespace cc {

TestPixelRef::TestPixelRef(const SkImageInfo& info)
    : SkPixelRef(info), pixels_(new char[4 * info.fWidth * info.fHeight]) {}

TestPixelRef::~TestPixelRef() {}

SkFlattenable::Factory TestPixelRef::getFactory() const { return NULL; }

void* TestPixelRef::onLockPixels(SkColorTable** color_table) {
  return pixels_.get();
}

SkPixelRef* TestPixelRef::deepCopy(
    SkBitmap::Config config,
    const SkIRect* subset) {
  this->ref();
  return this;
}


TestLazyPixelRef::TestLazyPixelRef(const SkImageInfo& info)
    : skia::LazyPixelRef(info),
      pixels_(new char[4 * info.fWidth * info.fHeight]) {}

TestLazyPixelRef::~TestLazyPixelRef() {}

SkFlattenable::Factory TestLazyPixelRef::getFactory() const { return NULL; }

void* TestLazyPixelRef::onLockPixels(SkColorTable** color_table) {
  return pixels_.get();
}

bool TestLazyPixelRef::PrepareToDecode(const PrepareParams& params) {
  return true;
}

bool TestLazyPixelRef::MaybeDecoded() {
  return true;
}

SkPixelRef* TestLazyPixelRef::deepCopy(
    SkBitmap::Config config,
    const SkIRect* subset) {
  this->ref();
  return this;
}

void DrawPicture(unsigned char* buffer,
                 gfx::Rect layer_rect,
                 scoped_refptr<Picture> picture) {
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                   layer_rect.width(),
                   layer_rect.height());
  bitmap.setPixels(buffer);
  SkBitmapDevice device(bitmap);
  SkCanvas canvas(&device);
  canvas.clipRect(gfx::RectToSkRect(layer_rect));
  picture->Raster(&canvas, NULL, layer_rect, 1.0f);
}

void CreateBitmap(gfx::Size size, const char* uri, SkBitmap* bitmap) {
  SkImageInfo info = {
    size.width(),
    size.height(),
    kPMColor_SkColorType,
    kPremul_SkAlphaType
  };

  skia::RefPtr<TestLazyPixelRef> lazy_pixel_ref =
      skia::AdoptRef(new TestLazyPixelRef(info));
  lazy_pixel_ref->setURI(uri);

  bitmap->setConfig(info);
  bitmap->setPixelRef(lazy_pixel_ref.get());
}


}  // namespace cc
