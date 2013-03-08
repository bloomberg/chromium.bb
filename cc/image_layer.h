// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IMAGE_LAYER_H_
#define CC_IMAGE_LAYER_H_

#include "cc/cc_export.h"
#include "cc/content_layer.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cc {

class ImageLayerUpdater;

// A Layer that contains only an Image element.
class CC_EXPORT ImageLayer : public TiledLayer {
 public:
  static scoped_refptr<ImageLayer> Create();

  virtual bool drawsContent() const OVERRIDE;
  virtual void setTexturePriorities(const PriorityCalculator& priority_calc)
      OVERRIDE;
  virtual void update(ResourceUpdateQueue& queue,
                      const OcclusionTracker* occlusion,
                      RenderingStats* stats) OVERRIDE;
  virtual void calculateContentsScale(float ideal_contents_scale,
                                      bool animating_transform_to_screen,
                                      float* contents_scale_x,
                                      float* contents_scale_y,
                                      gfx::Size* contentBounds) OVERRIDE;

  void SetBitmap(const SkBitmap& image);

 private:
  ImageLayer();
  virtual ~ImageLayer();

  virtual LayerUpdater* updater() const OVERRIDE;
  virtual void createUpdaterIfNeeded() OVERRIDE;
  float ImageContentsScaleX() const;
  float ImageContentsScaleY() const;

  SkBitmap bitmap_;

  scoped_refptr<ImageLayerUpdater> updater_;
};

}  // namespace cc

#endif  // CC_IMAGE_LAYER_H_
