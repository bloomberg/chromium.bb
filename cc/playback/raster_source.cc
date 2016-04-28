// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/raster_source.h"

#include <stddef.h>

#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/region.h"
#include "cc/debug/debug_colors.h"
#include "cc/playback/display_item_list.h"
#include "cc/playback/image_hijack_canvas.h"
#include "cc/playback/skip_image_canvas.h"
#include "skia/ext/analysis_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {

scoped_refptr<RasterSource> RasterSource::CreateFromRecordingSource(
    const RecordingSource* other,
    bool can_use_lcd_text) {
  return make_scoped_refptr(new RasterSource(other, can_use_lcd_text));
}

RasterSource::RasterSource(const RecordingSource* other, bool can_use_lcd_text)
    : display_list_(other->display_list_),
      painter_reported_memory_usage_(other->painter_reported_memory_usage_),
      background_color_(other->background_color_),
      requires_clear_(other->requires_clear_),
      can_use_lcd_text_(can_use_lcd_text),
      is_solid_color_(other->is_solid_color_),
      solid_color_(other->solid_color_),
      recorded_viewport_(other->recorded_viewport_),
      size_(other->size_),
      clear_canvas_with_debug_color_(other->clear_canvas_with_debug_color_),
      slow_down_raster_scale_factor_for_debug_(
          other->slow_down_raster_scale_factor_for_debug_),
      should_attempt_to_use_distance_field_text_(false),
      image_decode_controller_(nullptr) {
  // In certain cases, ThreadTaskRunnerHandle isn't set (Android Webview).
  // Don't register a dump provider in these cases.
  // TODO(ericrk): Get this working in Android Webview. crbug.com/517156
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "cc::RasterSource", base::ThreadTaskRunnerHandle::Get());
  }
}

RasterSource::RasterSource(const RasterSource* other, bool can_use_lcd_text)
    : display_list_(other->display_list_),
      painter_reported_memory_usage_(other->painter_reported_memory_usage_),
      background_color_(other->background_color_),
      requires_clear_(other->requires_clear_),
      can_use_lcd_text_(can_use_lcd_text),
      is_solid_color_(other->is_solid_color_),
      solid_color_(other->solid_color_),
      recorded_viewport_(other->recorded_viewport_),
      size_(other->size_),
      clear_canvas_with_debug_color_(other->clear_canvas_with_debug_color_),
      slow_down_raster_scale_factor_for_debug_(
          other->slow_down_raster_scale_factor_for_debug_),
      should_attempt_to_use_distance_field_text_(
          other->should_attempt_to_use_distance_field_text_),
      image_decode_controller_(other->image_decode_controller_) {
  // In certain cases, ThreadTaskRunnerHandle isn't set (Android Webview).
  // Don't register a dump provider in these cases.
  // TODO(ericrk): Get this working in Android Webview. crbug.com/517156
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "cc::RasterSource", base::ThreadTaskRunnerHandle::Get());
  }
}

