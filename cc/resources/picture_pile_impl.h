// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_PICTURE_PILE_IMPL_H_
#define CC_RESOURCES_PICTURE_PILE_IMPL_H_

#include <list>
#include <map>
#include <vector>

#include "base/time.h"
#include "cc/base/cc_export.h"
#include "cc/resources/picture_pile_base.h"
#include "skia/ext/analysis_canvas.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace cc {
struct RenderingStats;

class CC_EXPORT PicturePileImpl : public PicturePileBase {
 public:
  static scoped_refptr<PicturePileImpl> Create(bool enable_lcd_text);
  static scoped_refptr<PicturePileImpl> CreateFromOther(
      const PicturePileBase* other,
      bool enable_lcd_text);

  // Get paint-safe version of this picture for a specific thread.
  PicturePileImpl* GetCloneForDrawingOnThread(unsigned thread_index) const;

  struct CC_EXPORT RasterStats {
    // Minimum rasterize time from N runs
    // N=max(1,slow-down-raster-scale-factor)
    base::TimeDelta best_rasterize_time;
    // Total rasterize time for all N runs
    base::TimeDelta total_rasterize_time;
    // Total number of pixels rasterize in all N runs
    int64 total_pixels_rasterized;
  };

  // Raster a subrect of this PicturePileImpl into the given canvas.
  // It's only safe to call paint on a cloned version.  It is assumed
  // that contents_scale has already been applied to this canvas.
  // Writes the total number of pixels rasterized and the time spent
  // rasterizing to the stats if the respective pointer is not
  // NULL. When slow-down-raster-scale-factor is set to a value
  // greater than 1, the reported rasterize time is the minimum
  // measured value over all runs.
  void Raster(
      SkCanvas* canvas,
      gfx::Rect canvas_rect,
      float contents_scale,
      RasterStats* raster_stats);

  skia::RefPtr<SkPicture> GetFlattenedPicture();

  struct CC_EXPORT Analysis {
    Analysis();
    ~Analysis();

    bool is_solid_color;
    bool has_text;
    SkColor solid_color;
  };

  void AnalyzeInRect(gfx::Rect content_rect,
                     float contents_scale,
                     Analysis* analysis);

  class CC_EXPORT PixelRefIterator {
   public:
    PixelRefIterator(gfx::Rect content_rect,
                     float contents_scale,
                     const PicturePileImpl* picture_pile);
    ~PixelRefIterator();

    skia::LazyPixelRef* operator->() const { return *pixel_ref_iterator_; }
    skia::LazyPixelRef* operator*() const { return *pixel_ref_iterator_; }
    PixelRefIterator& operator++();
    operator bool() const { return pixel_ref_iterator_; }

   private:
    bool AdvanceToTileWithPictures();
    void AdvanceToPictureWithPixelRefs();

    const PicturePileImpl* picture_pile_;
    gfx::Rect layer_rect_;
    TilingData::Iterator tile_iterator_;
    Picture::PixelRefIterator pixel_ref_iterator_;
    const PictureList* picture_list_;
    PictureList::const_iterator picture_list_iterator_;
  };

 protected:
  friend class PicturePile;
  friend class PixelRefIterator;

  explicit PicturePileImpl(bool enable_lcd_text);
  PicturePileImpl(const PicturePileBase* other, bool enable_lcd_text);
  virtual ~PicturePileImpl();

 private:
  class ClonesForDrawing {
   public:
    ClonesForDrawing(const PicturePileImpl* pile, int num_threads);
    ~ClonesForDrawing();

    typedef std::vector<scoped_refptr<PicturePileImpl> > PicturePileVector;
    PicturePileVector clones_;
  };

  static scoped_refptr<PicturePileImpl> CreateCloneForDrawing(
      const PicturePileImpl* other, unsigned thread_index);

  PicturePileImpl(const PicturePileImpl* other, unsigned thread_index);

  bool enable_lcd_text_;

  // Once instantiated, |clones_for_drawing_| can't be modified.  This
  // guarantees thread-safe access during the life time of a PicturePileImpl
  // instance.  This member variable must be last so that other member
  // variables have already been initialized and can be clonable.
  const ClonesForDrawing clones_for_drawing_;

  DISALLOW_COPY_AND_ASSIGN(PicturePileImpl);
};

}  // namespace cc

#endif  // CC_RESOURCES_PICTURE_PILE_IMPL_H_
