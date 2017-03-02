// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/display_item_list.h"

#include <stddef.h>

#include <string>

#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/base/math_util.h"
#include "cc/debug/picture_debug_util.h"
#include "cc/debug/traced_display_item_list.h"
#include "cc/debug/traced_value.h"
#include "cc/playback/clip_display_item.h"
#include "cc/playback/clip_path_display_item.h"
#include "cc/playback/compositing_display_item.h"
#include "cc/playback/drawing_display_item.h"
#include "cc/playback/filter_display_item.h"
#include "cc/playback/float_clip_display_item.h"
#include "cc/playback/largest_display_item.h"
#include "cc/playback/transform_display_item.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/utils/SkPictureUtils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace cc {

namespace {

// We don't perform per-layer solid color analysis when there are too many skia
// operations.
const int kOpCountThatIsOkToAnalyze = 10;

bool DisplayItemsTracingEnabled() {
  bool tracing_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.display_items"), &tracing_enabled);
  return tracing_enabled;
}

bool GetCanvasClipBounds(SkCanvas* canvas, gfx::Rect* clip_bounds) {
  SkRect canvas_clip_bounds;
  if (!canvas->getLocalClipBounds(&canvas_clip_bounds))
    return false;
  *clip_bounds = ToEnclosingRect(gfx::SkRectToRectF(canvas_clip_bounds));
  return true;
}

const int kDefaultNumDisplayItemsToReserve = 100;

}  // namespace

DisplayItemList::Inputs::Inputs()
    : items(LargestDisplayItemSize(),
            LargestDisplayItemSize() * kDefaultNumDisplayItemsToReserve) {}

DisplayItemList::Inputs::~Inputs() = default;

DisplayItemList::DisplayItemList() = default;

DisplayItemList::~DisplayItemList() = default;

void DisplayItemList::Raster(SkCanvas* canvas,
                             SkPicture::AbortCallback* callback,
                             const gfx::Rect& canvas_target_playback_rect,
                             float contents_scale) const {
  canvas->save();
  if (!canvas_target_playback_rect.IsEmpty()) {
    // canvas_target_playback_rect is specified in device space. We can't
    // use clipRect because canvas CTM will be applied on it. Use clipRegion
    // instead because it ignores canvas CTM.
    SkRegion device_clip;
    device_clip.setRect(gfx::RectToSkIRect(canvas_target_playback_rect));
    canvas->clipRegion(device_clip);
  }
  canvas->scale(contents_scale, contents_scale);
  Raster(canvas, callback);
  canvas->restore();
}

DISABLE_CFI_PERF
void DisplayItemList::Raster(SkCanvas* canvas,
                             SkPicture::AbortCallback* callback) const {
  gfx::Rect canvas_playback_rect;
  if (!GetCanvasClipBounds(canvas, &canvas_playback_rect))
    return;

  std::vector<size_t> indices;
  rtree_.Search(canvas_playback_rect, &indices);
  for (size_t index : indices) {
    inputs_.items[index].Raster(canvas, callback);
    // We use a callback during solid color analysis on the compositor thread to
    // break out early. Since we're handling a sequence of pictures via rtree
    // query results ourselves, we have to respect the callback and early out.
    if (callback && callback->abort())
      break;
  }
}

void DisplayItemList::GrowCurrentBeginItemVisualRect(
    const gfx::Rect& visual_rect) {
  if (!inputs_.begin_item_indices.empty())
    inputs_.visual_rects[inputs_.begin_item_indices.back()].Union(visual_rect);
}

void DisplayItemList::Finalize() {
  TRACE_EVENT0("cc", "DisplayItemList::Finalize");
  DCHECK(inputs_.items.size() == inputs_.visual_rects.size())
      << "items.size() " << inputs_.items.size() << " visual_rects.size() "
      << inputs_.visual_rects.size();
  rtree_.Build(inputs_.visual_rects);

  if (!retain_visual_rects_)
    // This clears both the vector and the vector's capacity, since
    // visual_rects won't be used anymore.
    std::vector<gfx::Rect>().swap(inputs_.visual_rects);
}

bool DisplayItemList::IsSuitableForGpuRasterization() const {
  // TODO(wkorman): This is more permissive than Picture's implementation, since
  // none of the items might individually trigger a veto even though they
  // collectively have enough "bad" operations that a corresponding Picture
  // would get vetoed. See crbug.com/513016.
  return inputs_.all_items_are_suitable_for_gpu_rasterization;
}

int DisplayItemList::ApproximateOpCount() const {
  return approximate_op_count_;
}

