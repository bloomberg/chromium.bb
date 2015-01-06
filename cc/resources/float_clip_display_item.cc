// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/float_clip_display_item.h"

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

}  // namespace cc
