// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_DISPLAY_LIST_RASTER_SOURCE_H_
#define CC_RESOURCES_DISPLAY_LIST_RASTER_SOURCE_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/resources/display_list_recording_source.h"
#include "cc/resources/raster_source.h"
#include "skia/ext/analysis_canvas.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace cc {
class DisplayItemList;

class CC_EXPORT DisplayListRasterSource : public RasterSource {
 public:
  static scoped_refptr<DisplayListRasterSource> Create();
  static scoped_refptr<DisplayListRasterSource>
  CreateFromDisplayListRecordingSource(const DisplayListRecordingSource* other);

  // RasterSource overrides.
  void PlaybackToCanvas(SkCanvas* canvas,
                        const gfx::Rect& canvas_rect,
                        float contents_scale) const override;
  void PlaybackToSharedCanvas(SkCanvas* canvas,
                              const gfx::Rect& canvas_rect,
                              float contents_scale) const override;
  void PerformSolidColorAnalysis(
      const gfx::Rect& content_rect,
      float contents_scale,
      RasterSource::SolidColorAnalysis* analysis) const override;
  bool IsSolidColor() const override;
  SkColor GetSolidColor() const override;
  gfx::Size GetSize() const override;
  void GatherPixelRefs(const gfx::Rect& content_rect,
                       float contents_scale,
                       std::vector<SkPixelRef*>* pixel_refs) const override;
  bool CoversRect(const gfx::Rect& content_rect,
                  float contents_scale) const override;
  bool HasRecordings() const override;
  void SetShouldAttemptToUseDistanceFieldText() override;
  void SetBackgoundColor(SkColor background_color) override;
  void SetRequiresClear(bool requires_clear) override;
  bool ShouldAttemptToUseDistanceFieldText() const override;
  void DidBeginTracing() override;
  void AsValueInto(base::debug::TracedValue* array) const override;
  skia::RefPtr<SkPicture> GetFlattenedPicture() override;
  size_t GetPictureMemoryUsage() const override;
  bool CanUseLCDText() const override;

 protected:
  DisplayListRasterSource();
  explicit DisplayListRasterSource(const DisplayListRecordingSource* other);
  ~DisplayListRasterSource() override;

  scoped_refptr<DisplayItemList> display_list_;
  SkColor background_color_;
  bool requires_clear_;
  bool can_use_lcd_text_;
  bool is_solid_color_;
  SkColor solid_color_;
  gfx::Rect recorded_viewport_;
  gfx::Size size_;
  bool clear_canvas_with_debug_color_;
  int slow_down_raster_scale_factor_for_debug_;

 private:
  // Called when analyzing a tile. We can use AnalysisCanvas as
  // SkDrawPictureCallback, which allows us to early out from analysis.
  void RasterForAnalysis(skia::AnalysisCanvas* canvas,
                         const gfx::Rect& canvas_rect,
                         float contents_scale) const;

  void RasterCommon(SkCanvas* canvas,
                    SkDrawPictureCallback* callback,
                    const gfx::Rect& canvas_rect,
                    float contents_scale,
                    bool is_analysis) const;

  bool should_attempt_to_use_distance_field_text_;

  DISALLOW_COPY_AND_ASSIGN(DisplayListRasterSource);
};

}  // namespace cc

#endif  // CC_RESOURCES_DISPLAY_LIST_RASTER_SOURCE_H_
