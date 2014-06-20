// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tile.h"
#include "cc/resources/tile_priority.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_picture_pile_impl.h"
#include "cc/test/fake_tile_manager.h"
#include "cc/test/impl_side_painting_settings.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_tile_priorities.h"
#include "cc/trees/layer_tree_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class TileManagerTileIteratorTest : public testing::Test {
 public:
  TileManagerTileIteratorTest()
      : memory_limit_policy_(ALLOW_ANYTHING),
        max_tiles_(10000),
        ready_to_activate_(false),
        id_(7),
        proxy_(base::MessageLoopProxy::current()),
        host_impl_(ImplSidePaintingSettings(),
                   &proxy_,
                   &shared_bitmap_manager_) {}

  void SetTreePriority(TreePriority tree_priority) {
    GlobalStateThatImpactsTilePriority state;
    gfx::Size tile_size(256, 256);

    state.soft_memory_limit_in_bytes = 100 * 1000 * 1000;
    state.num_resources_limit = max_tiles_;
    state.hard_memory_limit_in_bytes = state.soft_memory_limit_in_bytes * 2;
    state.memory_limit_policy = memory_limit_policy_;
    state.tree_priority = tree_priority;

    global_state_ = state;
    host_impl_.resource_pool()->SetResourceUsageLimits(
        state.soft_memory_limit_in_bytes,
        state.soft_memory_limit_in_bytes,
        state.num_resources_limit);
    host_impl_.tile_manager()->SetGlobalStateForTesting(state);
  }

  virtual void SetUp() OVERRIDE {
    InitializeRenderer();
    SetTreePriority(SAME_PRIORITY_FOR_BOTH_TREES);
  }

  virtual void InitializeRenderer() {
    host_impl_.InitializeRenderer(
        FakeOutputSurface::Create3d().PassAs<OutputSurface>());
  }

  void SetupDefaultTrees(const gfx::Size& layer_bounds) {
    gfx::Size tile_size(100, 100);

    scoped_refptr<FakePicturePileImpl> pending_pile =
        FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
    scoped_refptr<FakePicturePileImpl> active_pile =
        FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

    SetupTrees(pending_pile, active_pile);
  }

  void ActivateTree() {
    host_impl_.ActivatePendingTree();
    CHECK(!host_impl_.pending_tree());
    pending_layer_ = NULL;
    active_layer_ = static_cast<FakePictureLayerImpl*>(
        host_impl_.active_tree()->LayerById(id_));
  }

  void SetupDefaultTreesWithFixedTileSize(const gfx::Size& layer_bounds,
                                          const gfx::Size& tile_size) {
    SetupDefaultTrees(layer_bounds);
    pending_layer_->set_fixed_tile_size(tile_size);
    active_layer_->set_fixed_tile_size(tile_size);
  }

  void SetupTrees(scoped_refptr<PicturePileImpl> pending_pile,
                  scoped_refptr<PicturePileImpl> active_pile) {
    SetupPendingTree(active_pile);
    ActivateTree();
    SetupPendingTree(pending_pile);
  }

  void SetupPendingTree(scoped_refptr<PicturePileImpl> pile) {
    host_impl_.CreatePendingTree();
    LayerTreeImpl* pending_tree = host_impl_.pending_tree();
    // Clear recycled tree.
    pending_tree->DetachLayerTree();

    scoped_ptr<FakePictureLayerImpl> pending_layer =
        FakePictureLayerImpl::CreateWithPile(pending_tree, id_, pile);
    pending_layer->SetDrawsContent(true);
    pending_tree->SetRootLayer(pending_layer.PassAs<LayerImpl>());

    pending_layer_ = static_cast<FakePictureLayerImpl*>(
        host_impl_.pending_tree()->LayerById(id_));
    pending_layer_->DoPostCommitInitializationIfNeeded();
  }

  void CreateHighLowResAndSetAllTilesVisible() {
    // Active layer must get updated first so pending layer can share from it.
    active_layer_->CreateDefaultTilingsAndTiles();
    active_layer_->SetAllTilesVisible();
    pending_layer_->CreateDefaultTilingsAndTiles();
    pending_layer_->SetAllTilesVisible();
  }

  TileManager* tile_manager() { return host_impl_.tile_manager(); }

 protected:
  GlobalStateThatImpactsTilePriority global_state_;

  TestSharedBitmapManager shared_bitmap_manager_;
  TileMemoryLimitPolicy memory_limit_policy_;
  int max_tiles_;
  bool ready_to_activate_;
  int id_;
  FakeImplProxy proxy_;
  FakeLayerTreeHostImpl host_impl_;
  FakePictureLayerImpl* pending_layer_;
  FakePictureLayerImpl* active_layer_;
};

