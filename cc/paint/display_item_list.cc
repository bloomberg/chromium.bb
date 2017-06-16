// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/display_item_list.h"

#include <stddef.h>

#include <string>

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/base/math_util.h"
#include "cc/base/render_surface_filters.h"
#include "cc/debug/picture_debug_util.h"
#include "cc/paint/discardable_image_store.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace cc {

namespace {

// We don't perform per-layer solid color analysis when there are too many skia
// operations.
const int kOpCountThatIsOkToAnalyze = 10;

bool GetCanvasClipBounds(SkCanvas* canvas, gfx::Rect* clip_bounds) {
  SkRect canvas_clip_bounds;
  if (!canvas->getLocalClipBounds(&canvas_clip_bounds))
    return false;
  *clip_bounds = ToEnclosingRect(gfx::SkRectToRectF(canvas_clip_bounds));
  return true;
}

}  // namespace

DisplayItemList::DisplayItemList() {
  visual_rects_.reserve(1024);
  begin_paired_indices_.reserve(32);
}

DisplayItemList::~DisplayItemList() = default;

void DisplayItemList::Raster(SkCanvas* canvas,
                             SkPicture::AbortCallback* callback) const {
  gfx::Rect canvas_playback_rect;
  if (!GetCanvasClipBounds(canvas, &canvas_playback_rect))
    return;

  std::vector<size_t> indices = rtree_.Search(canvas_playback_rect);
  paint_op_buffer_.Playback(canvas, callback, &indices);
}

void DisplayItemList::GrowCurrentBeginItemVisualRect(
    const gfx::Rect& visual_rect) {
  if (!begin_paired_indices_.empty())
    visual_rects_[begin_paired_indices_.back().first].Union(visual_rect);
}

void DisplayItemList::Finalize() {
  TRACE_EVENT0("cc", "DisplayItemList::Finalize");
  // If this fails a call to StartPaint() was not ended.
  DCHECK(!in_painting_);
  // If this fails we had more calls to EndPaintOfPairedBegin() than
  // to EndPaintOfPairedEnd().
  DCHECK_EQ(0, in_paired_begin_count_);

  paint_op_buffer_.ShrinkToFit();
  rtree_.Build(visual_rects_);

  if (!retain_visual_rects_)
    visual_rects_.clear();
  visual_rects_.shrink_to_fit();
  begin_paired_indices_.shrink_to_fit();
}

size_t DisplayItemList::BytesUsed() const {
  // TODO(jbroman): Does anything else owned by this class substantially
  // contribute to memory usage?
  // TODO(vmpstr): Probably DiscardableImageMap is worth counting here.
  return sizeof(*this) + paint_op_buffer_.bytes_used();
}

bool DisplayItemList::ShouldBeAnalyzedForSolidColor() const {
  return op_count() <= kOpCountThatIsOkToAnalyze;
}

void DisplayItemList::EmitTraceSnapshot() const {
  bool include_items;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.display_items"), &include_items);
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.display_items") ","
      TRACE_DISABLED_BY_DEFAULT("cc.debug.picture") ","
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.picture"),
      "cc::DisplayItemList", this, CreateTracedValue(include_items));
}

std::unique_ptr<base::trace_event::TracedValue>
DisplayItemList::CreateTracedValue(bool include_items) const {
  auto state = base::MakeUnique<base::trace_event::TracedValue>();
  state->BeginDictionary("params");

  if (include_items) {
    state->BeginArray("items");

    for (size_t i = 0; i < paint_op_buffer_.size(); ++i) {
      gfx::Rect visual_rect = visual_rects_[i];

      state->BeginDictionary();
      state->BeginArray("visualRect");
      state->AppendInteger(visual_rect.x());
      state->AppendInteger(visual_rect.y());
      state->AppendInteger(visual_rect.width());
      state->AppendInteger(visual_rect.height());
      state->EndArray();

      SkPictureRecorder recorder;
      SkCanvas* canvas =
          recorder.beginRecording(gfx::RectToSkRect(rtree_.GetBounds()));
      std::vector<size_t> indices{i};
      paint_op_buffer_.Playback(canvas, nullptr, &indices);
      sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();

      std::string b64_picture;
      PictureDebugUtil::SerializeAsBase64(picture.get(), &b64_picture);
      state->SetString("skp64", b64_picture);
      state->EndDictionary();
    }

    state->EndArray();  // "items".
  }

  MathUtil::AddToTracedValue("layer_rect", rtree_.GetBounds(), state.get());
  state->EndDictionary();  // "params".

  {
    SkPictureRecorder recorder;
    gfx::Rect bounds = rtree_.GetBounds();
    SkCanvas* canvas = recorder.beginRecording(gfx::RectToSkRect(bounds));
    canvas->translate(-bounds.x(), -bounds.y());
    canvas->clipRect(gfx::RectToSkRect(bounds));
    Raster(canvas);
    sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();

    std::string b64_picture;
    PictureDebugUtil::SerializeAsBase64(picture.get(), &b64_picture);
    state->SetString("skp64", b64_picture);
  }
  return state;
}

void DisplayItemList::GenerateDiscardableImagesMetadata() {
  // This should be only called once.
  DCHECK(image_map_.empty());
  if (!paint_op_buffer_.HasDiscardableImages())
    return;

  gfx::Rect bounds = rtree_.GetBounds();
  DiscardableImageMap::ScopedMetadataGenerator generator(
      &image_map_, gfx::Size(bounds.right(), bounds.bottom()));
  GatherDiscardableImages(generator.image_store());
}

void DisplayItemList::GatherDiscardableImages(
    DiscardableImageStore* image_store) const {
  image_store->GatherDiscardableImages(&paint_op_buffer_);
}

void DisplayItemList::GetDiscardableImagesInRect(
    const gfx::Rect& rect,
    float contents_scale,
    const gfx::ColorSpace& target_color_space,
    std::vector<DrawImage>* images) {
  image_map_.GetDiscardableImagesInRect(rect, contents_scale,
                                        target_color_space, images);
}

gfx::Rect DisplayItemList::GetRectForImage(PaintImage::Id image_id) const {
  return image_map_.GetRectForImage(image_id);
}

void DisplayItemList::Reset() {
  DCHECK(!in_painting_);
  DCHECK_EQ(0, in_paired_begin_count_);

  rtree_.Reset();
  image_map_.Reset();
  paint_op_buffer_.Reset();
  visual_rects_.clear();
  begin_paired_indices_.clear();
  current_range_start_ = 0;
  in_paired_begin_count_ = 0;
  in_painting_ = false;
  op_count_ = 0u;
}

sk_sp<PaintRecord> DisplayItemList::ReleaseAsRecord() {
  sk_sp<PaintRecord> record =
      sk_make_sp<PaintOpBuffer>(std::move(paint_op_buffer_));

  Reset();
  return record;
}

}  // namespace cc
