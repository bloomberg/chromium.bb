// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/raster_source.h"

#include <stddef.h>

#include "base/trace_event/trace_event.h"
#include "cc/base/math_util.h"
#include "cc/base/region.h"
#include "cc/debug/debug_colors.h"
#include "cc/debug/traced_value.h"
#include "cc/paint/display_item_list.h"
#include "cc/raster/image_hijack_canvas.h"
#include "cc/raster/skip_image_canvas.h"
#include "skia/ext/analysis_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpaceXformCanvas.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {

RasterSource::RasterSource(const RecordingSource* other)
    : display_list_(other->display_list_),
      painter_reported_memory_usage_(other->painter_reported_memory_usage_),
      background_color_(other->background_color_),
      requires_clear_(other->requires_clear_),
      is_solid_color_(other->is_solid_color_),
      solid_color_(other->solid_color_),
      recorded_viewport_(other->recorded_viewport_),
      size_(other->size_),
      clear_canvas_with_debug_color_(other->clear_canvas_with_debug_color_),
      slow_down_raster_scale_factor_for_debug_(
          other->slow_down_raster_scale_factor_for_debug_),
      image_decode_cache_(nullptr) {}
RasterSource::~RasterSource() = default;

void RasterSource::PlaybackToCanvas(
    SkCanvas* raster_canvas,
    const gfx::ColorSpace& target_color_space,
    const gfx::Rect& canvas_bitmap_rect,
    const gfx::Rect& canvas_playback_rect,
    const gfx::AxisTransform2d& raster_transform,
    const PlaybackSettings& settings) const {
  SkIRect raster_bounds = gfx::RectToSkIRect(canvas_bitmap_rect);
  if (!canvas_playback_rect.IsEmpty() &&
      !raster_bounds.intersect(gfx::RectToSkIRect(canvas_playback_rect)))
    return;
  // Treat all subnormal values as zero for performance.
  ScopedSubnormalFloatDisabler disabler;

  raster_canvas->save();
  raster_canvas->translate(-canvas_bitmap_rect.x(), -canvas_bitmap_rect.y());
  raster_canvas->clipRect(SkRect::MakeFromIRect(raster_bounds));
  raster_canvas->translate(raster_transform.translation().x(),
                           raster_transform.translation().y());
  raster_canvas->scale(raster_transform.scale(), raster_transform.scale());
  PlaybackToCanvas(raster_canvas, target_color_space, settings);
  raster_canvas->restore();
}

void RasterSource::PlaybackToCanvas(SkCanvas* input_canvas,
                                    const gfx::ColorSpace& target_color_space,
                                    const PlaybackSettings& settings) const {
  SkCanvas* raster_canvas = input_canvas;
  std::unique_ptr<SkCanvas> color_transform_canvas;
  if (target_color_space.IsValid()) {
    color_transform_canvas = SkCreateColorSpaceXformCanvas(
        input_canvas, target_color_space.ToSkColorSpace());
    raster_canvas = color_transform_canvas.get();
  }

  if (!settings.playback_to_shared_canvas)
    PrepareForPlaybackToCanvas(raster_canvas);

  if (settings.skip_images) {
    SkipImageCanvas canvas(raster_canvas);
    RasterCommon(&canvas);
  } else if (settings.use_image_hijack_canvas) {
    const SkImageInfo& info = raster_canvas->imageInfo();
    ImageHijackCanvas canvas(info.width(), info.height(), image_decode_cache_,
                             &settings.images_to_skip, target_color_space);
    // Before adding the canvas, make sure that the ImageHijackCanvas is aware
    // of the current transform and clip, which may affect the clip bounds.
    // Since we query the clip bounds of the current canvas to get the list of
    // draw commands to process, this is important to produce correct content.
    canvas.clipRect(
        SkRect::MakeFromIRect(raster_canvas->getDeviceClipBounds()));
    canvas.setMatrix(raster_canvas->getTotalMatrix());
    canvas.addCanvas(raster_canvas);

    RasterCommon(&canvas);
  } else {
    RasterCommon(raster_canvas);
  }
}

namespace {

bool CanvasIsUnclipped(const SkCanvas* canvas) {
  if (!canvas->isClipRect())
    return false;

  SkIRect bounds;
  if (!canvas->getDeviceClipBounds(&bounds))
    return false;

  SkISize size = canvas->getBaseLayerSize();
  return bounds.contains(0, 0, size.width(), size.height());
}

}  // namespace