TEST_F(TileManagerTileIteratorTest, PairedPictureLayers) {
  host_impl_.CreatePendingTree();
  host_impl_.ActivatePendingTree();
  host_impl_.CreatePendingTree();

  LayerTreeImpl* active_tree = host_impl_.active_tree();
  LayerTreeImpl* pending_tree = host_impl_.pending_tree();
  EXPECT_NE(active_tree, pending_tree);

  scoped_ptr<FakePictureLayerImpl> active_layer =
      FakePictureLayerImpl::Create(active_tree, 10);
  scoped_ptr<FakePictureLayerImpl> pending_layer =
      FakePictureLayerImpl::Create(pending_tree, 10);

  TileManager* tile_manager = TileManagerTileIteratorTest::tile_manager();
  EXPECT_TRUE(tile_manager);

  std::vector<TileManager::PairedPictureLayer> paired_layers;
  tile_manager->GetPairedPictureLayers(&paired_layers);

  EXPECT_EQ(2u, paired_layers.size());
  if (paired_layers[0].active_layer) {
    EXPECT_EQ(active_layer.get(), paired_layers[0].active_layer);
    EXPECT_EQ(NULL, paired_layers[0].pending_layer);
  } else {
    EXPECT_EQ(pending_layer.get(), paired_layers[0].pending_layer);
    EXPECT_EQ(NULL, paired_layers[0].active_layer);
  }

  if (paired_layers[1].active_layer) {
    EXPECT_EQ(active_layer.get(), paired_layers[1].active_layer);
    EXPECT_EQ(NULL, paired_layers[1].pending_layer);
  } else {
    EXPECT_EQ(pending_layer.get(), paired_layers[1].pending_layer);
    EXPECT_EQ(NULL, paired_layers[1].active_layer);
  }

  active_layer->set_twin_layer(pending_layer.get());
  pending_layer->set_twin_layer(active_layer.get());

  tile_manager->GetPairedPictureLayers(&paired_layers);
  EXPECT_EQ(1u, paired_layers.size());

  EXPECT_EQ(active_layer.get(), paired_layers[0].active_layer);
  EXPECT_EQ(pending_layer.get(), paired_layers[0].pending_layer);
}

