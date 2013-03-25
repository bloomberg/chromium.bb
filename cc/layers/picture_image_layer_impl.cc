// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_image_layer_impl.h"

#include "cc/debug/debug_colors.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

PictureImageLayerImpl::PictureImageLayerImpl(LayerTreeImpl* tree_impl, int id)
    : PictureLayerImpl(tree_impl, id) {
}

PictureImageLayerImpl::~PictureImageLayerImpl() {
}

const char* PictureImageLayerImpl::LayerTypeAsString() const {
  return "PictureImageLayer";
}

scoped_ptr<LayerImpl> PictureImageLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return PictureImageLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

void PictureImageLayerImpl::GetDebugBorderProperties(
    SkColor* color, float* width) const {
  *color = DebugColors::ImageLayerBorderColor();
  *width = DebugColors::ImageLayerBorderWidth(layer_tree_impl());
}

void PictureImageLayerImpl::CalculateRasterContentsScale(
    bool animating_transform_to_screen,
    float* raster_contents_scale,
    float* low_res_raster_contents_scale) {
  // Don't scale images during rastering to ensure image quality, save memory
  // and avoid frequent re-rastering on change of scale.
  *raster_contents_scale = std::max(1.f, MinimumContentsScale());
  // We don't need low res tiles.
  *low_res_raster_contents_scale = *raster_contents_scale;
}

}  // namespace cc
