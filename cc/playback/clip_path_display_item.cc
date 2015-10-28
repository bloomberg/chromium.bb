// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/clip_path_display_item.h"

#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/proto/display_item.pb.h"
#include "cc/proto/skia_conversions.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace cc {

ClipPathDisplayItem::ClipPathDisplayItem() {
}

ClipPathDisplayItem::~ClipPathDisplayItem() {
}

void ClipPathDisplayItem::SetNew(const SkPath& clip_path,
                                 SkRegion::Op clip_op,
                                 bool antialias) {
  clip_path_ = clip_path;
  clip_op_ = clip_op;
  antialias_ = antialias;

  // The size of SkPath's external storage is not currently accounted for (and
  // may well be shared anyway).
  DisplayItem::SetNew(true /* suitable_for_gpu_raster */, 1 /* op_count */,
                      0 /* external_memory_usage */);
}

void ClipPathDisplayItem::ToProtobuf(proto::DisplayItem* proto) const {
  proto->set_type(proto::DisplayItem::Type_ClipPath);

  proto::ClipPathDisplayItem* details = proto->mutable_clip_path_item();
  details->set_clip_op(SkRegionOpToProto(clip_op_));
  details->set_antialias(antialias_);

  // Just use skia's serialization method for the SkPath for now.
  size_t path_size = clip_path_.writeToMemory(nullptr);
  if (path_size > 0) {
    scoped_ptr<char[]> buffer(new char[path_size]);
    clip_path_.writeToMemory(buffer.get());
    details->set_clip_path(std::string(buffer.get(), path_size));
  }
}

void ClipPathDisplayItem::FromProtobuf(const proto::DisplayItem& proto) {
  DCHECK_EQ(proto::DisplayItem::Type_ClipPath, proto.type());

  const proto::ClipPathDisplayItem& details = proto.clip_path_item();
  SkRegion::Op clip_op = SkRegionOpFromProto(details.clip_op());
  bool antialias = details.antialias();

  SkPath clip_path;
  if (details.has_clip_path()) {
    size_t bytes_read = clip_path.readFromMemory(details.clip_path().c_str(),
                                                 details.clip_path().size());
    DCHECK_EQ(details.clip_path().size(), bytes_read);
  }

  SetNew(clip_path, clip_op, antialias);
}

void ClipPathDisplayItem::Raster(SkCanvas* canvas,
                                 const gfx::Rect& canvas_target_playback_rect,
                                 SkPicture::AbortCallback* callback) const {
  canvas->save();
  canvas->clipPath(clip_path_, clip_op_, antialias_);
}

void ClipPathDisplayItem::AsValueInto(
    base::trace_event::TracedValue* array) const {
  array->AppendString(base::StringPrintf("ClipPathDisplayItem length: %d",
                                         clip_path_.countPoints()));
}

EndClipPathDisplayItem::EndClipPathDisplayItem() {
  DisplayItem::SetNew(true /* suitable_for_gpu_raster */, 0 /* op_count */,
                      0 /* external_memory_usage */);
}

EndClipPathDisplayItem::~EndClipPathDisplayItem() {
}

void EndClipPathDisplayItem::ToProtobuf(proto::DisplayItem* proto) const {
  proto->set_type(proto::DisplayItem::Type_EndClipPath);
}

void EndClipPathDisplayItem::FromProtobuf(const proto::DisplayItem& proto) {
  DCHECK_EQ(proto::DisplayItem::Type_EndClipPath, proto.type());
}

void EndClipPathDisplayItem::Raster(
    SkCanvas* canvas,
    const gfx::Rect& canvas_target_playback_rect,
    SkPicture::AbortCallback* callback) const {
  canvas->restore();
}

void EndClipPathDisplayItem::AsValueInto(
    base::trace_event::TracedValue* array) const {
  array->AppendString("EndClipPathDisplayItem");
}

}  // namespace cc
