// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/transform_display_item.h"

#include "third_party/skia/include/core/SkCanvas.h"

namespace cc {

TransformDisplayItem::TransformDisplayItem(const gfx::Transform& transform)
    : transform_(transform) {
}

TransformDisplayItem::~TransformDisplayItem() {
}

void TransformDisplayItem::Raster(SkCanvas* canvas,
                                  SkDrawPictureCallback* callback) const {
  canvas->save();
  if (!transform_.IsIdentity())
    canvas->concat(transform_.matrix());
}

bool TransformDisplayItem::IsSuitableForGpuRasterization() const {
  return true;
}

int TransformDisplayItem::ApproximateOpCount() const {
  return 1;
}

size_t TransformDisplayItem::PictureMemoryUsage() const {
  return sizeof(gfx::Transform);
}

EndTransformDisplayItem::EndTransformDisplayItem() {
}

EndTransformDisplayItem::~EndTransformDisplayItem() {
}

void EndTransformDisplayItem::Raster(SkCanvas* canvas,
                                     SkDrawPictureCallback* callback) const {
  canvas->restore();
}

bool EndTransformDisplayItem::IsSuitableForGpuRasterization() const {
  return true;
}

int EndTransformDisplayItem::ApproximateOpCount() const {
  return 0;
}

size_t EndTransformDisplayItem::PictureMemoryUsage() const {
  return 0;
}

}  // namespace cc
