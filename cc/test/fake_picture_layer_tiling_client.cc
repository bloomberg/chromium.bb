// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_picture_layer_tiling_client.h"

#include <limits>

#include "cc/test/fake_picture_pile_impl.h"
#include "cc/test/fake_tile_manager.h"

namespace cc {

FakePictureLayerTilingClient::FakePictureLayerTilingClient()
    : tile_manager_(new FakeTileManager(&tile_manager_client_)),
      pile_(FakePicturePileImpl::CreateInfiniteFilledPile()),
      twin_tiling_(NULL),
      recycled_twin_tiling_(NULL),
      allow_create_tile_(true),
      max_tiles_for_interest_area_(10000),
      skewport_target_time_in_seconds_(1.0f),
      skewport_extrapolation_limit_in_content_pixels_(2000) {
}

FakePictureLayerTilingClient::FakePictureLayerTilingClient(
    ResourceProvider* resource_provider)
    : resource_pool_(
          ResourcePool::Create(resource_provider, GL_TEXTURE_2D, RGBA_8888)),
      tile_manager_(
          new FakeTileManager(&tile_manager_client_, resource_pool_.get())),
      pile_(FakePicturePileImpl::CreateInfiniteFilledPile()),
      twin_tiling_(NULL),
      recycled_twin_tiling_(NULL),
      allow_create_tile_(true),
      max_tiles_for_interest_area_(10000),
      skewport_target_time_in_seconds_(1.0f) {
}

FakePictureLayerTilingClient::~FakePictureLayerTilingClient() {
}

scoped_refptr<Tile> FakePictureLayerTilingClient::CreateTile(
    PictureLayerTiling*,
    const gfx::Rect& rect) {
  if (!allow_create_tile_)
    return scoped_refptr<Tile>();
  return tile_manager_->CreateTile(
      pile_.get(), tile_size_, rect, gfx::Rect(), 1, 0, 0, 0);
}

PicturePileImpl* FakePictureLayerTilingClient::GetPile() {
  return pile_.get();
}

void FakePictureLayerTilingClient::SetTileSize(const gfx::Size& tile_size) {
  tile_size_ = tile_size;
}

gfx::Size FakePictureLayerTilingClient::CalculateTileSize(
    const gfx::Size& /* content_bounds */) const {
  return tile_size_;
}

size_t FakePictureLayerTilingClient::GetMaxTilesForInterestArea() const {
  return max_tiles_for_interest_area_;
}

float FakePictureLayerTilingClient::GetSkewportTargetTimeInSeconds() const {
  return skewport_target_time_in_seconds_;
}

int FakePictureLayerTilingClient::GetSkewportExtrapolationLimitInContentPixels()
    const {
  return skewport_extrapolation_limit_in_content_pixels_;
}

const Region* FakePictureLayerTilingClient::GetInvalidation() {
  return &invalidation_;
}

const PictureLayerTiling* FakePictureLayerTilingClient::GetTwinTiling(
      const PictureLayerTiling* tiling) const {
  return twin_tiling_;
}

PictureLayerTiling* FakePictureLayerTilingClient::GetRecycledTwinTiling(
    const PictureLayerTiling* tiling) {
  return recycled_twin_tiling_;
}

WhichTree FakePictureLayerTilingClient::GetTree() const {
  return tree_;
}

}  // namespace cc
