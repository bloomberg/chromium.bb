// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/float_clip_display_item.h"

#include <stddef.h>

#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event_argument.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/skia_util.h"

namespace cc {

FloatClipDisplayItem::FloatClipDisplayItem(const gfx::RectF& clip_rect)
    : DisplayItem(FLOAT_CLIP) {
  SetNew(clip_rect);
}

FloatClipDisplayItem::~FloatClipDisplayItem() {
}

void FloatClipDisplayItem::SetNew(const gfx::RectF& clip_rect) {
  clip_rect_ = clip_rect;
}

void FloatClipDisplayItem::Raster(SkCanvas* canvas,
                                  SkPicture::AbortCallback* callback) const {
  canvas->save();
  canvas->clipRect(gfx::RectFToSkRect(clip_rect_));
}

void FloatClipDisplayItem::AsValueInto(
    const gfx::Rect& visual_rect,
    base::trace_event::TracedValue* array) const {
  array->AppendString(base::StringPrintf(
      "FloatClipDisplayItem rect: [%s] visualRect: [%s]",
      clip_rect_.ToString().c_str(), visual_rect.ToString().c_str()));
}

EndFloatClipDisplayItem::EndFloatClipDisplayItem()
    : DisplayItem(END_FLOAT_CLIP) {}

EndFloatClipDisplayItem::~EndFloatClipDisplayItem() {
}

void EndFloatClipDisplayItem::Raster(
    SkCanvas* canvas,
    SkPicture::AbortCallback* callback) const {
  canvas->restore();
}

void EndFloatClipDisplayItem::AsValueInto(
    const gfx::Rect& visual_rect,
    base::trace_event::TracedValue* array) const {
  array->AppendString(
      base::StringPrintf("EndFloatClipDisplayItem visualRect: [%s]",
                         visual_rect.ToString().c_str()));
}

}  // namespace cc
