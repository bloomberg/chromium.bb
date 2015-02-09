// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/float_clip_display_item.h"

#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event_argument.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/skia_util.h"

namespace cc {

FloatClipDisplayItem::FloatClipDisplayItem(gfx::RectF clip_rect)
    : clip_rect_(clip_rect) {
}

FloatClipDisplayItem::~FloatClipDisplayItem() {
}

void FloatClipDisplayItem::Raster(SkCanvas* canvas,
                                  SkDrawPictureCallback* callback) const {
  canvas->save();
  canvas->clipRect(gfx::RectFToSkRect(clip_rect_));
}

bool FloatClipDisplayItem::IsSuitableForGpuRasterization() const {
  return true;
}

int FloatClipDisplayItem::ApproximateOpCount() const {
  return 1;
}

size_t FloatClipDisplayItem::PictureMemoryUsage() const {
  return sizeof(gfx::RectF);
}

void FloatClipDisplayItem::AsValueInto(
    base::trace_event::TracedValue* array) const {
  array->AppendString(base::StringPrintf("FloatClipDisplayItem rect: [%s]",
                                         clip_rect_.ToString().c_str()));
}

EndFloatClipDisplayItem::EndFloatClipDisplayItem() {
}

EndFloatClipDisplayItem::~EndFloatClipDisplayItem() {
}

void EndFloatClipDisplayItem::Raster(SkCanvas* canvas,
                                     SkDrawPictureCallback* callback) const {
  canvas->restore();
}

bool EndFloatClipDisplayItem::IsSuitableForGpuRasterization() const {
  return true;
}

int EndFloatClipDisplayItem::ApproximateOpCount() const {
  return 0;
}

size_t EndFloatClipDisplayItem::PictureMemoryUsage() const {
  return 0;
}

void EndFloatClipDisplayItem::AsValueInto(
    base::trace_event::TracedValue* array) const {
  array->AppendString("EndFloatClipDisplayItem");
}

}  // namespace cc
