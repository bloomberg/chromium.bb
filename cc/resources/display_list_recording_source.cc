// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/display_list_recording_source.h"

#include <algorithm>

#include "cc/base/region.h"
#include "cc/layers/content_layer_client.h"
#include "cc/resources/display_item_list.h"
#include "cc/resources/display_list_raster_source.h"
#include "skia/ext/analysis_canvas.h"

namespace {

// Layout pixel buffer around the visible layer rect to record.  Any base
// picture that intersects the visible layer rect expanded by this distance
// will be recorded.
const int kPixelDistanceToRecord = 8000;
// We don't perform solid color analysis on images that have more than 10 skia
// operations.
const int kOpCountThatIsOkToAnalyze = 10;

}  // namespace

namespace cc {

DisplayListRecordingSource::DisplayListRecordingSource()
    : slow_down_raster_scale_factor_for_debug_(0),
      requires_clear_(false),
      is_solid_color_(false),
      solid_color_(SK_ColorTRANSPARENT),
      background_color_(SK_ColorTRANSPARENT),
      pixel_record_distance_(kPixelDistanceToRecord),
      is_suitable_for_gpu_rasterization_(true) {
}

DisplayListRecordingSource::~DisplayListRecordingSource() {
}

bool DisplayListRecordingSource::UpdateAndExpandInvalidation(
    ContentLayerClient* painter,
    Region* invalidation,
    const gfx::Size& layer_size,
    const gfx::Rect& visible_layer_rect,
    int frame_number,
    RecordingMode recording_mode) {
  bool updated = false;

  if (size_ != layer_size) {
    size_ = layer_size;
    updated = true;
  }

  gfx::Rect old_recorded_viewport = recorded_viewport_;
  recorded_viewport_ = visible_layer_rect;
  recorded_viewport_.Inset(-pixel_record_distance_, -pixel_record_distance_);
  recorded_viewport_.Intersect(gfx::Rect(GetSize()));

  if (recorded_viewport_ != old_recorded_viewport) {
    // Invalidate newly-exposed and no-longer-exposed areas.
    Region newly_exposed_region(recorded_viewport_);
    newly_exposed_region.Subtract(old_recorded_viewport);
    invalidation->Union(newly_exposed_region);

    Region no_longer_exposed_region(old_recorded_viewport);
    no_longer_exposed_region.Subtract(recorded_viewport_);
    invalidation->Union(no_longer_exposed_region);

    updated = true;
  }

  if (!updated && !invalidation->Intersects(recorded_viewport_))
    return false;

  ContentLayerClient::PaintingControlSetting painting_control =
      ContentLayerClient::PAINTING_BEHAVIOR_NORMAL;

  switch (recording_mode) {
    case RECORD_NORMALLY:
      // Already setup for normal recording.
      break;
    case RECORD_WITH_SK_NULL_CANVAS:
    // TODO(schenney): Remove this when DisplayList recording is the only
    // option. For now, fall through and disable construction.
    case RECORD_WITH_PAINTING_DISABLED:
      painting_control = ContentLayerClient::DISPLAY_LIST_CONSTRUCTION_DISABLED;
      break;
    case RECORD_WITH_CACHING_DISABLED:
      painting_control = ContentLayerClient::DISPLAY_LIST_CACHING_DISABLED;
      break;
    default:
      NOTREACHED();
  }

  int repeat_count = 1;
  if (slow_down_raster_scale_factor_for_debug_ > 1) {
    repeat_count = slow_down_raster_scale_factor_for_debug_;
    if (painting_control !=
        ContentLayerClient::DISPLAY_LIST_CONSTRUCTION_DISABLED) {
      painting_control = ContentLayerClient::DISPLAY_LIST_CACHING_DISABLED;
    }
  }
  for (int i = 0; i < repeat_count; ++i) {
    display_list_ = painter->PaintContentsToDisplayList(recorded_viewport_,
                                                        painting_control);
  }
  display_list_->set_layer_rect(recorded_viewport_);
  is_suitable_for_gpu_rasterization_ =
      display_list_->IsSuitableForGpuRasterization();

  DetermineIfSolidColor();
  display_list_->EmitTraceSnapshot();

  display_list_->CreateAndCacheSkPicture();

  return true;
}

void DisplayListRecordingSource::DidMoveToNewCompositor() {
  // No invalidation history to worry about here.
}

gfx::Size DisplayListRecordingSource::GetSize() const {
  return size_;
}

void DisplayListRecordingSource::SetEmptyBounds() {
  size_ = gfx::Size();
  Clear();
}

void DisplayListRecordingSource::SetSlowdownRasterScaleFactor(int factor) {
  slow_down_raster_scale_factor_for_debug_ = factor;
}

void DisplayListRecordingSource::SetBackgroundColor(SkColor background_color) {
  background_color_ = background_color;
}

void DisplayListRecordingSource::SetRequiresClear(bool requires_clear) {
  requires_clear_ = requires_clear;
}

void DisplayListRecordingSource::SetUnsuitableForGpuRasterizationForTesting() {
  is_suitable_for_gpu_rasterization_ = false;
}

bool DisplayListRecordingSource::IsSuitableForGpuRasterization() const {
  return is_suitable_for_gpu_rasterization_;
}

scoped_refptr<RasterSource> DisplayListRecordingSource::CreateRasterSource(
    bool can_use_lcd_text) const {
  return scoped_refptr<RasterSource>(
      DisplayListRasterSource::CreateFromDisplayListRecordingSource(
          this, can_use_lcd_text));
}

gfx::Size DisplayListRecordingSource::GetTileGridSizeForTesting() const {
  return gfx::Size();
}

void DisplayListRecordingSource::DetermineIfSolidColor() {
  DCHECK(display_list_.get());
  is_solid_color_ = false;
  solid_color_ = SK_ColorTRANSPARENT;

  if (display_list_->ApproximateOpCount() > kOpCountThatIsOkToAnalyze)
    return;

  gfx::Size layer_size = GetSize();
  skia::AnalysisCanvas canvas(layer_size.width(), layer_size.height());
  display_list_->Raster(&canvas, nullptr, 1.f);
  is_solid_color_ = canvas.GetColorIfSolid(&solid_color_);
}

void DisplayListRecordingSource::Clear() {
  recorded_viewport_ = gfx::Rect();
  display_list_ = NULL;
  is_solid_color_ = false;
}

}  // namespace cc
