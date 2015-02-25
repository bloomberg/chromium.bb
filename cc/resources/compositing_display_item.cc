// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/compositing_display_item.h"

#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event_argument.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/gfx/skia_util.h"

namespace cc {

CompositingDisplayItem::CompositingDisplayItem(float opacity,
                                               SkXfermode::Mode xfermode,
                                               SkRect* bounds,
                                               skia::RefPtr<SkColorFilter> cf)
    : opacity_(opacity),
      xfermode_(xfermode),
      has_bounds_(!!bounds),
      color_filter_(cf) {
  if (bounds)
    bounds_ = SkRect(*bounds);
}

CompositingDisplayItem::~CompositingDisplayItem() {
}

void CompositingDisplayItem::Raster(SkCanvas* canvas,
                                    SkDrawPictureCallback* callback) const {
  SkPaint paint;
  paint.setXfermodeMode(xfermode_);
  paint.setAlpha(opacity_ * 255);
  paint.setColorFilter(color_filter_.get());
  canvas->saveLayer(has_bounds_ ? &bounds_ : nullptr, &paint);
}

bool CompositingDisplayItem::IsSuitableForGpuRasterization() const {
  return true;
}

int CompositingDisplayItem::ApproximateOpCount() const {
  return 1;
}

size_t CompositingDisplayItem::PictureMemoryUsage() const {
  // TODO(pdr): Include color_filter's memory here.
  return sizeof(float) + sizeof(bool) + sizeof(SkRect) +
         sizeof(SkXfermode::Mode);
}

void CompositingDisplayItem::AsValueInto(
    base::trace_event::TracedValue* array) const {
  array->AppendString(base::StringPrintf(
      "CompositingDisplayItem opacity: %f, xfermode: %d", opacity_, xfermode_));
  if (has_bounds_)
    array->AppendString(base::StringPrintf(
        ", bounds: [%f, %f, %f, %f]", static_cast<float>(bounds_.x()),
        static_cast<float>(bounds_.y()), static_cast<float>(bounds_.width()),
        static_cast<float>(bounds_.height())));
}

EndCompositingDisplayItem::EndCompositingDisplayItem() {
}

EndCompositingDisplayItem::~EndCompositingDisplayItem() {
}

void EndCompositingDisplayItem::Raster(SkCanvas* canvas,
                                       SkDrawPictureCallback* callback) const {
  canvas->restore();
}

bool EndCompositingDisplayItem::IsSuitableForGpuRasterization() const {
  return true;
}

int EndCompositingDisplayItem::ApproximateOpCount() const {
  return 0;
}

size_t EndCompositingDisplayItem::PictureMemoryUsage() const {
  return 0;
}

void EndCompositingDisplayItem::AsValueInto(
    base::trace_event::TracedValue* array) const {
  array->AppendString("EndCompositingDisplayItem");
}

}  // namespace cc
