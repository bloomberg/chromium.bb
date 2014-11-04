// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RASTER_SOURCE_H_
#define CC_RESOURCES_RASTER_SOURCE_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "cc/base/cc_export.h"
#include "third_party/skia/include/core/SkColor.h"

class SkCanvas;

namespace cc {

class CC_EXPORT RasterSource : public base::RefCountedThreadSafe<RasterSource> {
 public:
  struct CC_EXPORT SolidColorAnalysis {
    SolidColorAnalysis()
        : is_solid_color(false), solid_color(SK_ColorTRANSPARENT) {}
    ~SolidColorAnalysis() {}

    bool is_solid_color;
    SkColor solid_color;
  };

  // Raster a subrect of this RasterSource into the given canvas. It is
  // assumed that contents_scale has already been applied to this canvas.
  // Writes the total number of pixels rasterized and the time spent
  // rasterizing to the stats if the respective pointer is not nullptr.
  virtual void PlaybackToCanvas(SkCanvas* canvas,
                                const gfx::Rect& canvas_rect,
                                float contents_scale) const = 0;

  // Analyze to determine if the given rect at given scale is of solid color in
  // this raster source.
  virtual void PerformSolidColorAnalysis(
      const gfx::Rect& content_rect,
      float contents_scale,
      SolidColorAnalysis* analysis) const = 0;

  // Populate the given list with all SkPixelRefs that may overlap the given
  // rect at given scale.
  virtual void GatherPixelRefs(const gfx::Rect& content_rect,
                               float contents_scale,
                               std::vector<SkPixelRef*>* pixel_refs) const = 0;

  // Return true iff this raster source can raster the given rect at given
  // scale.
  virtual bool CoversRect(const gfx::Rect& content_rect,
                          float contents_scale) const = 0;

  // Return true iff this raster source would benefit from using distance
  // field text.
  virtual bool SuitableForDistanceFieldText() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<RasterSource>;

  RasterSource() {}
  virtual ~RasterSource() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(RasterSource);
};

}  // namespace cc

#endif  // CC_RESOURCES_RASTER_SOURCE_H_
