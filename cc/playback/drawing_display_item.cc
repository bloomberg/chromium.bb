// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/drawing_display_item.h"

#include <stddef.h>

#include <string>

#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "base/values.h"
#include "cc/debug/picture_debug_util.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/utils/SkPictureUtils.h"
#include "ui/gfx/skia_util.h"

namespace cc {

DrawingDisplayItem::DrawingDisplayItem() : DisplayItem(DRAWING) {}

DrawingDisplayItem::DrawingDisplayItem(sk_sp<const PaintRecord> record)
    : DisplayItem(DRAWING) {
  SetNew(std::move(record));
}

DrawingDisplayItem::DrawingDisplayItem(const DrawingDisplayItem& item)
    : DisplayItem(DRAWING) {
  item.CloneTo(this);
}

DrawingDisplayItem::~DrawingDisplayItem() {
}

void DrawingDisplayItem::SetNew(sk_sp<const PaintRecord> record) {
  picture_ = std::move(record);
}

DISABLE_CFI_PERF
void DrawingDisplayItem::Raster(SkCanvas* canvas,
                                SkPicture::AbortCallback* callback) const {
  if (canvas->quickReject(picture_->cullRect()))
    return;

  // SkPicture always does a wrapping save/restore on the canvas, so it is not
  // necessary here.
  if (callback) {
    picture_->playback(canvas, callback);
  } else {
    // TODO(enne): switch this to playback once PaintRecord is real.
    canvas->drawPicture(ToSkPicture(picture_.get()));
  }
}

void DrawingDisplayItem::AsValueInto(
    const gfx::Rect& visual_rect,
    base::trace_event::TracedValue* array) const {
  array->BeginDictionary();
  array->SetString("name", "DrawingDisplayItem");

  array->BeginArray("visualRect");
  array->AppendInteger(visual_rect.x());
  array->AppendInteger(visual_rect.y());
  array->AppendInteger(visual_rect.width());
  array->AppendInteger(visual_rect.height());
  array->EndArray();

  array->BeginArray("cullRect");
  array->AppendInteger(picture_->cullRect().x());
  array->AppendInteger(picture_->cullRect().y());
  array->AppendInteger(picture_->cullRect().width());
  array->AppendInteger(picture_->cullRect().height());
  array->EndArray();

  std::string b64_picture;
  PictureDebugUtil::SerializeAsBase64(ToSkPicture(picture_.get()),
                                      &b64_picture);
  array->SetString("skp64", b64_picture);
  array->EndDictionary();
}

void DrawingDisplayItem::CloneTo(DrawingDisplayItem* item) const {
  item->SetNew(picture_);
}

size_t DrawingDisplayItem::ExternalMemoryUsage() const {
  return picture_->approximateBytesUsed();
}

DISABLE_CFI_PERF
int DrawingDisplayItem::ApproximateOpCount() const {
  return picture_->approximateOpCount();
}

}  // namespace cc
