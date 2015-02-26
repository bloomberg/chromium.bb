// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_DISPLAY_LIST_RECORDING_SOURCE_H_
#define CC_RESOURCES_DISPLAY_LIST_RECORDING_SOURCE_H_

#include "base/memory/scoped_ptr.h"
#include "cc/resources/recording_source.h"

namespace cc {
class DisplayItemList;
class DisplayListRasterSource;

class CC_EXPORT DisplayListRecordingSource : public RecordingSource {
 public:
  DisplayListRecordingSource();
  ~DisplayListRecordingSource() override;

  // RecordingSource overrides.
  bool UpdateAndExpandInvalidation(ContentLayerClient* painter,
                                   Region* invalidation,
                                   const gfx::Size& layer_size,
                                   const gfx::Rect& visible_layer_rect,
                                   int frame_number,
                                   RecordingMode recording_mode) override;
  void DidMoveToNewCompositor() override;
  scoped_refptr<RasterSource> CreateRasterSource(
      bool can_use_lcd_text) const override;
  gfx::Size GetSize() const final;
  void SetEmptyBounds() override;
  void SetSlowdownRasterScaleFactor(int factor) override;
  void SetBackgroundColor(SkColor background_color) override;
  void SetRequiresClear(bool requires_clear) override;
  bool IsSuitableForGpuRasterization() const override;
  void SetUnsuitableForGpuRasterizationForTesting() override;
  gfx::Size GetTileGridSizeForTesting() const override;

 protected:
  void Clear();

  gfx::Rect recorded_viewport_;
  gfx::Size size_;
  int slow_down_raster_scale_factor_for_debug_;
  bool requires_clear_;
  bool is_solid_color_;
  SkColor solid_color_;
  SkColor background_color_;
  int pixel_record_distance_;

  scoped_refptr<DisplayItemList> display_list_;

 private:
  friend class DisplayListRasterSource;

  void DetermineIfSolidColor();

  bool is_suitable_for_gpu_rasterization_;

  DISALLOW_COPY_AND_ASSIGN(DisplayListRecordingSource);
};

}  // namespace cc

#endif  // CC_RESOURCES_DISPLAY_LIST_RECORDING_SOURCE_H_
