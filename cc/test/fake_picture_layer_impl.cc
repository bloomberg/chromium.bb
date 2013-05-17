// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_picture_layer_impl.h"

namespace cc {

FakePictureLayerImpl::FakePictureLayerImpl(
    LayerTreeImpl* tree_impl,
    int id,
    scoped_refptr<PicturePileImpl> pile)
    : PictureLayerImpl(tree_impl, id) {
  pile_ = pile;
  SetBounds(pile_->size());
  CreateTilingSet();
}

FakePictureLayerImpl::FakePictureLayerImpl(LayerTreeImpl* tree_impl, int id)
    : PictureLayerImpl(tree_impl, id) {}

scoped_ptr<LayerImpl> FakePictureLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return make_scoped_ptr(
      new FakePictureLayerImpl(tree_impl, id())).PassAs<LayerImpl>();
}

gfx::Size FakePictureLayerImpl::CalculateTileSize(gfx::Size content_bounds) {
  if (fixed_tile_size_.IsEmpty()) {
    return PictureLayerImpl::CalculateTileSize(content_bounds);
  }

  return fixed_tile_size_;
}

}  // namespace cc