TEST_F(TileManagerTileIteratorTest, RasterTileIterator) {
  SetupDefaultTrees(gfx::Size(1000, 1000));
  TileManager* tile_manager = TileManagerTileIteratorTest::tile_manager();
  EXPECT_TRUE(tile_manager);

  active_layer_->CreateDefaultTilingsAndTiles();
  pending_layer_->CreateDefaultTilingsAndTiles();

  std::vector<TileManager::PairedPictureLayer> paired_layers;
  tile_manager->GetPairedPictureLayers(&paired_layers);
  EXPECT_EQ(1u, paired_layers.size());

  TileManager::RasterTileIterator it(tile_manager,
                                     SAME_PRIORITY_FOR_BOTH_TREES);
  EXPECT_TRUE(it);

  size_t tile_count = 0;
  std::set<Tile*> all_tiles;
  for (; it; ++it) {
    EXPECT_TRUE(*it);
    all_tiles.insert(*it);
    ++tile_count;
  }

  EXPECT_EQ(tile_count, all_tiles.size());
  EXPECT_EQ(17u, tile_count);

  // Sanity check, all tiles should be visible.
  std::set<Tile*> smoothness_tiles;
  for (TileManager::RasterTileIterator it(tile_manager,
                                          SMOOTHNESS_TAKES_PRIORITY);
       it;
       ++it) {
    Tile* tile = *it;
    EXPECT_TRUE(tile);
    EXPECT_EQ(TilePriority::NOW, tile->priority(ACTIVE_TREE).priority_bin);
    EXPECT_EQ(TilePriority::NOW, tile->priority(PENDING_TREE).priority_bin);
    smoothness_tiles.insert(tile);
  }
  EXPECT_EQ(all_tiles, smoothness_tiles);

  Region invalidation(gfx::Rect(0, 0, 500, 500));

  // Invalidate the pending tree.
  pending_layer_->set_invalidation(invalidation);
  pending_layer_->HighResTiling()->Invalidate(invalidation);
  pending_layer_->LowResTiling()->Invalidate(invalidation);

  active_layer_->ResetAllTilesPriorities();
  pending_layer_->ResetAllTilesPriorities();

  // Renew all of the tile priorities.
  gfx::Rect viewport(50, 50, 100, 100);
  pending_layer_->HighResTiling()->UpdateTilePriorities(
      PENDING_TREE, viewport, 1.0f, 1.0);
  pending_layer_->LowResTiling()->UpdateTilePriorities(
      PENDING_TREE, viewport, 1.0f, 1.0);
  active_layer_->HighResTiling()->UpdateTilePriorities(
      ACTIVE_TREE, viewport, 1.0f, 1.0);
  active_layer_->LowResTiling()->UpdateTilePriorities(
      ACTIVE_TREE, viewport, 1.0f, 1.0);

  // Populate all tiles directly from the tilings.
  all_tiles.clear();
  std::vector<Tile*> pending_high_res_tiles =
      pending_layer_->HighResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < pending_high_res_tiles.size(); ++i)
    all_tiles.insert(pending_high_res_tiles[i]);

  std::vector<Tile*> pending_low_res_tiles =
      pending_layer_->LowResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < pending_low_res_tiles.size(); ++i)
    all_tiles.insert(pending_low_res_tiles[i]);

  std::vector<Tile*> active_high_res_tiles =
      active_layer_->HighResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < active_high_res_tiles.size(); ++i)
    all_tiles.insert(active_high_res_tiles[i]);

  std::vector<Tile*> active_low_res_tiles =
      active_layer_->LowResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < active_low_res_tiles.size(); ++i)
    all_tiles.insert(active_low_res_tiles[i]);

  Tile* last_tile = NULL;
  smoothness_tiles.clear();
  tile_count = 0;
  size_t increasing_distance_tiles = 0u;
  // Here we expect to get increasing ACTIVE_TREE priority_bin.
  for (TileManager::RasterTileIterator it(tile_manager,
                                          SMOOTHNESS_TAKES_PRIORITY);
       it;
       ++it) {
    Tile* tile = *it;
    EXPECT_TRUE(tile);

    if (!last_tile)
      last_tile = tile;

    EXPECT_LE(last_tile->priority(ACTIVE_TREE).priority_bin,
              tile->priority(ACTIVE_TREE).priority_bin);
    if (last_tile->priority(ACTIVE_TREE).priority_bin ==
        tile->priority(ACTIVE_TREE).priority_bin) {
      increasing_distance_tiles +=
          last_tile->priority(ACTIVE_TREE).distance_to_visible <=
          tile->priority(ACTIVE_TREE).distance_to_visible;
    }

    if (tile->priority(ACTIVE_TREE).priority_bin == TilePriority::NOW &&
        last_tile->priority(ACTIVE_TREE).resolution !=
            tile->priority(ACTIVE_TREE).resolution) {
      // Low resolution should come first.
      EXPECT_EQ(LOW_RESOLUTION, last_tile->priority(ACTIVE_TREE).resolution);
    }

    last_tile = tile;
    ++tile_count;
    smoothness_tiles.insert(tile);
  }

  EXPECT_EQ(tile_count, smoothness_tiles.size());
  EXPECT_EQ(all_tiles, smoothness_tiles);
  // Since we don't guarantee increasing distance due to spiral iterator, we
  // should check that we're _mostly_ right.
  EXPECT_GT(increasing_distance_tiles, 3 * tile_count / 4);

  std::set<Tile*> new_content_tiles;
  last_tile = NULL;
  increasing_distance_tiles = 0u;
  // Here we expect to get increasing PENDING_TREE priority_bin.
  for (TileManager::RasterTileIterator it(tile_manager,
                                          NEW_CONTENT_TAKES_PRIORITY);
       it;
       ++it) {
    Tile* tile = *it;
    EXPECT_TRUE(tile);

    if (!last_tile)
      last_tile = tile;

    EXPECT_LE(last_tile->priority(PENDING_TREE).priority_bin,
              tile->priority(PENDING_TREE).priority_bin);
    if (last_tile->priority(PENDING_TREE).priority_bin ==
        tile->priority(PENDING_TREE).priority_bin) {
      increasing_distance_tiles +=
          last_tile->priority(PENDING_TREE).distance_to_visible <=
          tile->priority(PENDING_TREE).distance_to_visible;
    }

    if (tile->priority(PENDING_TREE).priority_bin == TilePriority::NOW &&
        last_tile->priority(PENDING_TREE).resolution !=
            tile->priority(PENDING_TREE).resolution) {
      // High resolution should come first.
      EXPECT_EQ(HIGH_RESOLUTION, last_tile->priority(PENDING_TREE).resolution);
    }

    last_tile = tile;
    new_content_tiles.insert(tile);
  }

  EXPECT_EQ(tile_count, new_content_tiles.size());
  EXPECT_EQ(all_tiles, new_content_tiles);
  // Since we don't guarantee increasing distance due to spiral iterator, we
  // should check that we're _mostly_ right.
  EXPECT_GT(increasing_distance_tiles, 3 * tile_count / 4);
}

