// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/compositing_display_item.h"

#include "third_party/skia/include/core/SkColorFilter.h"

namespace cc {

CompositingDisplayItem::CompositingDisplayItem(
    uint8_t alpha,
    SkBlendMode xfermode,
    SkRect* bounds,
    sk_sp<SkColorFilter> color_filter,
    bool lcd_text_requires_opaque_layer)
    : DisplayItem(COMPOSITING),
      alpha(alpha),
      xfermode(xfermode),
      has_bounds(!!bounds),
      bounds(bounds ? SkRect(*bounds) : SkRect()),
      color_filter(std::move(color_filter)),
      lcd_text_requires_opaque_layer(lcd_text_requires_opaque_layer) {}

CompositingDisplayItem::~CompositingDisplayItem() = default;

EndCompositingDisplayItem::EndCompositingDisplayItem()
    : DisplayItem(END_COMPOSITING) {}

EndCompositingDisplayItem::~EndCompositingDisplayItem() = default;

}  // namespace cc
