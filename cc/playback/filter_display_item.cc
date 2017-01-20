// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/filter_display_item.h"

#include <stddef.h>

#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/output/render_surface_filters.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/skia_util.h"

namespace cc {

FilterDisplayItem::FilterDisplayItem(const FilterOperations& filters,
                                     const gfx::RectF& bounds,
                                     const gfx::PointF& origin)
    : DisplayItem(FILTER) {
  SetNew(filters, bounds, origin);
}

FilterDisplayItem::~FilterDisplayItem() {}

void FilterDisplayItem::SetNew(const FilterOperations& filters,
                               const gfx::RectF& bounds,
                               const gfx::PointF& origin) {
  filters_ = filters;
  bounds_ = bounds;
  origin_ = origin;
}

void FilterDisplayItem::Raster(SkCanvas* canvas,
                               SkPicture::AbortCallback* callback) const {
  canvas->save();
  canvas->translate(origin_.x(), origin_.y());

  sk_sp<SkImageFilter> image_filter = RenderSurfaceFilters::BuildImageFilter(
      filters_, gfx::SizeF(bounds_.width(), bounds_.height()));
  SkRect boundaries = RectFToSkRect(bounds_);
  boundaries.offset(-origin_.x(), -origin_.y());

  SkPaint paint;
  paint.setBlendMode(SkBlendMode::kSrcOver);
  paint.setImageFilter(std::move(image_filter));
  canvas->saveLayer(&boundaries, &paint);

  canvas->translate(-origin_.x(), -origin_.y());
}

void FilterDisplayItem::AsValueInto(
    const gfx::Rect& visual_rect,
    base::trace_event::TracedValue* array) const {
  array->AppendString(base::StringPrintf(
      "FilterDisplayItem bounds: [%s] visualRect: [%s]",
      bounds_.ToString().c_str(), visual_rect.ToString().c_str()));
}

EndFilterDisplayItem::EndFilterDisplayItem() : DisplayItem(END_FILTER) {}

EndFilterDisplayItem::~EndFilterDisplayItem() {}

void EndFilterDisplayItem::Raster(SkCanvas* canvas,
                                  SkPicture::AbortCallback* callback) const {
  canvas->restore();
  canvas->restore();
}

void EndFilterDisplayItem::AsValueInto(
    const gfx::Rect& visual_rect,
    base::trace_event::TracedValue* array) const {
  array->AppendString(
      base::StringPrintf("EndFilterDisplayItem  visualRect: [%s]",
                         visual_rect.ToString().c_str()));
}

}  // namespace cc
