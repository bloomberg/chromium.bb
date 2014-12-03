// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/clip_display_item.h"

#include "third_party/skia/include/core/SkCanvas.h"

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

}  // namespace cc
