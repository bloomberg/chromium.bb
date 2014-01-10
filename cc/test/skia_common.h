// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_SKIA_COMMON_H_
#define CC_TEST_SKIA_COMMON_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkFlattenable.h"
#include "third_party/skia/include/core/SkPixelRef.h"

namespace gfx {
class Rect;
class Size;
}

namespace cc {
class Picture;

class TestPixelRef : public SkPixelRef {
 public:
  explicit TestPixelRef(const SkImageInfo& info);
  virtual ~TestPixelRef();

  virtual SkFlattenable::Factory getFactory() const OVERRIDE;
  virtual void* onLockPixels(SkColorTable** color_table) OVERRIDE;
  virtual void onUnlockPixels() OVERRIDE {}
  virtual SkPixelRef* deepCopy(
      SkBitmap::Config config,
      const SkIRect* subset) OVERRIDE;
 private:
  scoped_ptr<char[]> pixels_;
};

void DrawPicture(unsigned char* buffer,
                 const gfx::Rect& layer_rect,
                 scoped_refptr<Picture> picture);

void CreateBitmap(gfx::Size size, const char* uri, SkBitmap* bitmap);

}  // namespace cc

#endif  // CC_TEST_SKIA_COMMON_H_
