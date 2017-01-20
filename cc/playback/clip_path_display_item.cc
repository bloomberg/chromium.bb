// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/clip_path_display_item.h"

#include <stddef.h>
#include <stdint.h>

#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event_argument.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace cc {

ClipPathDisplayItem::ClipPathDisplayItem(const SkPath& clip_path,
                                         bool antialias)
    : DisplayItem(CLIP_PATH) {
  SetNew(clip_path, antialias);
}

ClipPathDisplayItem::~ClipPathDisplayItem() {
}

void ClipPathDisplayItem::SetNew(const SkPath& clip_path,
                                 bool antialias) {
  clip_path_ = clip_path;
  antialias_ = antialias;
}

void ClipPathDisplayItem::Raster(SkCanvas* canvas,
                                 SkPicture::AbortCallback* callback) const {
  canvas->save();
  canvas->clipPath(clip_path_, antialias_);
}

void ClipPathDisplayItem::AsValueInto(
    const gfx::Rect& visual_rect,
    base::trace_event::TracedValue* array) const {
  array->AppendString(base::StringPrintf(
      "ClipPathDisplayItem length: %d visualRect: [%s]",
      clip_path_.countPoints(), visual_rect.ToString().c_str()));
}

EndClipPathDisplayItem::EndClipPathDisplayItem() : DisplayItem(END_CLIP_PATH) {}

EndClipPathDisplayItem::~EndClipPathDisplayItem() {
}

void EndClipPathDisplayItem::Raster(
    SkCanvas* canvas,
    SkPicture::AbortCallback* callback) const {
  canvas->restore();
}

void EndClipPathDisplayItem::AsValueInto(
    const gfx::Rect& visual_rect,
    base::trace_event::TracedValue* array) const {
  array->AppendString(
      base::StringPrintf("EndClipPathDisplayItem visualRect: [%s]",
                         visual_rect.ToString().c_str()));
}

}  // namespace cc
