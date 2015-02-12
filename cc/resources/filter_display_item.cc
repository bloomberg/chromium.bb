// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/filter_display_item.h"

#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/output/render_surface_filters.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/gfx/skia_util.h"

namespace cc {

FilterDisplayItem::FilterDisplayItem(const FilterOperations& filters,
                                     gfx::RectF bounds)
    : filters_(filters), bounds_(bounds) {
}

FilterDisplayItem::~FilterDisplayItem() {
}

void FilterDisplayItem::Raster(SkCanvas* canvas,
                               SkDrawPictureCallback* callback) const {
  canvas->save();
  canvas->translate(bounds_.x(), bounds_.y());

  skia::RefPtr<SkImageFilter> image_filter =
      RenderSurfaceFilters::BuildImageFilter(
          filters_, gfx::SizeF(bounds_.width(), bounds_.height()));
  SkRect boundaries;
  image_filter->computeFastBounds(
      SkRect::MakeWH(bounds_.width(), bounds_.height()), &boundaries);

  SkPaint paint;
  paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
  paint.setImageFilter(image_filter.get());
  canvas->saveLayer(&boundaries, &paint);

  canvas->translate(-bounds_.x(), -bounds_.y());
}

bool FilterDisplayItem::IsSuitableForGpuRasterization() const {
  return true;
}

int FilterDisplayItem::ApproximateOpCount() const {
  return 1;
}

size_t FilterDisplayItem::PictureMemoryUsage() const {
  return sizeof(skia::RefPtr<SkImageFilter>) + sizeof(gfx::RectF);
}

void FilterDisplayItem::AsValueInto(
    base::trace_event::TracedValue* array) const {
  array->AppendString(base::StringPrintf("FilterDisplayItem bounds: [%s]",
                                         bounds_.ToString().c_str()));
}

EndFilterDisplayItem::EndFilterDisplayItem() {
}

EndFilterDisplayItem::~EndFilterDisplayItem() {
}

void EndFilterDisplayItem::Raster(SkCanvas* canvas,
                                  SkDrawPictureCallback* callback) const {
  canvas->restore();
  canvas->restore();
}

bool EndFilterDisplayItem::IsSuitableForGpuRasterization() const {
  return true;
}

int EndFilterDisplayItem::ApproximateOpCount() const {
  return 0;
}

size_t EndFilterDisplayItem::PictureMemoryUsage() const {
  return 0;
}

void EndFilterDisplayItem::AsValueInto(
    base::trace_event::TracedValue* array) const {
  array->AppendString("EndFilterDisplayItem");
}

}  // namespace cc
