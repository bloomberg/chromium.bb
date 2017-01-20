// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/compositing_display_item.h"

#include <stddef.h>
#include <stdint.h>

#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event_argument.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkFlattenable.h"
#include "third_party/skia/include/core/SkFlattenableSerialization.h"
#include "third_party/skia/include/core/SkPaint.h"

#include "ui/gfx/skia_util.h"

namespace cc {

CompositingDisplayItem::CompositingDisplayItem(
    uint8_t alpha,
    SkBlendMode xfermode,
    SkRect* bounds,
    sk_sp<SkColorFilter> cf,
    bool lcd_text_requires_opaque_layer)
    : DisplayItem(COMPOSITING) {
  SetNew(alpha, xfermode, bounds, std::move(cf),
         lcd_text_requires_opaque_layer);
}

CompositingDisplayItem::~CompositingDisplayItem() {
}

void CompositingDisplayItem::SetNew(uint8_t alpha,
                                    SkBlendMode xfermode,
                                    SkRect* bounds,
                                    sk_sp<SkColorFilter> cf,
                                    bool lcd_text_requires_opaque_layer) {
  alpha_ = alpha;
  xfermode_ = xfermode;
  has_bounds_ = !!bounds;
  if (bounds)
    bounds_ = SkRect(*bounds);
  color_filter_ = std::move(cf);
  lcd_text_requires_opaque_layer_ = lcd_text_requires_opaque_layer;
}

void CompositingDisplayItem::Raster(
    SkCanvas* canvas,
    SkPicture::AbortCallback* callback) const {
  SkPaint paint;
  paint.setBlendMode(xfermode_);
  paint.setAlpha(alpha_);
  paint.setColorFilter(color_filter_);
  const SkRect* bounds = has_bounds_ ? &bounds_ : nullptr;
  if (lcd_text_requires_opaque_layer_)
    canvas->saveLayer(bounds, &paint);
  else
    canvas->saveLayerPreserveLCDTextRequests(bounds, &paint);
}

void CompositingDisplayItem::AsValueInto(
    const gfx::Rect& visual_rect,
    base::trace_event::TracedValue* array) const {
  std::string info = base::StringPrintf(
      "CompositingDisplayItem alpha: %d, xfermode: %d, visualRect: [%s]",
      alpha_, static_cast<int>(xfermode_), visual_rect.ToString().c_str());
  if (has_bounds_) {
    base::StringAppendF(
        &info, ", bounds: [%f, %f, %f, %f]", static_cast<float>(bounds_.x()),
        static_cast<float>(bounds_.y()), static_cast<float>(bounds_.width()),
        static_cast<float>(bounds_.height()));
  }
  array->AppendString(info);
}

EndCompositingDisplayItem::EndCompositingDisplayItem()
    : DisplayItem(END_COMPOSITING) {}

EndCompositingDisplayItem::~EndCompositingDisplayItem() {
}

void EndCompositingDisplayItem::Raster(
    SkCanvas* canvas,
    SkPicture::AbortCallback* callback) const {
  canvas->restore();
}

void EndCompositingDisplayItem::AsValueInto(
    const gfx::Rect& visual_rect,
    base::trace_event::TracedValue* array) const {
  array->AppendString(
      base::StringPrintf("EndCompositingDisplayItem visualRect: [%s]",
                         visual_rect.ToString().c_str()));
}

}  // namespace cc
