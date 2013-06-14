// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_SKIA_COMMON_H_
#define CC_TEST_SKIA_COMMON_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "skia/ext/lazy_pixel_ref.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkFlattenable.h"

namespace gfx {
class Rect;
class Size;
}

namespace cc {
class Picture;

class TestPixelRef : public SkPixelRef {
 public:
  TestPixelRef(int width, int height);
  virtual ~TestPixelRef();

  virtual SkFlattenable::Factory getFactory() OVERRIDE;
  virtual void* onLockPixels(SkColorTable** color_table) OVERRIDE;
  virtual void onUnlockPixels() OVERRIDE {}
  virtual SkPixelRef* deepCopy(
      SkBitmap::Config config,
      const SkIRect* subset) OVERRIDE;
 private:
  scoped_ptr<char[]> pixels_;
};

class TestLazyPixelRef : public skia::LazyPixelRef {
 public:
  TestLazyPixelRef(int width, int height);
  virtual ~TestLazyPixelRef();

  virtual SkFlattenable::Factory getFactory() OVERRIDE;
  virtual void* onLockPixels(SkColorTable** color_table) OVERRIDE;
  virtual void onUnlockPixels() OVERRIDE {}
  virtual bool PrepareToDecode(const PrepareParams& params) OVERRIDE;
  virtual bool MaybeDecoded() OVERRIDE;
  virtual SkPixelRef* deepCopy(
      SkBitmap::Config config,
      const SkIRect* subset) OVERRIDE;
  virtual void Decode() OVERRIDE {}
 private:
  scoped_ptr<char[]> pixels_;
};

void DrawPicture(unsigned char* buffer,
                 gfx::Rect layer_rect,
                 scoped_refptr<Picture> picture);

void CreateBitmap(gfx::Size size, const char* uri, SkBitmap* bitmap);

}  // namespace cc

#endif  // CC_TEST_SKIA_COMMON_H_
