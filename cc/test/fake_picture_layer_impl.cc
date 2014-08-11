// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_picture_layer_impl.h"

#include <vector>
#include "cc/resources/tile.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

FakePictureLayerImpl::FakePictureLayerImpl(LayerTreeImpl* tree_impl,
                                           int id,
                                           scoped_refptr<PicturePileImpl> pile)
    : PictureLayerImpl(tree_impl, id),
      append_quads_count_(0),
      did_become_active_call_count_(0),
      has_valid_tile_priorities_(false),
      use_set_valid_tile_priorities_flag_(false) {
  pile_ = pile;
  SetBounds(pile_->tiling_size());
  SetContentBounds(pile_->tiling_size());
}

FakePictureLayerImpl::FakePictureLayerImpl(LayerTreeImpl* tree_impl,
                                           int id,
                                           scoped_refptr<PicturePileImpl> pile,
                                           const gfx::Size& layer_bounds)
    : PictureLayerImpl(tree_impl, id),
      append_quads_count_(0),
      did_become_active_call_count_(0),
      has_valid_tile_priorities_(false),
      use_set_valid_tile_priorities_flag_(false) {
  pile_ = pile;
  SetBounds(layer_bounds);
  SetContentBounds(layer_bounds);
}

FakePictureLayerImpl::FakePictureLayerImpl(LayerTreeImpl* tree_impl, int id)
    : PictureLayerImpl(tree_impl, id),
      append_quads_count_(0),
      did_become_active_call_count_(0),
      has_valid_tile_priorities_(false),
      use_set_valid_tile_priorities_flag_(false) {
}

scoped_ptr<LayerImpl> FakePictureLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return make_scoped_ptr(
      new FakePictureLayerImpl(tree_impl, id())).PassAs<LayerImpl>();
}

void FakePictureLayerImpl::AppendQuads(
    RenderPass* render_pass,
    const OcclusionTracker<LayerImpl>& occlusion_tracker,
    AppendQuadsData* append_quads_data) {
  PictureLayerImpl::AppendQuads(
      render_pass, occlusion_tracker, append_quads_data);
  ++append_quads_count_;
}

gfx::Size FakePictureLayerImpl::CalculateTileSize(
    const gfx::Size& content_bounds) const {
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

void FakePictureLayerImpl::SetAllTilesVisible() {
  WhichTree tree =
      layer_tree_impl()->IsActiveTree() ? ACTIVE_TREE : PENDING_TREE;

  for (size_t tiling_idx = 0; tiling_idx < tilings_->num_tilings();
       ++tiling_idx) {
    PictureLayerTiling* tiling = tilings_->tiling_at(tiling_idx);
    std::vector<Tile*> tiles = tiling->AllTilesForTesting();
    for (size_t tile_idx = 0; tile_idx < tiles.size(); ++tile_idx) {
      Tile* tile = tiles[tile_idx];
      TilePriority priority;
      priority.resolution = HIGH_RESOLUTION;
      priority.priority_bin = TilePriority::NOW;
      priority.distance_to_visible = 0.f;
      tile->SetPriority(tree, priority);
    }
  }
}

void FakePictureLayerImpl::ResetAllTilesPriorities() {
  for (size_t tiling_idx = 0; tiling_idx < tilings_->num_tilings();
       ++tiling_idx) {
    PictureLayerTiling* tiling = tilings_->tiling_at(tiling_idx);
    std::vector<Tile*> tiles = tiling->AllTilesForTesting();
    for (size_t tile_idx = 0; tile_idx < tiles.size(); ++tile_idx) {
      Tile* tile = tiles[tile_idx];
      tile->SetPriority(ACTIVE_TREE, TilePriority());
      tile->SetPriority(PENDING_TREE, TilePriority());
    }
  }
}

void FakePictureLayerImpl::SetAllTilesReady() {
  for (size_t tiling_idx = 0; tiling_idx < tilings_->num_tilings();
       ++tiling_idx) {
    PictureLayerTiling* tiling = tilings_->tiling_at(tiling_idx);
    SetAllTilesReadyInTiling(tiling);
  }
}

void FakePictureLayerImpl::SetAllTilesReadyInTiling(
    PictureLayerTiling* tiling) {
  std::vector<Tile*> tiles = tiling->AllTilesForTesting();
  for (size_t tile_idx = 0; tile_idx < tiles.size(); ++tile_idx) {
    Tile* tile = tiles[tile_idx];
    ManagedTileState& state = tile->managed_state();
    for (size_t mode_idx = 0; mode_idx < NUM_RASTER_MODES; ++mode_idx)
      state.tile_versions[mode_idx].SetSolidColorForTesting(true);
    DCHECK(tile->IsReadyToDraw());
  }
}

void FakePictureLayerImpl::CreateDefaultTilingsAndTiles() {
  layer_tree_impl()->UpdateDrawProperties();

  if (CanHaveTilings()) {
    DCHECK_EQ(tilings()->num_tilings(),
              layer_tree_impl()->settings().create_low_res_tiling ? 2u : 1u);
    DCHECK_EQ(tilings()->tiling_at(0)->resolution(), HIGH_RESOLUTION);
    HighResTiling()->CreateAllTilesForTesting();
    if (layer_tree_impl()->settings().create_low_res_tiling) {
      DCHECK_EQ(tilings()->tiling_at(1)->resolution(), LOW_RESOLUTION);
      LowResTiling()->CreateAllTilesForTesting();
    }
  } else {
    DCHECK_EQ(tilings()->num_tilings(), 0u);
  }
}

void FakePictureLayerImpl::DidBecomeActive() {
  PictureLayerImpl::DidBecomeActive();
  ++did_become_active_call_count_;
}

bool FakePictureLayerImpl::HasValidTilePriorities() const {
  return use_set_valid_tile_priorities_flag_
             ? has_valid_tile_priorities_
             : PictureLayerImpl::HasValidTilePriorities();
}

}  // namespace cc
