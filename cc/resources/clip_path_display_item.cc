// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/clip_path_display_item.h"

#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event_argument.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace cc {

ClipPathDisplayItem::ClipPathDisplayItem(const SkPath& clip_path,
                                         SkRegion::Op clip_op,
                                         bool antialias)
    : clip_path_(clip_path), clip_op_(clip_op), antialias_(antialias) {
}

ClipPathDisplayItem::~ClipPathDisplayItem() {
}

void ClipPathDisplayItem::Raster(SkCanvas* canvas,
                                 SkDrawPictureCallback* callback) const {
  canvas->save();
  canvas->clipPath(clip_path_, clip_op_, antialias_);
}

bool ClipPathDisplayItem::IsSuitableForGpuRasterization() const {
  return true;
}

int ClipPathDisplayItem::ApproximateOpCount() const {
  return 1;
}

size_t ClipPathDisplayItem::PictureMemoryUsage() const {
  size_t total_size = sizeof(SkPath) + sizeof(SkRegion::Op) + sizeof(bool);
  return total_size;
}

void ClipPathDisplayItem::AsValueInto(
    base::trace_event::TracedValue* array) const {
  array->AppendString(base::StringPrintf("ClipPathDisplayItem length: %d",
                                         clip_path_.countPoints()));
}

EndClipPathDisplayItem::EndClipPathDisplayItem() {
}

EndClipPathDisplayItem::~EndClipPathDisplayItem() {
}

void EndClipPathDisplayItem::Raster(SkCanvas* canvas,
                                    SkDrawPictureCallback* callback) const {
  canvas->restore();
}

bool EndClipPathDisplayItem::IsSuitableForGpuRasterization() const {
  return true;
}

int EndClipPathDisplayItem::ApproximateOpCount() const {
  return 0;
}

size_t EndClipPathDisplayItem::PictureMemoryUsage() const {
  return 0;
}

void EndClipPathDisplayItem::AsValueInto(
    base::trace_event::TracedValue* array) const {
  array->AppendString("EndClipPathDisplayItem");
}

}  // namespace cc