RasterSource::~RasterSource() {
  // For MemoryDumpProvider deregistration to work correctly, this must happen
  // on the same thread that the RasterSource was created on.
  DCHECK(memory_dump_thread_checker_.CalledOnValidThread());
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

void RasterSource::PlaybackToCanvas(SkCanvas* raster_canvas,
                                    const gfx::Rect& canvas_bitmap_rect,
                                    const gfx::Rect& canvas_playback_rect,
                                    float contents_scale,
                                    const PlaybackSettings& settings) const {
  if (!settings.playback_to_shared_canvas) {
    PrepareForPlaybackToCanvas(raster_canvas, canvas_bitmap_rect,
                               canvas_playback_rect, contents_scale);
  }

  if (settings.skip_images) {
    SkipImageCanvas canvas(raster_canvas);
    RasterCommon(&canvas, nullptr, canvas_bitmap_rect, canvas_playback_rect,
                 contents_scale);
  } else if (settings.use_image_hijack_canvas) {
    const SkImageInfo& info = raster_canvas->imageInfo();

    ImageHijackCanvas canvas(info.width(), info.height(),
                             image_decode_controller_);
    // Before adding the canvas, make sure that the ImageHijackCanvas is aware
    // of the current transform, which may affect the clip bounds. Since we
    // query the clip bounds of the current canvas to get the list of draw
    // commands to process, this is important to produce correct content.
    canvas.setMatrix(raster_canvas->getTotalMatrix());
    canvas.addCanvas(raster_canvas);

    RasterCommon(&canvas, nullptr, canvas_bitmap_rect, canvas_playback_rect,
                 contents_scale);
  } else {
    RasterCommon(raster_canvas, nullptr, canvas_bitmap_rect,
                 canvas_playback_rect, contents_scale);
  }
}

void RasterSource::PrepareForPlaybackToCanvas(
    SkCanvas* canvas,
    const gfx::Rect& canvas_bitmap_rect,
    const gfx::Rect& canvas_playback_rect,
    float contents_scale) const {
  // TODO(hendrikw): See if we can split this up into separate functions.
  bool partial_update = canvas_bitmap_rect != canvas_playback_rect;

  if (!partial_update)
    canvas->discard();
  if (clear_canvas_with_debug_color_) {
    // Any non-painted areas in the content bounds will be left in this color.
    if (!partial_update) {
      canvas->clear(DebugColors::NonPaintedFillColor());
    } else {
      canvas->save();
      canvas->clipRect(gfx::RectToSkRect(
          canvas_playback_rect - canvas_bitmap_rect.OffsetFromOrigin()));
      canvas->drawColor(DebugColors::NonPaintedFillColor());
      canvas->restore();
    }
  }

  // If this raster source has opaque contents, it is guaranteeing that it will
  // draw an opaque rect the size of the layer.  If it is not, then we must
  // clear this canvas ourselves.
  if (requires_clear_) {
    TRACE_EVENT_INSTANT0("cc", "SkCanvas::clear", TRACE_EVENT_SCOPE_THREAD);
    // Clearing is about ~4x faster than drawing a rect even if the content
    // isn't covering a majority of the canvas.
    if (!partial_update) {
      canvas->clear(SK_ColorTRANSPARENT);
    } else {
      canvas->save();
      canvas->clipRect(gfx::RectToSkRect(
          canvas_playback_rect - canvas_bitmap_rect.OffsetFromOrigin()));
      canvas->drawColor(SK_ColorTRANSPARENT, SkXfermode::kClear_Mode);
      canvas->restore();
    }
  } else {
    // Even if completely covered, for rasterizations that touch the edge of the
    // layer, we also need to raster the background color underneath the last
    // texel (since the recording won't cover it) and outside the last texel
    // (due to linear filtering when using this texture).
    gfx::Rect content_rect =
        gfx::ScaleToEnclosingRect(gfx::Rect(size_), contents_scale);

    // The final texel of content may only be partially covered by a
    // rasterization; this rect represents the content rect that is fully
    // covered by content.
    gfx::Rect deflated_content_rect = content_rect;
    deflated_content_rect.Inset(0, 0, 1, 1);
    deflated_content_rect.Intersect(canvas_playback_rect);
    if (!deflated_content_rect.Contains(canvas_playback_rect)) {
      if (clear_canvas_with_debug_color_) {
        // Any non-painted areas outside of the content bounds are left in
        // this color.  If this is seen then it means that cc neglected to
        // rerasterize a tile that used to intersect with the content rect
        // after the content bounds grew.
        canvas->save();
        canvas->translate(-canvas_bitmap_rect.x(), -canvas_bitmap_rect.y());
        canvas->clipRect(gfx::RectToSkRect(content_rect),
                         SkRegion::kDifference_Op);
        canvas->drawColor(DebugColors::MissingResizeInvalidations(),
                          SkXfermode::kSrc_Mode);
        canvas->restore();
      }

      // Drawing at most 2 x 2 x (canvas width + canvas height) texels is 2-3X
      // faster than clearing, so special case this.
      canvas->save();
      canvas->translate(-canvas_bitmap_rect.x(), -canvas_bitmap_rect.y());
      gfx::Rect inflated_content_rect = content_rect;
      // Only clear edges that will be inside the canvas_playback_rect, else we
      // clear things that are still valid from a previous raster.
      inflated_content_rect.Inset(0, 0, -1, -1);
      inflated_content_rect.Intersect(canvas_playback_rect);
      canvas->clipRect(gfx::RectToSkRect(inflated_content_rect),
                       SkRegion::kReplace_Op);
      canvas->clipRect(gfx::RectToSkRect(deflated_content_rect),
                       SkRegion::kDifference_Op);
      canvas->drawColor(background_color_, SkXfermode::kSrc_Mode);
      canvas->restore();
    }
  }
}

void RasterSource::RasterCommon(SkCanvas* canvas,
                                SkPicture::AbortCallback* callback,
                                const gfx::Rect& canvas_bitmap_rect,
                                const gfx::Rect& canvas_playback_rect,
                                float contents_scale) const {
  canvas->translate(-canvas_bitmap_rect.x(), -canvas_bitmap_rect.y());
  gfx::Rect content_rect =
      gfx::ScaleToEnclosingRect(gfx::Rect(size_), contents_scale);
  content_rect.Intersect(canvas_playback_rect);

  canvas->clipRect(gfx::RectToSkRect(content_rect), SkRegion::kIntersect_Op);

  DCHECK(display_list_.get());
  gfx::Rect canvas_target_playback_rect =
      canvas_playback_rect - canvas_bitmap_rect.OffsetFromOrigin();
  int repeat_count = std::max(1, slow_down_raster_scale_factor_for_debug_);
  for (int i = 0; i < repeat_count; ++i) {
    display_list_->Raster(canvas, callback, canvas_target_playback_rect,
                          contents_scale);
  }
}

sk_sp<SkPicture> RasterSource::GetFlattenedPicture() {
  TRACE_EVENT0("cc", "RasterSource::GetFlattenedPicture");

  gfx::Rect display_list_rect(size_);
  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(display_list_rect.width(),
                                             display_list_rect.height());
  if (!display_list_rect.IsEmpty()) {
    PrepareForPlaybackToCanvas(canvas, display_list_rect, display_list_rect,
                               1.f);
    RasterCommon(canvas, nullptr, display_list_rect, display_list_rect, 1.f);
  }

  return recorder.finishRecordingAsPicture();
}

size_t RasterSource::GetPictureMemoryUsage() const {
  if (!display_list_)
    return 0;
  return display_list_->ApproximateMemoryUsage() +
         painter_reported_memory_usage_;
}

bool RasterSource::PerformSolidColorAnalysis(const gfx::Rect& content_rect,
                                             float contents_scale,
                                             SkColor* color) const {
  TRACE_EVENT0("cc", "RasterSource::PerformSolidColorAnalysis");

  gfx::Rect layer_rect =
      gfx::ScaleToEnclosingRect(content_rect, 1.0f / contents_scale);

  layer_rect.Intersect(gfx::Rect(size_));
  skia::AnalysisCanvas canvas(layer_rect.width(), layer_rect.height());
  RasterCommon(&canvas, &canvas, layer_rect, layer_rect, 1.0f);
  return canvas.GetColorIfSolid(color);
}

void RasterSource::GetDiscardableImagesInRect(
    const gfx::Rect& layer_rect,
    float raster_scale,
    std::vector<DrawImage>* images) const {
  DCHECK_EQ(0u, images->size());
  display_list_->GetDiscardableImagesInRect(layer_rect, raster_scale, images);
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

void RasterSource::SetShouldAttemptToUseDistanceFieldText() {
  should_attempt_to_use_distance_field_text_ = true;
}

bool RasterSource::ShouldAttemptToUseDistanceFieldText() const {
  return should_attempt_to_use_distance_field_text_;
}

void RasterSource::AsValueInto(base::trace_event::TracedValue* array) const {
  if (display_list_.get())
    TracedValue::AppendIDRef(display_list_.get(), array);
}

void RasterSource::DidBeginTracing() {
  if (display_list_.get())
    display_list_->EmitTraceSnapshot();
}

bool RasterSource::CanUseLCDText() const {
  return can_use_lcd_text_;
}

scoped_refptr<RasterSource> RasterSource::CreateCloneWithoutLCDText() const {
  bool can_use_lcd_text = false;
  return scoped_refptr<RasterSource>(new RasterSource(this, can_use_lcd_text));
}

void RasterSource::SetImageDecodeController(
    ImageDecodeController* image_decode_controller) {
  DCHECK(image_decode_controller);
  image_decode_controller_ = image_decode_controller;
}

bool RasterSource::OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                                base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK(memory_dump_thread_checker_.CalledOnValidThread());

  uint64_t memory_usage = GetPictureMemoryUsage();
  if (memory_usage > 0) {
    std::string dump_name =
        base::StringPrintf("cc/display_lists/raster_source_%p", this);
    base::trace_event::MemoryAllocatorDump* dump =
        pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    memory_usage);
  }
  return true;
}

RasterSource::PlaybackSettings::PlaybackSettings()
    : playback_to_shared_canvas(false),
      skip_images(false),
      use_image_hijack_canvas(true) {}

}  // namespace cc
