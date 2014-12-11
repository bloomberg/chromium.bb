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
      can_use_lcd_text_(true),
      is_solid_color_(false),
      solid_color_(SK_ColorTRANSPARENT),
      pixel_record_distance_(kPixelDistanceToRecord),
      is_suitable_for_gpu_rasterization_(true) {
}

DisplayListRecordingSource::~DisplayListRecordingSource() {
}

bool DisplayListRecordingSource::UpdateAndExpandInvalidation(
    ContentLayerClient* painter,
    Region* invalidation,
    bool can_use_lcd_text,
    const gfx::Size& layer_size,
    const gfx::Rect& visible_layer_rect,
    int frame_number,
    Picture::RecordingMode recording_mode) {
  bool updated = false;

  if (size_ != layer_size) {
    size_ = layer_size;
    updated = true;
  }

  if (can_use_lcd_text_ != can_use_lcd_text) {
    can_use_lcd_text_ = can_use_lcd_text;
    invalidation->Union(gfx::Rect(GetSize()));
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

  // TODO(ajuma): Does repeating this way really makes sense with display lists?
  // With Blink caching recordings, repeated calls will not cause re-recording.
  int repeat_count = std::max(1, slow_down_raster_scale_factor_for_debug_);
  for (int i = 0; i < repeat_count; ++i) {
    display_list_ = painter->PaintContentsToDisplayList(
        recorded_viewport_, ContentLayerClient::GRAPHICS_CONTEXT_ENABLED);
  }
  is_suitable_for_gpu_rasterization_ =
      display_list_->IsSuitableForGpuRasterization();

  DetermineIfSolidColor();
  display_list_->EmitTraceSnapshot();
  return true;
}

gfx::Size DisplayListRecordingSource::GetSize() const {
  return size_;
}

void DisplayListRecordingSource::SetEmptyBounds() {
  size_ = gfx::Size();
  Clear();
}

void DisplayListRecordingSource::SetMinContentsScale(float min_contents_scale) {
}

void DisplayListRecordingSource::SetTileGridSize(
    const gfx::Size& tile_grid_size) {
}

void DisplayListRecordingSource::SetSlowdownRasterScaleFactor(int factor) {
  slow_down_raster_scale_factor_for_debug_ = factor;
}

void DisplayListRecordingSource::SetUnsuitableForGpuRasterizationForTesting() {
  is_suitable_for_gpu_rasterization_ = false;
}

bool DisplayListRecordingSource::IsSuitableForGpuRasterization() const {
  return is_suitable_for_gpu_rasterization_;
}

scoped_refptr<RasterSource> DisplayListRecordingSource::CreateRasterSource()
    const {
  return scoped_refptr<RasterSource>(
      DisplayListRasterSource::CreateFromDisplayListRecordingSource(this));
}

SkTileGridFactory::TileGridInfo
DisplayListRecordingSource::GetTileGridInfoForTesting() const {
  return SkTileGridFactory::TileGridInfo();
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
