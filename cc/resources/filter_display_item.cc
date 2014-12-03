// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/filter_display_item.h"

#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/gfx/skia_util.h"

namespace cc {

FilterDisplayItem::FilterDisplayItem(skia::RefPtr<SkImageFilter> filter,
                                     gfx::RectF bounds)
    : filter_(filter), bounds_(bounds) {
}

FilterDisplayItem::~FilterDisplayItem() {
}

void FilterDisplayItem::Raster(SkCanvas* canvas,
                               SkDrawPictureCallback* callback) const {
  canvas->save();
  SkRect boundaries;
  filter_->computeFastBounds(gfx::RectFToSkRect(bounds_), &boundaries);
  canvas->translate(bounds_.x(), bounds_.y());
  boundaries.offset(-bounds_.x(), -bounds_.y());

  SkPaint paint;
  paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
  paint.setImageFilter(filter_.get());
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

}  // namespace cc
