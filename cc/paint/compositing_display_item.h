// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_COMPOSITING_DISPLAY_ITEM_H_
#define CC_PAINT_COMPOSITING_DISPLAY_ITEM_H_

#include <stddef.h>

#include "cc/paint/display_item.h"
#include "cc/paint/paint_export.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {

class CC_PAINT_EXPORT CompositingDisplayItem : public DisplayItem {
 public:
  CompositingDisplayItem(uint8_t alpha,
                         SkBlendMode xfermode,
                         SkRect* bounds,
                         sk_sp<SkColorFilter> color_filter,
                         bool lcd_text_requires_opaque_layer);
  ~CompositingDisplayItem() override;

  size_t ExternalMemoryUsage() const {
    // TODO(pdr): Include color_filter's memory here.
    return 0;
  }
  int OpCount() const { return 1; }

  const uint8_t alpha;
  const SkBlendMode xfermode;
  const bool has_bounds;
  const SkRect bounds;
  const sk_sp<SkColorFilter> color_filter;
  const bool lcd_text_requires_opaque_layer;
};

class CC_PAINT_EXPORT EndCompositingDisplayItem : public DisplayItem {
 public:
  EndCompositingDisplayItem();
  ~EndCompositingDisplayItem() override;

  int OpCount() const { return 0; }
};

}  // namespace cc

#endif  // CC_PAINT_COMPOSITING_DISPLAY_ITEM_H_
