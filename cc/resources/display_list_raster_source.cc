// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/display_list_raster_source.h"

#include "base/trace_event/trace_event.h"
#include "cc/base/region.h"
#include "cc/debug/debug_colors.h"
#include "cc/resources/display_item_list.h"
#include "cc/resources/raster_source_helper.h"
#include "skia/ext/analysis_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace {

#ifdef NDEBUG
const bool kDefaultClearCanvasSetting = false;
#else
const bool kDefaultClearCanvasSetting = true;
#endif

}  // namespace

namespace cc {

scoped_refptr<DisplayListRasterSource>
DisplayListRasterSource::CreateFromDisplayListRecordingSource(
    const DisplayListRecordingSource* other,
    bool can_use_lcd_text) {
  return make_scoped_refptr(
      new DisplayListRasterSource(other, can_use_lcd_text));
}

DisplayListRasterSource::DisplayListRasterSource()
    : background_color_(SK_ColorTRANSPARENT),
      requires_clear_(true),
      can_use_lcd_text_(true),
      is_solid_color_(false),
      solid_color_(SK_ColorTRANSPARENT),
      clear_canvas_with_debug_color_(kDefaultClearCanvasSetting),
      slow_down_raster_scale_factor_for_debug_(0),
      should_attempt_to_use_distance_field_text_(false) {
}

DisplayListRasterSource::DisplayListRasterSource(
    const DisplayListRecordingSource* other,
    bool can_use_lcd_text)
    : display_list_(other->display_list_),
      background_color_(other->background_color_),
      requires_clear_(other->requires_clear_),
      can_use_lcd_text_(can_use_lcd_text),
      is_solid_color_(other->is_solid_color_),
      solid_color_(other->solid_color_),
      recorded_viewport_(other->recorded_viewport_),
      size_(other->size_),
      clear_canvas_with_debug_color_(kDefaultClearCanvasSetting),
      slow_down_raster_scale_factor_for_debug_(
          other->slow_down_raster_scale_factor_for_debug_),
      should_attempt_to_use_distance_field_text_(false) {
}

DisplayListRasterSource::DisplayListRasterSource(
    const DisplayListRasterSource* other,
    bool can_use_lcd_text)
    : display_list_(other->display_list_),
      background_color_(other->background_color_),
      requires_clear_(other->requires_clear_),
      can_use_lcd_text_(can_use_lcd_text),
      is_solid_color_(other->is_solid_color_),
      solid_color_(other->solid_color_),
      recorded_viewport_(other->recorded_viewport_),
      size_(other->size_),
      clear_canvas_with_debug_color_(kDefaultClearCanvasSetting),
      slow_down_raster_scale_factor_for_debug_(
          other->slow_down_raster_scale_factor_for_debug_),
      should_attempt_to_use_distance_field_text_(
          other->should_attempt_to_use_distance_field_text_) {
}

DisplayListRasterSource::~DisplayListRasterSource() {
}

void DisplayListRasterSource::PlaybackToSharedCanvas(
    SkCanvas* canvas,
    const gfx::Rect& canvas_rect,
    float contents_scale) const {
  RasterCommon(canvas, NULL, canvas_rect, contents_scale, false);
}

void DisplayListRasterSource::RasterForAnalysis(skia::AnalysisCanvas* canvas,
                                                const gfx::Rect& canvas_rect,
                                                float contents_scale) const {
  RasterCommon(canvas, canvas, canvas_rect, contents_scale, true);
}

void DisplayListRasterSource::PlaybackToCanvas(SkCanvas* canvas,
                                               const gfx::Rect& canvas_rect,
                                               float contents_scale) const {
  RasterSourceHelper::PrepareForPlaybackToCanvas(
      canvas, canvas_rect, gfx::Rect(size_), contents_scale, background_color_,
      clear_canvas_with_debug_color_, requires_clear_);

  RasterCommon(canvas, NULL, canvas_rect, contents_scale, false);
}

void DisplayListRasterSource::RasterCommon(SkCanvas* canvas,
                                           SkDrawPictureCallback* callback,
                                           const gfx::Rect& canvas_rect,
                                           float contents_scale,
                                           bool is_analysis) const {
  canvas->translate(-canvas_rect.x(), -canvas_rect.y());
  gfx::Rect content_rect =
      gfx::ToEnclosingRect(gfx::ScaleRect(gfx::Rect(size_), contents_scale));
  content_rect.Intersect(canvas_rect);

  canvas->clipRect(gfx::RectToSkRect(content_rect), SkRegion::kIntersect_Op);

  DCHECK(display_list_.get());
  display_list_->Raster(canvas, callback, contents_scale);
}

skia::RefPtr<SkPicture> DisplayListRasterSource::GetFlattenedPicture() {
  TRACE_EVENT0("cc", "DisplayListRasterSource::GetFlattenedPicture");

  gfx::Rect display_list_rect(size_);
  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(display_list_rect.width(),
                                             display_list_rect.height());
  if (!display_list_rect.IsEmpty())
    PlaybackToCanvas(canvas, display_list_rect, 1.0);
  skia::RefPtr<SkPicture> picture = skia::AdoptRef(recorder.endRecording());

  return picture;
}

size_t DisplayListRasterSource::GetPictureMemoryUsage() const {
  return display_list_->PictureMemoryUsage();
}

void DisplayListRasterSource::PerformSolidColorAnalysis(
    const gfx::Rect& content_rect,
    float contents_scale,
    RasterSource::SolidColorAnalysis* analysis) const {
  DCHECK(analysis);
  TRACE_EVENT0("cc", "DisplayListRasterSource::PerformSolidColorAnalysis");

  gfx::Rect layer_rect =
      gfx::ScaleToEnclosingRect(content_rect, 1.0f / contents_scale);

  layer_rect.Intersect(gfx::Rect(size_));
  skia::AnalysisCanvas canvas(layer_rect.width(), layer_rect.height());
  RasterForAnalysis(&canvas, layer_rect, 1.0f);
  analysis->is_solid_color = canvas.GetColorIfSolid(&analysis->solid_color);
}

void DisplayListRasterSource::GatherPixelRefs(
    const gfx::Rect& content_rect,
    float contents_scale,
    std::vector<SkPixelRef*>* pixel_refs) const {
  // TODO(ajuma): Implement this.
}

bool DisplayListRasterSource::CoversRect(const gfx::Rect& content_rect,
                                         float contents_scale) const {
  if (size_.IsEmpty())
    return false;
  gfx::Rect layer_rect =
      gfx::ScaleToEnclosingRect(content_rect, 1.f / contents_scale);
  layer_rect.Intersect(gfx::Rect(size_));

  return recorded_viewport_.Contains(layer_rect);
}

gfx::Size DisplayListRasterSource::GetSize() const {
  return size_;
}

bool DisplayListRasterSource::IsSolidColor() const {
  return is_solid_color_;
}

SkColor DisplayListRasterSource::GetSolidColor() const {
  DCHECK(IsSolidColor());
  return solid_color_;
}

bool DisplayListRasterSource::HasRecordings() const {
  return !!display_list_.get();
}

void DisplayListRasterSource::SetShouldAttemptToUseDistanceFieldText() {
  should_attempt_to_use_distance_field_text_ = true;
}

bool DisplayListRasterSource::ShouldAttemptToUseDistanceFieldText() const {
  return should_attempt_to_use_distance_field_text_;
}

void DisplayListRasterSource::AsValueInto(
    base::trace_event::TracedValue* array) const {
  if (display_list_.get())
    TracedValue::AppendIDRef(display_list_.get(), array);
}

void DisplayListRasterSource::DidBeginTracing() {
  if (display_list_.get())
    display_list_->EmitTraceSnapshot();
}

bool DisplayListRasterSource::CanUseLCDText() const {
  return can_use_lcd_text_;
}

scoped_refptr<RasterSource> DisplayListRasterSource::CreateCloneWithoutLCDText()
    const {
  bool can_use_lcd_text = false;
  return scoped_refptr<RasterSource>(
      new DisplayListRasterSource(this, can_use_lcd_text));
}

}  // namespace cc
