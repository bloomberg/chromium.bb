// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/drawing_display_item.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/debug/picture_debug_util.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDrawPictureCallback.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/utils/SkPictureUtils.h"

namespace cc {

DrawingDisplayItem::DrawingDisplayItem(skia::RefPtr<SkPicture> picture)
    : picture_(picture) {
}

DrawingDisplayItem::~DrawingDisplayItem() {
}

void DrawingDisplayItem::Raster(SkCanvas* canvas,
                                SkDrawPictureCallback* callback) const {
  canvas->save();
  if (callback)
    picture_->playback(canvas, callback);
  else
    canvas->drawPicture(picture_.get());
  canvas->restore();
}

void DrawingDisplayItem::RasterForTracing(SkCanvas* canvas) const {
  canvas->save();
  // The picture debugger in about:tracing doesn't drill down into |drawPicture|
  // operations. Calling |playback()| rather than |drawPicture()| causes the
  // skia operations in |picture_| to appear individually in the picture
  // produced for tracing rather than being hidden inside a drawPicture
  // operation.
  picture_->playback(canvas);
  canvas->restore();
}

bool DrawingDisplayItem::IsSuitableForGpuRasterization() const {
  return picture_->suitableForGpuRasterization(NULL);
}

int DrawingDisplayItem::ApproximateOpCount() const {
  return picture_->approximateOpCount();
}

size_t DrawingDisplayItem::PictureMemoryUsage() const {
  DCHECK(picture_);
  return SkPictureUtils::ApproximateBytesUsed(picture_.get());
}

void DrawingDisplayItem::AsValueInto(
    base::trace_event::TracedValue* array) const {
  array->BeginDictionary();
  array->SetString("name", "DrawingDisplayItem");
  array->SetString(
      "cullRect",
      base::StringPrintf("[%f,%f,%f,%f]", picture_->cullRect().x(),
                         picture_->cullRect().y(), picture_->cullRect().width(),
                         picture_->cullRect().height()));
  std::string b64_picture;
  PictureDebugUtil::SerializeAsBase64(picture_.get(), &b64_picture);
  array->SetString("skp64", b64_picture);
  array->EndDictionary();
}

}  // namespace cc
