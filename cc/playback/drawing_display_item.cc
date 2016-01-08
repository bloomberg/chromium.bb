// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/drawing_display_item.h"

#include <stddef.h>

#include <string>

#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event_argument.h"
#include "base/values.h"
#include "cc/debug/picture_debug_util.h"
#include "cc/proto/display_item.pb.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/utils/SkPictureUtils.h"
#include "ui/gfx/skia_util.h"

namespace cc {

DrawingDisplayItem::DrawingDisplayItem() {}

DrawingDisplayItem::DrawingDisplayItem(skia::RefPtr<const SkPicture> picture) {
  SetNew(std::move(picture));
}

DrawingDisplayItem::DrawingDisplayItem(const proto::DisplayItem& proto) {
  DCHECK_EQ(proto::DisplayItem::Type_Drawing, proto.type());

  skia::RefPtr<SkPicture> picture;
  const proto::DrawingDisplayItem& details = proto.drawing_item();
  if (details.has_picture()) {
    SkMemoryStream stream(details.picture().data(), details.picture().size());

    // TODO(dtrainor, nyquist): Add an image decoder.
    picture = skia::AdoptRef(SkPicture::CreateFromStream(&stream, nullptr));
  }

  SetNew(std::move(picture));
}

DrawingDisplayItem::DrawingDisplayItem(const DrawingDisplayItem& item) {
  item.CloneTo(this);
}

DrawingDisplayItem::~DrawingDisplayItem() {
}

void DrawingDisplayItem::SetNew(skia::RefPtr<const SkPicture> picture) {
  picture_ = std::move(picture);
}

void DrawingDisplayItem::ToProtobuf(proto::DisplayItem* proto) const {
  proto->set_type(proto::DisplayItem::Type_Drawing);

  proto::DrawingDisplayItem* details = proto->mutable_drawing_item();

  // Just use skia's serialize() method for now.
  if (picture_) {
    SkDynamicMemoryWStream stream;

    // TODO(dtrainor, nyquist): Add an SkPixelSerializer to not serialize images
    // more than once (crbug.com/548434).
    picture_->serialize(&stream, nullptr);
    if (stream.bytesWritten() > 0) {
      SkAutoDataUnref data(stream.copyToData());
      details->set_picture(data->data(), data->size());
    }
  }
}

void DrawingDisplayItem::Raster(SkCanvas* canvas,
                                const gfx::Rect& canvas_target_playback_rect,
                                SkPicture::AbortCallback* callback) const {
  // The canvas_playback_rect can be empty to signify no culling is desired.
  if (!canvas_target_playback_rect.IsEmpty()) {
    const SkMatrix& matrix = canvas->getTotalMatrix();
    const SkRect& cull_rect = picture_->cullRect();
    SkRect target_rect;
    matrix.mapRect(&target_rect, cull_rect);
    if (!target_rect.intersect(gfx::RectToSkRect(canvas_target_playback_rect)))
      return;
  }

  // SkPicture always does a wrapping save/restore on the canvas, so it is not
  // necessary here.
  if (callback)
    picture_->playback(canvas, callback);
  else
    canvas->drawPicture(picture_.get());
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
  PictureDebugUtil::SerializeAsBase64(picture_.get(), &b64_picture);
  array->SetString("skp64", b64_picture);
  array->EndDictionary();
}

void DrawingDisplayItem::CloneTo(DrawingDisplayItem* item) const {
  item->SetNew(picture_);
}

size_t DrawingDisplayItem::ExternalMemoryUsage() const {
  return SkPictureUtils::ApproximateBytesUsed(picture_.get());
}

int DrawingDisplayItem::ApproximateOpCount() const {
  return picture_->approximateOpCount();
}

bool DrawingDisplayItem::IsSuitableForGpuRasterization() const {
  return picture_->suitableForGpuRasterization(NULL);
}

}  // namespace cc
