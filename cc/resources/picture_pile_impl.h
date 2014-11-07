// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_PICTURE_PILE_IMPL_H_
#define CC_RESOURCES_PICTURE_PILE_IMPL_H_

#include <list>
#include <map>
#include <set>
#include <vector>

#include "base/time/time.h"
#include "cc/base/cc_export.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/resources/picture_pile_base.h"
#include "cc/resources/raster_source.h"
#include "skia/ext/analysis_canvas.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace cc {

// TODO(vmpstr): Clean up PicturePileBase and make it a member.
class CC_EXPORT PicturePileImpl : public RasterSource {
 public:
  static scoped_refptr<PicturePileImpl> Create();
  static scoped_refptr<PicturePileImpl> CreateFromOther(
      const PicturePileBase* other);

  // RasterSource overrides. See RasterSource header for full description.
  // When slow-down-raster-scale-factor is set to a value greater than 1, the
  // reported rasterize time (in stats_instrumentation) is the minimum measured
  // value over all runs.
  void PlaybackToCanvas(SkCanvas* canvas,
                        const gfx::Rect& canvas_rect,
                        float contents_scale) const override;
  void PerformSolidColorAnalysis(
      const gfx::Rect& content_rect,
      float contents_scale,
      RasterSource::SolidColorAnalysis* analysis) const override;
  void GatherPixelRefs(const gfx::Rect& content_rect,
                       float contents_scale,
                       std::vector<SkPixelRef*>* pixel_refs) const override;
  bool CoversRect(const gfx::Rect& content_rect,
                  float contents_scale) const override;
  bool SuitableForDistanceFieldText() const override;

  // Raster into the canvas without applying clips.
  void RasterDirect(SkCanvas* canvas,
                    const gfx::Rect& canvas_rect,
                    float contents_scale) const;

  // Tracing functionality.
  void DidBeginTracing();
  skia::RefPtr<SkPicture> GetFlattenedPicture();

  void set_likely_to_be_used_for_transform_animation() {
    likely_to_be_used_for_transform_animation_ = true;
  }

  gfx::Size tiling_size() const { return tiling_.tiling_size(); }
  bool is_solid_color() const { return is_solid_color_; }
  SkColor solid_color() const { return solid_color_; }
  // If this pile contains any valid recordings. May have false positives.
  bool HasRecordings() const { return has_any_recordings_; }
  void AsValueInto(base::debug::TracedValue* array) const;
  bool is_mask() const { return is_mask_; }

  // Iterator used to return SkPixelRefs from this picture pile.
  // Public for testing.
  class CC_EXPORT PixelRefIterator {
   public:
    PixelRefIterator(const gfx::Rect& content_rect,
                     float contents_scale,
                     const PicturePileImpl* picture_pile);
    ~PixelRefIterator();

    SkPixelRef* operator->() const { return *pixel_ref_iterator_; }
    SkPixelRef* operator*() const { return *pixel_ref_iterator_; }
    PixelRefIterator& operator++();
    operator bool() const { return pixel_ref_iterator_; }

   private:
    void AdvanceToTilePictureWithPixelRefs();

    const PicturePileImpl* picture_pile_;
    gfx::Rect layer_rect_;
    TilingData::Iterator tile_iterator_;
    Picture::PixelRefIterator pixel_ref_iterator_;
    std::set<const void*> processed_pictures_;
  };

 protected:
  friend class PicturePile;
  friend class PixelRefIterator;

  // TODO(vmpstr): Change this when pictures are split from invalidation info,
  // and when PicturePileBase goes away.
  using PictureMapKey = PicturePileBase::PictureMapKey;
  using PictureMap = PicturePileBase::PictureMap;
  using PictureInfo = PicturePileBase::PictureInfo;

  PicturePileImpl();
  explicit PicturePileImpl(const PicturePileBase* other);
  ~PicturePileImpl() override;

  int buffer_pixels() const { return tiling_.border_texels(); }

  PictureMap picture_map_;
  TilingData tiling_;
  SkColor background_color_;
  bool contents_opaque_;
  bool contents_fill_bounds_completely_;
  bool is_solid_color_;
  SkColor solid_color_;
  gfx::Rect recorded_viewport_;
  bool has_any_recordings_;
  bool is_mask_;
  bool clear_canvas_with_debug_color_;
  float min_contents_scale_;
  int slow_down_raster_scale_factor_for_debug_;

 private:
  typedef std::map<const Picture*, Region> PictureRegionMap;

  // Called when analyzing a tile. We can use AnalysisCanvas as
  // SkDrawPictureCallback, which allows us to early out from analysis.
  void RasterForAnalysis(skia::AnalysisCanvas* canvas,
                         const gfx::Rect& canvas_rect,
                         float contents_scale) const;

  void CoalesceRasters(const gfx::Rect& canvas_rect,
                       const gfx::Rect& content_rect,
                       float contents_scale,
                       PictureRegionMap* result) const;

  void RasterCommon(
      SkCanvas* canvas,
      SkDrawPictureCallback* callback,
      const gfx::Rect& canvas_rect,
      float contents_scale,
      bool is_analysis) const;

  // An internal CanRaster check that goes to the picture_map rather than
  // using the recorded_viewport hint.
  bool CanRasterSlowTileCheck(const gfx::Rect& layer_rect) const;

  gfx::Rect PaddedRect(const PictureMapKey& key) const;

  bool likely_to_be_used_for_transform_animation_;

  DISALLOW_COPY_AND_ASSIGN(PicturePileImpl);
};

}  // namespace cc

#endif  // CC_RESOURCES_PICTURE_PILE_IMPL_H_
