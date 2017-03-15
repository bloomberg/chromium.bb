// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/clip_display_item.h"

#include <stddef.h>

#include <string>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event_argument.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/skia_util.h"

namespace cc {
class ImageSerializationProcessor;

ClipDisplayItem::ClipDisplayItem(const gfx::Rect& clip_rect,
                                 const std::vector<SkRRect>& rounded_clip_rects,
                                 bool antialias)
    : DisplayItem(CLIP) {
  SetNew(clip_rect, rounded_clip_rects, antialias);
}

void ClipDisplayItem::SetNew(const gfx::Rect& clip_rect,
                             const std::vector<SkRRect>& rounded_clip_rects,
                             bool antialias) {
  clip_rect_ = clip_rect;
  rounded_clip_rects_ = rounded_clip_rects;
  antialias_ = antialias;
}

ClipDisplayItem::~ClipDisplayItem() {}

void ClipDisplayItem::Raster(SkCanvas* canvas,
                             SkPicture::AbortCallback* callback) const {
  canvas->save();
  canvas->clipRect(gfx::RectToSkRect(clip_rect_), antialias_);
  for (size_t i = 0; i < rounded_clip_rects_.size(); ++i) {
    if (rounded_clip_rects_[i].isRect()) {
      canvas->clipRect(rounded_clip_rects_[i].rect(), antialias_);
    } else {
      canvas->clipRRect(rounded_clip_rects_[i], antialias_);
    }
  }
}

EndClipDisplayItem::EndClipDisplayItem() : DisplayItem(END_CLIP) {}

EndClipDisplayItem::~EndClipDisplayItem() {
}

void EndClipDisplayItem::Raster(SkCanvas* canvas,
                                SkPicture::AbortCallback* callback) const {
  canvas->restore();
}

}  // namespace cc
