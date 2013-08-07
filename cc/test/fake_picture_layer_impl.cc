// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_picture_layer_impl.h"

namespace cc {

FakePictureLayerImpl::FakePictureLayerImpl(
    LayerTreeImpl* tree_impl,
    int id,
    scoped_refptr<PicturePileImpl> pile)
    : PictureLayerImpl(tree_impl, id),
      append_quads_count_(0) {
  pile_ = pile;
  SetBounds(pile_->size());
  CreateTilingSetIfNeeded();
}

FakePictureLayerImpl::FakePictureLayerImpl(LayerTreeImpl* tree_impl, int id)
    : PictureLayerImpl(tree_impl, id), append_quads_count_(0) {}

scoped_ptr<LayerImpl> FakePictureLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return make_scoped_ptr(
      new FakePictureLayerImpl(tree_impl, id())).PassAs<LayerImpl>();
}

void FakePictureLayerImpl::AppendQuads(QuadSink* quad_sink,
                                       AppendQuadsData* append_quads_data) {
  PictureLayerImpl::AppendQuads(quad_sink, append_quads_data);
  ++append_quads_count_;
}

gfx::Size FakePictureLayerImpl::CalculateTileSize(
    gfx::Size content_bounds) const {
  if (fixed_tile_size_.IsEmpty()) {
    return PictureLayerImpl::CalculateTileSize(content_bounds);
  }

  return fixed_tile_size_;
}

PictureLayerTiling* FakePictureLayerImpl::HighResTiling() const {
  PictureLayerTiling* result = NULL;
  for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
    PictureLayerTiling* tiling = tilings_->tiling_at(i);
    if (tiling->resolution() == HIGH_RESOLUTION) {
      // There should be only one high res tiling.
      CHECK(!result);
      result = tiling;
    }
  }
  return result;
}

PictureLayerTiling* FakePictureLayerImpl::LowResTiling() const {
  PictureLayerTiling* result = NULL;
  for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
    PictureLayerTiling* tiling = tilings_->tiling_at(i);
    if (tiling->resolution() == LOW_RESOLUTION) {
      // There should be only one low res tiling.
      CHECK(!result);
      result = tiling;
    }
  }
  return result;
}

}  // namespace cc
