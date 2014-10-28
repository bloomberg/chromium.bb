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
class CC_EXPORT PicturePileImpl : public PicturePileBase, public RasterSource {
 public:
  static scoped_refptr<PicturePileImpl> Create();
  static scoped_refptr<PicturePileImpl> CreateFromOther(
      const PicturePileBase* other);

  // RasterSource overrides. See RasterSource header for full description.
  // When slow-down-raster-scale-factor is set to a value greater than 1, the
  // reported rasterize time (in stats_instrumentation) is the minimum measured
  // value over all runs.
  void PlaybackToCanvas(
      SkCanvas* canvas,
      const gfx::Rect& canvas_rect,
      float contents_scale,
      RenderingStatsInstrumentation* stats_instrumentation) const override;
  void PerformSolidColorAnalysis(
      const gfx::Rect& content_rect,
      float contents_scale,
      RasterSource::SolidColorAnalysis* analysis,
      RenderingStatsInstrumentation* stats_instrumentation) const override;
  void GatherPixelRefs(const gfx::Rect& content_rect,
                       float contents_scale,
                       std::vector<SkPixelRef*>* pixel_refs) const override;
  bool CoversRect(const gfx::Rect& content_rect,
                  float contents_scale) const override;
  bool SuitableForDistanceFieldText() const override;

  // Raster into the canvas without applying clips.
  void RasterDirect(
      SkCanvas* canvas,
      const gfx::Rect& canvas_rect,
      float contents_scale,
      RenderingStatsInstrumentation* rendering_stats_instrumentation) const;

  // Tracing functionality.
  void DidBeginTracing();
  skia::RefPtr<SkPicture> GetFlattenedPicture();

  void set_likely_to_be_used_for_transform_animation() {
    likely_to_be_used_for_transform_animation_ = true;
  }

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

  PicturePileImpl();
  explicit PicturePileImpl(const PicturePileBase* other);
  ~PicturePileImpl() override;

 private:
  typedef std::map<const Picture*, Region> PictureRegionMap;

  // Called when analyzing a tile. We can use AnalysisCanvas as
  // SkDrawPictureCallback, which allows us to early out from analysis.
  void RasterForAnalysis(
      skia::AnalysisCanvas* canvas,
      const gfx::Rect& canvas_rect,
      float contents_scale,
      RenderingStatsInstrumentation* stats_instrumentation) const;

  void CoalesceRasters(const gfx::Rect& canvas_rect,
                       const gfx::Rect& content_rect,
                       float contents_scale,
                       PictureRegionMap* result) const;

  void RasterCommon(
      SkCanvas* canvas,
      SkDrawPictureCallback* callback,
      const gfx::Rect& canvas_rect,
      float contents_scale,
      RenderingStatsInstrumentation* rendering_stats_instrumentation,
      bool is_analysis) const;

  bool likely_to_be_used_for_transform_animation_;

  DISALLOW_COPY_AND_ASSIGN(PicturePileImpl);
};

}  // namespace cc

#endif  // CC_RESOURCES_PICTURE_PILE_IMPL_H_
