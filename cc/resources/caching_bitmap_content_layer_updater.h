// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_CACHING_BITMAP_CONTENT_LAYER_UPDATER_H_
#define CC_RESOURCES_CACHING_BITMAP_CONTENT_LAYER_UPDATER_H_

#include "base/compiler_specific.h"
#include "cc/resources/bitmap_content_layer_updater.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cc {

class CachingBitmapContentLayerUpdater : public BitmapContentLayerUpdater {
 public:
  static scoped_refptr<CachingBitmapContentLayerUpdater> Create(
      scoped_ptr<LayerPainter>,
      RenderingStatsInstrumentation* stats_instrumentation);

  virtual void PrepareToUpdate(gfx::Rect content_rect,
                               gfx::Size tile_size,
                               float contents_width_scale,
                               float contents_height_scale,
                               gfx::Rect* resulting_opaque_rect) OVERRIDE;

  bool pixels_did_change() const {
    return pixels_did_change_;
  }

 private:
  CachingBitmapContentLayerUpdater(
      scoped_ptr<LayerPainter> painter,
      RenderingStatsInstrumentation* stats_instrumentation);
  virtual ~CachingBitmapContentLayerUpdater();

  bool pixels_did_change_;
  SkBitmap cached_bitmap_;

  DISALLOW_COPY_AND_ASSIGN(CachingBitmapContentLayerUpdater);
};

}  // namespace cc

#endif  // CC_RESOURCES_CACHING_BITMAP_CONTENT_LAYER_UPDATER_H_