TEST_F(TileManagerTileIteratorTest, EvictionTileIterator) {
  SetupDefaultTrees(gfx::Size(1000, 1000));
  TileManager* tile_manager = TileManagerTileIteratorTest::tile_manager();
  EXPECT_TRUE(tile_manager);

  active_layer_->CreateDefaultTilingsAndTiles();
  pending_layer_->CreateDefaultTilingsAndTiles();

  std::vector<TileManager::PairedPictureLayer> paired_layers;
  tile_manager->GetPairedPictureLayers(&paired_layers);
  EXPECT_EQ(1u, paired_layers.size());

  TileManager::EvictionTileIterator empty_it(tile_manager,
                                             SAME_PRIORITY_FOR_BOTH_TREES);
  EXPECT_FALSE(empty_it);
  std::set<Tile*> all_tiles;
  size_t tile_count = 0;

  for (TileManager::RasterTileIterator raster_it(tile_manager,
                                                 SAME_PRIORITY_FOR_BOTH_TREES);
       raster_it;
       ++raster_it) {
    ++tile_count;
    EXPECT_TRUE(*raster_it);
    all_tiles.insert(*raster_it);
  }

  EXPECT_EQ(tile_count, all_tiles.size());
  EXPECT_EQ(17u, tile_count);

  tile_manager->InitializeTilesWithResourcesForTesting(
      std::vector<Tile*>(all_tiles.begin(), all_tiles.end()));

  TileManager::EvictionTileIterator it(tile_manager, SMOOTHNESS_TAKES_PRIORITY);
  EXPECT_TRUE(it);

  // Sanity check, all tiles should be visible.
  std::set<Tile*> smoothness_tiles;
  for (; it; ++it) {
    Tile* tile = *it;
    EXPECT_TRUE(tile);
    EXPECT_EQ(TilePriority::NOW, tile->priority(ACTIVE_TREE).priority_bin);
    EXPECT_EQ(TilePriority::NOW, tile->priority(PENDING_TREE).priority_bin);
    EXPECT_TRUE(tile->HasResources());
    smoothness_tiles.insert(tile);
  }
  EXPECT_EQ(all_tiles, smoothness_tiles);

  tile_manager->ReleaseTileResourcesForTesting(
      std::vector<Tile*>(all_tiles.begin(), all_tiles.end()));

  Region invalidation(gfx::Rect(0, 0, 500, 500));

  // Invalidate the pending tree.
  pending_layer_->set_invalidation(invalidation);
  pending_layer_->HighResTiling()->Invalidate(invalidation);
  pending_layer_->LowResTiling()->Invalidate(invalidation);

  active_layer_->ResetAllTilesPriorities();
  pending_layer_->ResetAllTilesPriorities();

  // Renew all of the tile priorities.
  gfx::Rect viewport(50, 50, 100, 100);
  pending_layer_->HighResTiling()->UpdateTilePriorities(
      PENDING_TREE, viewport, 1.0f, 1.0);
  pending_layer_->LowResTiling()->UpdateTilePriorities(
      PENDING_TREE, viewport, 1.0f, 1.0);
  active_layer_->HighResTiling()->UpdateTilePriorities(
      ACTIVE_TREE, viewport, 1.0f, 1.0);
  active_layer_->LowResTiling()->UpdateTilePriorities(
      ACTIVE_TREE, viewport, 1.0f, 1.0);

  // Populate all tiles directly from the tilings.
  all_tiles.clear();
  std::vector<Tile*> pending_high_res_tiles =
      pending_layer_->HighResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < pending_high_res_tiles.size(); ++i)
    all_tiles.insert(pending_high_res_tiles[i]);

  std::vector<Tile*> pending_low_res_tiles =
      pending_layer_->LowResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < pending_low_res_tiles.size(); ++i)
    all_tiles.insert(pending_low_res_tiles[i]);

  std::vector<Tile*> active_high_res_tiles =
      active_layer_->HighResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < active_high_res_tiles.size(); ++i)
    all_tiles.insert(active_high_res_tiles[i]);

  std::vector<Tile*> active_low_res_tiles =
      active_layer_->LowResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < active_low_res_tiles.size(); ++i)
    all_tiles.insert(active_low_res_tiles[i]);

  tile_manager->InitializeTilesWithResourcesForTesting(
      std::vector<Tile*>(all_tiles.begin(), all_tiles.end()));

  Tile* last_tile = NULL;
  smoothness_tiles.clear();
  tile_count = 0;
  // Here we expect to get increasing ACTIVE_TREE priority_bin.
  for (TileManager::EvictionTileIterator it(tile_manager,
                                            SMOOTHNESS_TAKES_PRIORITY);
       it;
       ++it) {
    Tile* tile = *it;
    EXPECT_TRUE(tile);
    EXPECT_TRUE(tile->HasResources());

    if (!last_tile)
      last_tile = tile;

    EXPECT_GE(last_tile->priority(ACTIVE_TREE).priority_bin,
              tile->priority(ACTIVE_TREE).priority_bin);
    if (last_tile->priority(ACTIVE_TREE).priority_bin ==
        tile->priority(ACTIVE_TREE).priority_bin) {
      EXPECT_GE(last_tile->priority(ACTIVE_TREE).distance_to_visible,
                tile->priority(ACTIVE_TREE).distance_to_visible);
    }

    last_tile = tile;
    ++tile_count;
    smoothness_tiles.insert(tile);
  }

  EXPECT_EQ(tile_count, smoothness_tiles.size());
  EXPECT_EQ(all_tiles, smoothness_tiles);

  std::set<Tile*> new_content_tiles;
  last_tile = NULL;
  // Here we expect to get increasing PENDING_TREE priority_bin.
  for (TileManager::EvictionTileIterator it(tile_manager,
                                            NEW_CONTENT_TAKES_PRIORITY);
       it;
       ++it) {
    Tile* tile = *it;
    EXPECT_TRUE(tile);

    if (!last_tile)
      last_tile = tile;

    EXPECT_GE(last_tile->priority(PENDING_TREE).priority_bin,
              tile->priority(PENDING_TREE).priority_bin);
    if (last_tile->priority(PENDING_TREE).priority_bin ==
        tile->priority(PENDING_TREE).priority_bin) {
      EXPECT_GE(last_tile->priority(PENDING_TREE).distance_to_visible,
                tile->priority(PENDING_TREE).distance_to_visible);
    }

    last_tile = tile;
    new_content_tiles.insert(tile);
  }

  EXPECT_EQ(tile_count, new_content_tiles.size());
  EXPECT_EQ(all_tiles, new_content_tiles);
}
}  // namespace
}  // namespace cc
