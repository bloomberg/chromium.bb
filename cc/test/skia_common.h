// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_SKIA_COMMON_H_
#define CC_TEST_SKIA_COMMON_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "cc/paint/paint_image.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace gfx {
class Rect;
class Size;
}

namespace cc {
class DisplayItemList;

void DrawDisplayList(unsigned char* buffer,
                     const gfx::Rect& layer_rect,
                     scoped_refptr<const DisplayItemList> list);

bool AreDisplayListDrawingResultsSame(const gfx::Rect& layer_rect,
                                      const DisplayItemList* list_a,
                                      const DisplayItemList* list_b);

sk_sp<PaintImageGenerator> CreatePaintImageGenerator(const gfx::Size& size);

PaintImage CreateDiscardablePaintImage(
    const gfx::Size& size,
    sk_sp<SkColorSpace> color_space = nullptr);

}  // namespace cc

#endif  // CC_TEST_SKIA_COMMON_H_
