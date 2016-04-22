// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_SKIA_COMMON_H_
#define CC_TEST_SKIA_COMMON_H_

#include <memory>

#include "base/memory/ref_counted.h"
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
                     scoped_refptr<DisplayItemList> list);

bool AreDisplayListDrawingResultsSame(const gfx::Rect& layer_rect,
                                      scoped_refptr<DisplayItemList> list_a,
                                      scoped_refptr<DisplayItemList> list_b);

sk_sp<SkImage> CreateDiscardableImage(const gfx::Size& size);

}  // namespace cc

#endif  // CC_TEST_SKIA_COMMON_H_
