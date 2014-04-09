// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_PICTURE_IMAGE_LAYER_IMPL_H_
#define CC_LAYERS_PICTURE_IMAGE_LAYER_IMPL_H_

#include "cc/layers/picture_layer_impl.h"

namespace cc {

class CC_EXPORT PictureImageLayerImpl : public PictureLayerImpl {
 public:
  static scoped_ptr<PictureImageLayerImpl> Create(LayerTreeImpl* tree_impl,
                                                  int id) {
    return make_scoped_ptr(new PictureImageLayerImpl(tree_impl, id));
  }
  virtual ~PictureImageLayerImpl();

  // LayerImpl overrides.
  virtual const char* LayerTypeAsString() const OVERRIDE;
  virtual scoped_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) OVERRIDE;
  virtual void CalculateContentsScale(float ideal_contents_scale,
                                      float device_scale_factor,
                                      float page_scale_factor,
                                      float maximum_animation_contents_scale,
                                      bool animating_transform_to_screen,
                                      float* contents_scale_x,
                                      float* contents_scale_y,
                                      gfx::Size* content_bounds) OVERRIDE;

 protected:
  PictureImageLayerImpl(LayerTreeImpl* tree_impl, int id);

  virtual bool ShouldAdjustRasterScale(
      bool animating_transform_to_screen) const OVERRIDE;
  virtual void RecalculateRasterScales(bool animating_transform_to_screen,
                                       float maximum_animation_contents_scale)
      OVERRIDE;
  virtual void GetDebugBorderProperties(
      SkColor* color, float* width) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(PictureImageLayerImpl);
};

}  // namespace cc

#endif  // CC_LAYERS_PICTURE_IMAGE_LAYER_IMPL_H_
