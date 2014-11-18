// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_PICTURE_PILE_H_
#define CC_RESOURCES_PICTURE_PILE_H_

#include <bitset>
#include <utility>

#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "cc/base/tiling_data.h"
#include "cc/resources/recording_source.h"

namespace cc {
class PicturePileImpl;

class CC_EXPORT PicturePile : public RecordingSource {
 public:
  PicturePile();
  ~PicturePile() override;

  // RecordingSource overrides.
  bool UpdateAndExpandInvalidation(
      ContentLayerClient* painter,
      Region* invalidation,
      bool can_use_lcd_text,
      const gfx::Size& layer_size,
      const gfx::Rect& visible_layer_rect,
      int frame_number,
      Picture::RecordingMode recording_mode) override;
  scoped_refptr<RasterSource> CreateRasterSource() const override;
  gfx::Size GetSize() const final;
  void SetEmptyBounds() override;
  void SetMinContentsScale(float min_contents_scale) override;
  void SetSlowdownRasterScaleFactor(int factor) override;
  bool IsSuitableForGpuRasterization() const override;
  void SetTileGridSize(const gfx::Size& tile_grid_size) override;
  void SetUnsuitableForGpuRasterizationForTesting() override;
  SkTileGridFactory::TileGridInfo GetTileGridInfoForTesting() const override;

  static void ComputeTileGridInfo(const gfx::Size& tile_grid_size,
                                  SkTileGridFactory::TileGridInfo* info);

 protected:
  class CC_EXPORT PictureInfo {
   public:
    enum { INVALIDATION_FRAMES_TRACKED = 32 };

    PictureInfo();
    ~PictureInfo();

    bool Invalidate(int frame_number);
    bool NeedsRecording(int frame_number, int distance_to_visible);
    void SetPicture(scoped_refptr<Picture> picture);
    const Picture* GetPicture() const;

    float GetInvalidationFrequencyForTesting() const {
      return GetInvalidationFrequency();
    }

   private:
    void AdvanceInvalidationHistory(int frame_number);
    float GetInvalidationFrequency() const;

    int last_frame_number_;
    scoped_refptr<const Picture> picture_;
    std::bitset<INVALIDATION_FRAMES_TRACKED> invalidation_history_;
  };

  typedef std::pair<int, int> PictureMapKey;
  typedef base::hash_map<PictureMapKey, PictureInfo> PictureMap;

  // An internal CanRaster check that goes to the picture_map rather than
  // using the recorded_viewport hint.
  bool CanRasterSlowTileCheck(const gfx::Rect& layer_rect) const;

  void Clear();

  gfx::Rect PaddedRect(const PictureMapKey& key) const;
  gfx::Rect PadRect(const gfx::Rect& rect) const;

  int buffer_pixels() const { return tiling_.border_texels(); }

  // A picture pile is a tiled set of pictures. The picture map is a map of tile
  // indices to picture infos.
  PictureMap picture_map_;
  TilingData tiling_;

  // If non-empty, all pictures tiles inside this rect are recorded. There may
  // be recordings outside this rect, but everything inside the rect is
  // recorded.
  gfx::Rect recorded_viewport_;
  float min_contents_scale_;
  SkTileGridFactory::TileGridInfo tile_grid_info_;
  int slow_down_raster_scale_factor_for_debug_;
  bool can_use_lcd_text_;
  // A hint about whether there are any recordings. This may be a false
  // positive.
  bool has_any_recordings_;
  bool is_solid_color_;
  SkColor solid_color_;
  int pixel_record_distance_;

 private:
  friend class PicturePileImpl;

  void DetermineIfSolidColor();
  void SetBufferPixels(int buffer_pixels);

  bool is_suitable_for_gpu_rasterization_;

  DISALLOW_COPY_AND_ASSIGN(PicturePile);
};

}  // namespace cc

#endif  // CC_RESOURCES_PICTURE_PILE_H_
