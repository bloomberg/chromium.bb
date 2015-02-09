// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/transparency_display_item.h"

#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event_argument.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/gfx/skia_util.h"

namespace cc {

TransparencyDisplayItem::TransparencyDisplayItem(float opacity,
                                                 SkXfermode::Mode blend_mode)
    : opacity_(opacity), blend_mode_(blend_mode) {
}

TransparencyDisplayItem::~TransparencyDisplayItem() {
}

void TransparencyDisplayItem::Raster(SkCanvas* canvas,
                                     SkDrawPictureCallback* callback) const {
  SkPaint paint;
  paint.setXfermodeMode(blend_mode_);
  paint.setAlpha(opacity_ * 255);
  canvas->saveLayer(NULL, &paint);
}

bool TransparencyDisplayItem::IsSuitableForGpuRasterization() const {
  return true;
}

int TransparencyDisplayItem::ApproximateOpCount() const {
  return 1;
}

size_t TransparencyDisplayItem::PictureMemoryUsage() const {
  return sizeof(float) + sizeof(SkXfermode::Mode);
}

void TransparencyDisplayItem::AsValueInto(
    base::trace_event::TracedValue* array) const {
  array->AppendString(
      base::StringPrintf("TransparencyDisplayItem opacity: %f, blend_mode: %d",
                         opacity_, blend_mode_));
}

EndTransparencyDisplayItem::EndTransparencyDisplayItem() {
}

EndTransparencyDisplayItem::~EndTransparencyDisplayItem() {
}

void EndTransparencyDisplayItem::Raster(SkCanvas* canvas,
                                        SkDrawPictureCallback* callback) const {
  canvas->restore();
}

bool EndTransparencyDisplayItem::IsSuitableForGpuRasterization() const {
  return true;
}

int EndTransparencyDisplayItem::ApproximateOpCount() const {
  return 0;
}

size_t EndTransparencyDisplayItem::PictureMemoryUsage() const {
  return 0;
}

void EndTransparencyDisplayItem::AsValueInto(
    base::trace_event::TracedValue* array) const {
  array->AppendString("EndTransparencyDisplayItem");
}

}  // namespace cc
