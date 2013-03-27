// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_PICTURE_PILE_H_
#define CC_RESOURCES_PICTURE_PILE_H_

#include "cc/resources/picture_pile_base.h"
#include "ui/gfx/rect.h"

namespace cc {
class PicturePileImpl;
class Region;
class RenderingStatsInstrumentation;

class CC_EXPORT PicturePile : public PicturePileBase {
 public:
  PicturePile();

  // Re-record parts of the picture that are invalid.
  // Invalidations are in layer space.
  void Update(
      ContentLayerClient* painter,
      SkColor background_color,
      const Region& invalidation,
      gfx::Rect visible_layer_rect,
      RenderingStatsInstrumentation* stats_instrumentation);

  void set_num_raster_threads(int num_raster_threads) {
    num_raster_threads_ = num_raster_threads;
  }

  void set_slow_down_raster_scale_factor(int factor) {
    slow_down_raster_scale_factor_for_debug_ = factor;
  }

 private:
  virtual ~PicturePile();
  friend class PicturePileImpl;

  // Add an invalidation to this picture list.  If the list needs to be
  // entirely recreated, leave it empty.  Do not call this on an empty list.
  void InvalidateRect(
      PictureList& picture_list,
      gfx::Rect invalidation);

  DISALLOW_COPY_AND_ASSIGN(PicturePile);
};

}  // namespace cc

#endif  // CC_RESOURCES_PICTURE_PILE_H_
