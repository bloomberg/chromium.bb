// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/float_clip_display_item.h"

#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event_argument.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/skia_util.h"

namespace cc {

FloatClipDisplayItem::FloatClipDisplayItem() {
}

FloatClipDisplayItem::~FloatClipDisplayItem() {
}

void FloatClipDisplayItem::SetNew(const gfx::RectF& clip_rect) {
  clip_rect_ = clip_rect;

  size_t memory_usage = sizeof(gfx::RectF);
  DisplayItem::SetNew(true /* suitable_for_gpu_raster */, 1 /* op_count */,
                      memory_usage);
}

void FloatClipDisplayItem::Raster(SkCanvas* canvas,
                                  SkDrawPictureCallback* callback) const {
  canvas->save();
  canvas->clipRect(gfx::RectFToSkRect(clip_rect_));
}

void FloatClipDisplayItem::AsValueInto(
    base::trace_event::TracedValue* array) const {
  array->AppendString(base::StringPrintf("FloatClipDisplayItem rect: [%s]",
                                         clip_rect_.ToString().c_str()));
}

EndFloatClipDisplayItem::EndFloatClipDisplayItem() {
  DisplayItem::SetNew(true /* suitable_for_gpu_raster */, 0 /* op_count */,
                      0 /* memory_usage */);
}

EndFloatClipDisplayItem::~EndFloatClipDisplayItem() {
}

void EndFloatClipDisplayItem::Raster(SkCanvas* canvas,
                                     SkDrawPictureCallback* callback) const {
  canvas->restore();
}

void EndFloatClipDisplayItem::AsValueInto(
    base::trace_event::TracedValue* array) const {
  array->AppendString("EndFloatClipDisplayItem");
}

}  // namespace cc
