// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_COMPOSITING_DISPLAY_ITEM_H_
#define CC_PLAYBACK_COMPOSITING_DISPLAY_ITEM_H_

#include <stddef.h>

#include "cc/base/cc_export.h"
#include "cc/playback/display_item.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {

class CC_EXPORT CompositingDisplayItem : public DisplayItem {
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
  int ApproximateOpCount() const { return 1; }

  const uint8_t alpha;
  const SkBlendMode xfermode;
  const bool has_bounds;
  const SkRect bounds;
  const sk_sp<SkColorFilter> color_filter;
  const bool lcd_text_requires_opaque_layer;
};

class CC_EXPORT EndCompositingDisplayItem : public DisplayItem {
 public:
  EndCompositingDisplayItem();
  ~EndCompositingDisplayItem() override;

  int ApproximateOpCount() const { return 0; }
};

}  // namespace cc

#endif  // CC_PLAYBACK_COMPOSITING_DISPLAY_ITEM_H_