void RasterSource::PrepareForPlaybackToCanvas(SkCanvas* canvas) const {
  // TODO(hendrikw): See if we can split this up into separate functions.

  if (CanvasIsUnclipped(canvas))
    canvas->discard();

  // If this raster source has opaque contents, it is guaranteeing that it will
  // draw an opaque rect the size of the layer.  If it is not, then we must
  // clear this canvas ourselves.
  if (requires_clear_) {
    canvas->clear(SK_ColorTRANSPARENT);
    return;
  }

  if (clear_canvas_with_debug_color_)
    canvas->clear(DebugColors::NonPaintedFillColor());

  // If the canvas wants us to raster with complex transform, it is hard to
  // determine the exact region we must clear. Just clear everything.
  // TODO(trchen): Optimize the common case that transformed content bounds
  //               covers the whole clip region.
  if (!canvas->getTotalMatrix().rectStaysRect()) {
    canvas->clear(SK_ColorTRANSPARENT);
    return;
  }

  SkRect content_device_rect;
  canvas->getTotalMatrix().mapRect(
      &content_device_rect, SkRect::MakeWH(size_.width(), size_.height()));

  // The final texel of content may only be partially covered by a
  // rasterization; this rect represents the content rect that is fully
  // covered by content.
  SkIRect opaque_rect;
  content_device_rect.roundIn(&opaque_rect);

  if (opaque_rect.contains(canvas->getDeviceClipBounds()))
    return;

  // Even if completely covered, for rasterizations that touch the edge of the
  // layer, we also need to raster the background color underneath the last
  // texel (since the recording won't cover it) and outside the last texel
  // (due to linear filtering when using this texture).
  SkIRect interest_rect;
  content_device_rect.roundOut(&interest_rect);
  interest_rect.outset(1, 1);

  if (clear_canvas_with_debug_color_) {
    // Any non-painted areas outside of the content bounds are left in
    // this color.  If this is seen then it means that cc neglected to
    // rerasterize a tile that used to intersect with the content rect
    // after the content bounds grew.
    canvas->save();
    // Use clipRegion to bypass CTM because the rects are device rects.
    SkRegion interest_region;
    interest_region.setRect(interest_rect);
    canvas->clipRegion(interest_region, SkClipOp::kDifference);
    canvas->clear(DebugColors::MissingResizeInvalidations());
    canvas->restore();
  }

  // Drawing at most 2 x 2 x (canvas width + canvas height) texels is 2-3X
  // faster than clearing, so special case this.
  canvas->save();
  // Use clipRegion to bypass CTM because the rects are device rects.
  SkRegion interest_region;
  interest_region.setRect(interest_rect);
  interest_region.op(opaque_rect, SkRegion::kDifference_Op);
  canvas->clipRegion(interest_region);
  canvas->clear(background_color_);
  canvas->restore();
}

void RasterSource::RasterCommon(SkCanvas* raster_canvas,
                                SkPicture::AbortCallback* callback) const {
  DCHECK(display_list_.get());
  int repeat_count = std::max(1, slow_down_raster_scale_factor_for_debug_);
  for (int i = 0; i < repeat_count; ++i)
    display_list_->Raster(raster_canvas, callback);
}

sk_sp<SkPicture> RasterSource::GetFlattenedPicture() {
  TRACE_EVENT0("cc", "RasterSource::GetFlattenedPicture");

  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(size_.width(), size_.height());
  if (!size_.IsEmpty()) {
    PrepareForPlaybackToCanvas(canvas);
    RasterCommon(canvas);
  }

  return recorder.finishRecordingAsPicture();
}

size_t RasterSource::GetMemoryUsage() const {
  if (!display_list_)
    return 0;
  return display_list_->BytesUsed() + painter_reported_memory_usage_;
}

bool RasterSource::PerformSolidColorAnalysis(gfx::Rect layer_rect,
                                             SkColor* color) const {
  TRACE_EVENT0("cc", "RasterSource::PerformSolidColorAnalysis");

  layer_rect.Intersect(gfx::Rect(size_));
  return display_list_->GetColorIfSolidInRect(layer_rect, color);
}

void RasterSource::GetDiscardableImagesInRect(
    const gfx::Rect& layer_rect,
    float contents_scale,
    const gfx::ColorSpace& target_color_space,
    std::vector<DrawImage>* images) const {
  DCHECK_EQ(0u, images->size());
  display_list_->discardable_image_map().GetDiscardableImagesInRect(
      layer_rect, contents_scale, target_color_space, images);
}

gfx::Rect RasterSource::GetRectForImage(PaintImage::Id image_id) const {
  if (!display_list_)
    return gfx::Rect();
  return display_list_->discardable_image_map().GetRectForImage(image_id);
}

bool RasterSource::CoversRect(const gfx::Rect& layer_rect) const {
  if (size_.IsEmpty())
    return false;
  gfx::Rect bounded_rect = layer_rect;
  bounded_rect.Intersect(gfx::Rect(size_));
  return recorded_viewport_.Contains(bounded_rect);
}

gfx::Size RasterSource::GetSize() const {
  return size_;
}

bool RasterSource::IsSolidColor() const {
  return is_solid_color_;
}

SkColor RasterSource::GetSolidColor() const {
  DCHECK(IsSolidColor());
  return solid_color_;
}

bool RasterSource::HasRecordings() const {
  return !!display_list_.get();
}

gfx::Rect RasterSource::RecordedViewport() const {
  return recorded_viewport_;
}

void RasterSource::AsValueInto(base::trace_event::TracedValue* array) const {
  if (display_list_.get())
    TracedValue::AppendIDRef(display_list_.get(), array);
}

void RasterSource::DidBeginTracing() {
  if (display_list_.get())
    display_list_->EmitTraceSnapshot();
}

RasterSource::PlaybackSettings::PlaybackSettings()
    : playback_to_shared_canvas(false),
      skip_images(false),
      use_image_hijack_canvas(true),
      use_lcd_text(true) {}

RasterSource::PlaybackSettings::PlaybackSettings(const PlaybackSettings&) =
    default;

RasterSource::PlaybackSettings::PlaybackSettings(PlaybackSettings&&) = default;

RasterSource::PlaybackSettings::~PlaybackSettings() = default;

}  // namespace cc
