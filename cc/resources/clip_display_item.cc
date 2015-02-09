// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/clip_display_item.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event_argument.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/skia_util.h"

namespace cc {

ClipDisplayItem::ClipDisplayItem(gfx::Rect clip_rect,
                                 const std::vector<SkRRect>& rounded_clip_rects)
    : clip_rect_(clip_rect), rounded_clip_rects_(rounded_clip_rects) {
}

ClipDisplayItem::~ClipDisplayItem() {
}

void ClipDisplayItem::Raster(SkCanvas* canvas,
                             SkDrawPictureCallback* callback) const {
  canvas->save();
  canvas->clipRect(SkRect::MakeXYWH(clip_rect_.x(), clip_rect_.y(),
                                    clip_rect_.width(), clip_rect_.height()));
  for (size_t i = 0; i < rounded_clip_rects_.size(); ++i) {
    if (rounded_clip_rects_[i].isRect()) {
      canvas->clipRect(rounded_clip_rects_[i].rect());
    } else {
      bool antialiased = true;
      canvas->clipRRect(rounded_clip_rects_[i], SkRegion::kIntersect_Op,
                        antialiased);
    }
  }
}

bool ClipDisplayItem::IsSuitableForGpuRasterization() const {
  return true;
}

int ClipDisplayItem::ApproximateOpCount() const {
  return 1;
}

size_t ClipDisplayItem::PictureMemoryUsage() const {
  size_t total_size = sizeof(gfx::Rect);
  for (size_t i = 0; i < rounded_clip_rects_.size(); ++i) {
    total_size += sizeof(rounded_clip_rects_[i]);
  }
  return total_size;
}

void ClipDisplayItem::AsValueInto(base::trace_event::TracedValue* array) const {
  std::string value = base::StringPrintf("ClipDisplayItem rect: [%s]",
                                         clip_rect_.ToString().c_str());
  for (const SkRRect& rounded_rect : rounded_clip_rects_) {
    base::StringAppendF(
        &value, " rounded_rect: [rect: [%s]",
        gfx::SkRectToRectF(rounded_rect.rect()).ToString().c_str());
    base::StringAppendF(&value, " radii: [");
    SkVector upper_left_radius = rounded_rect.radii(SkRRect::kUpperLeft_Corner);
    base::StringAppendF(&value, "[%f,%f],", upper_left_radius.x(),
                        upper_left_radius.y());
    SkVector upper_right_radius =
        rounded_rect.radii(SkRRect::kUpperRight_Corner);
    base::StringAppendF(&value, " [%f,%f],", upper_right_radius.x(),
                        upper_right_radius.y());
    SkVector lower_right_radius =
        rounded_rect.radii(SkRRect::kLowerRight_Corner);
    base::StringAppendF(&value, " [%f,%f],", lower_right_radius.x(),
                        lower_right_radius.y());
    SkVector lower_left_radius = rounded_rect.radii(SkRRect::kLowerLeft_Corner);
    base::StringAppendF(&value, " [%f,%f]]", lower_left_radius.x(),
                        lower_left_radius.y());
  }
  array->AppendString(value);
}

EndClipDisplayItem::EndClipDisplayItem() {
}

EndClipDisplayItem::~EndClipDisplayItem() {
}

void EndClipDisplayItem::Raster(SkCanvas* canvas,
                                SkDrawPictureCallback* callback) const {
  canvas->restore();
}

bool EndClipDisplayItem::IsSuitableForGpuRasterization() const {
  return true;
}

int EndClipDisplayItem::ApproximateOpCount() const {
  return 0;
}

size_t EndClipDisplayItem::PictureMemoryUsage() const {
  return 0;
}

void EndClipDisplayItem::AsValueInto(
    base::trace_event::TracedValue* array) const {
  array->AppendString("EndClipDisplayItem");
}

}  // namespace cc
