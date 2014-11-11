// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_PICTURE_PILE_H_
#define CC_RESOURCES_PICTURE_PILE_H_

#include "base/memory/ref_counted.h"
#include "cc/resources/picture_pile_base.h"
#include "cc/resources/recording_source.h"

namespace cc {
class ContentLayerClient;
class PicturePileImpl;
class Region;
class RenderingStatsInstrumentation;

class CC_EXPORT PicturePile : public PicturePileBase, public RecordingSource {
 public:
  PicturePile();
  ~PicturePile() override;

  bool UpdateAndExpandInvalidation(
      ContentLayerClient* painter,
      Region* invalidation,
      SkColor background_color,
      bool contents_opaque,
      bool contents_fill_bounds_completely,
      const gfx::Size& layer_size,
      const gfx::Rect& visible_layer_rect,
      int frame_number,
      Picture::RecordingMode recording_mode) override;
  gfx::Size GetSize() const override;
  void SetEmptyBounds() override;
  void SetMinContentsScale(float min_contents_scale) override;
  void SetTileGridSize(const gfx::Size& tile_grid_size) override;
  void SetSlowdownRasterScaleFactor(int factor) override;
  void SetShowDebugPictureBorders(bool show) override;
  void SetIsMask(bool is_mask) override;
  bool IsSuitableForGpuRasterization() const override;
  scoped_refptr<RasterSource> CreateRasterSource() const override;
  void SetUnsuitableForGpuRasterizationForTesting() override;
  SkTileGridFactory::TileGridInfo GetTileGridInfoForTesting() const override;

  void SetPixelRecordDistanceForTesting(int d) { pixel_record_distance_ = d; }

 protected:
  // An internal CanRaster check that goes to the picture_map rather than
  // using the recorded_viewport hint.
  bool CanRasterSlowTileCheck(const gfx::Rect& layer_rect) const;

 private:
  friend class PicturePileImpl;

  void DetermineIfSolidColor();

  bool is_suitable_for_gpu_rasterization_;
  int pixel_record_distance_;

  DISALLOW_COPY_AND_ASSIGN(PicturePile);
};

}  // namespace cc

#endif  // CC_RESOURCES_PICTURE_PILE_H_