size_t DisplayItemList::ApproximateMemoryUsage() const {
  size_t memory_usage = sizeof(*this);

  size_t external_memory_usage = 0;
  for (const auto& item : inputs_.items) {
    size_t bytes = 0;
    switch (item.type()) {
      case DisplayItem::CLIP:
        bytes = static_cast<const ClipDisplayItem&>(item).ExternalMemoryUsage();
        break;
      case DisplayItem::CLIP_PATH:
        bytes =
            static_cast<const ClipPathDisplayItem&>(item).ExternalMemoryUsage();
        break;
      case DisplayItem::COMPOSITING:
        bytes = static_cast<const CompositingDisplayItem&>(item)
                    .ExternalMemoryUsage();
        break;
      case DisplayItem::DRAWING:
        bytes =
            static_cast<const DrawingDisplayItem&>(item).ExternalMemoryUsage();
        break;
      case DisplayItem::FLOAT_CLIP:
        bytes = static_cast<const FloatClipDisplayItem&>(item)
                    .ExternalMemoryUsage();
        break;
      case DisplayItem::FILTER:
        bytes =
            static_cast<const FilterDisplayItem&>(item).ExternalMemoryUsage();
        break;
      case DisplayItem::TRANSFORM:
        bytes = static_cast<const TransformDisplayItem&>(item)
                    .ExternalMemoryUsage();
        break;
      case DisplayItem::END_CLIP:
      case DisplayItem::END_CLIP_PATH:
      case DisplayItem::END_COMPOSITING:
      case DisplayItem::END_FLOAT_CLIP:
      case DisplayItem::END_FILTER:
      case DisplayItem::END_TRANSFORM:
        break;
    }
    external_memory_usage += bytes;
  }

  // Memory outside this class due to |items_|.
  memory_usage += inputs_.items.GetCapacityInBytes() + external_memory_usage;

  // TODO(jbroman): Does anything else owned by this class substantially
  // contribute to memory usage?
  // TODO(vmpstr): Probably DiscardableImageMap is worth counting here.

  return memory_usage;
}

bool DisplayItemList::ShouldBeAnalyzedForSolidColor() const {
  return ApproximateOpCount() <= kOpCountThatIsOkToAnalyze;
}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
DisplayItemList::AsValue(bool include_items) const {
  std::unique_ptr<base::trace_event::TracedValue> state(
      new base::trace_event::TracedValue());

  state->BeginDictionary("params");
  if (include_items) {
    state->BeginArray("items");
    size_t item_index = 0;
    for (const DisplayItem& item : inputs_.items) {
      item.AsValueInto(item_index < inputs_.visual_rects.size()
                           ? inputs_.visual_rects[item_index]
                           : gfx::Rect(),
                       state.get());
      item_index++;
    }
    state->EndArray();  // "items".
  }
  MathUtil::AddToTracedValue("layer_rect", rtree_.GetBounds(), state.get());
  state->EndDictionary();  // "params".

  SkPictureRecorder recorder;
  gfx::Rect bounds = rtree_.GetBounds();
  SkCanvas* canvas = recorder.beginRecording(bounds.width(), bounds.height());
  canvas->translate(-bounds.x(), -bounds.y());
  canvas->clipRect(gfx::RectToSkRect(bounds));
  Raster(canvas, nullptr, gfx::Rect(), 1.f);
  sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();

  std::string b64_picture;
  PictureDebugUtil::SerializeAsBase64(picture.get(), &b64_picture);
  state->SetString("skp64", b64_picture);

  return std::move(state);
}

void DisplayItemList::EmitTraceSnapshot() const {
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.display_items") ","
      TRACE_DISABLED_BY_DEFAULT("cc.debug.picture") ","
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.picture"),
      "cc::DisplayItemList", this,
      TracedDisplayItemList::AsTraceableDisplayItemList(this,
          DisplayItemsTracingEnabled()));
}

void DisplayItemList::GenerateDiscardableImagesMetadata() {
  // This should be only called once.
  DCHECK(image_map_.empty());

  gfx::Rect bounds = rtree_.GetBounds();
  DiscardableImageMap::ScopedMetadataGenerator generator(
      &image_map_, gfx::Size(bounds.right(), bounds.bottom()));
  Raster(generator.canvas(), nullptr, gfx::Rect(), 1.f);
}

void DisplayItemList::GetDiscardableImagesInRect(
    const gfx::Rect& rect,
    float contents_scale,
    std::vector<DrawImage>* images) {
  image_map_.GetDiscardableImagesInRect(rect, contents_scale, images);
}

gfx::Rect DisplayItemList::GetRectForImage(ImageId image_id) const {
  return image_map_.GetRectForImage(image_id);
}

}  // namespace cc
