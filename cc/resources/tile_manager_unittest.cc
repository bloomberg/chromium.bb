// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/eviction_tile_priority_queue.h"
#include "cc/resources/raster_tile_priority_queue.h"
#include "cc/resources/tile.h"
#include "cc/resources/tile_priority.h"
#include "cc/resources/tiling_set_raster_queue_all.h"
#include "cc/test/begin_frame_args_test.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_picture_layer_tiling_client.h"
#include "cc/test/fake_picture_pile_impl.h"
#include "cc/test/fake_tile_manager.h"
#include "cc/test/impl_side_painting_settings.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_tile_priorities.h"
#include "cc/trees/layer_tree_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class LowResTilingsSettings : public ImplSidePaintingSettings {
 public:
  LowResTilingsSettings() { create_low_res_tiling = true; }
};

class TileManagerTilePriorityQueueTest : public testing::Test {
 public:
  TileManagerTilePriorityQueueTest()
      : memory_limit_policy_(ALLOW_ANYTHING),
        max_tiles_(10000),
        ready_to_activate_(false),
        id_(7),
        proxy_(base::MessageLoopProxy::current()),
        host_impl_(LowResTilingsSettings(), &proxy_, &shared_bitmap_manager_) {}

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

  void SetUp() override {
    InitializeRenderer();
    SetTreePriority(SAME_PRIORITY_FOR_BOTH_TREES);
  }

  virtual void InitializeRenderer() {
    host_impl_.InitializeRenderer(FakeOutputSurface::Create3d());
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
    host_impl_.ActivateSyncTree();
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

    // Steal from the recycled tree.
    scoped_ptr<LayerImpl> old_pending_root = pending_tree->DetachLayerTree();
    DCHECK_IMPLIES(old_pending_root, old_pending_root->id() == id_);

    scoped_ptr<FakePictureLayerImpl> pending_layer;
    if (old_pending_root) {
      pending_layer.reset(
          static_cast<FakePictureLayerImpl*>(old_pending_root.release()));
      pending_layer->SetRasterSourceOnPending(pile, Region());
    } else {
      pending_layer =
          FakePictureLayerImpl::CreateWithRasterSource(pending_tree, id_, pile);
      pending_layer->SetDrawsContent(true);
      pending_layer->SetHasRenderSurface(true);
    }
    // The bounds() just mirror the pile size.
    pending_layer->SetBounds(pending_layer->raster_source()->GetSize());
    pending_tree->SetRootLayer(pending_layer.Pass());

    pending_layer_ = static_cast<FakePictureLayerImpl*>(
        host_impl_.pending_tree()->LayerById(id_));

    // Add tilings/tiles for the layer.
    bool update_lcd_text = false;
    host_impl_.pending_tree()->UpdateDrawProperties(update_lcd_text);
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

TEST_F(TileManagerTilePriorityQueueTest, RasterTilePriorityQueue) {
  const gfx::Size layer_bounds(1000, 1000);
  host_impl_.SetViewportSize(layer_bounds);
  SetupDefaultTrees(layer_bounds);

  scoped_ptr<RasterTilePriorityQueue> queue(host_impl_.BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES, RasterTilePriorityQueue::Type::ALL));
  EXPECT_FALSE(queue->IsEmpty());

  size_t tile_count = 0;
  std::set<Tile*> all_tiles;
  while (!queue->IsEmpty()) {
    EXPECT_TRUE(queue->Top());
    all_tiles.insert(queue->Top());
    ++tile_count;
    queue->Pop();
  }

  EXPECT_EQ(tile_count, all_tiles.size());
  EXPECT_EQ(16u, tile_count);

  // Sanity check, all tiles should be visible.
  std::set<Tile*> smoothness_tiles;
  queue = host_impl_.BuildRasterQueue(SMOOTHNESS_TAKES_PRIORITY,
                                      RasterTilePriorityQueue::Type::ALL);
  bool had_low_res = false;
  while (!queue->IsEmpty()) {
    Tile* tile = queue->Top();
    EXPECT_TRUE(tile);
    EXPECT_EQ(TilePriority::NOW, tile->priority(ACTIVE_TREE).priority_bin);
    EXPECT_EQ(TilePriority::NOW, tile->priority(PENDING_TREE).priority_bin);
    if (tile->priority(ACTIVE_TREE).resolution == LOW_RESOLUTION)
      had_low_res = true;
    else
      smoothness_tiles.insert(tile);
    queue->Pop();
  }
  EXPECT_EQ(all_tiles, smoothness_tiles);
  EXPECT_TRUE(had_low_res);

  // Check that everything is required for activation.
  queue = host_impl_.BuildRasterQueue(
      SMOOTHNESS_TAKES_PRIORITY,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_ACTIVATION);
  std::set<Tile*> required_for_activation_tiles;
  while (!queue->IsEmpty()) {
    Tile* tile = queue->Top();
    EXPECT_TRUE(tile->required_for_activation());
    required_for_activation_tiles.insert(tile);
    queue->Pop();
  }
  EXPECT_EQ(all_tiles, required_for_activation_tiles);

  // Check that everything is required for draw.
  queue = host_impl_.BuildRasterQueue(
      SMOOTHNESS_TAKES_PRIORITY,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_DRAW);
  std::set<Tile*> required_for_draw_tiles;
  while (!queue->IsEmpty()) {
    Tile* tile = queue->Top();
    EXPECT_TRUE(tile->required_for_draw());
    required_for_draw_tiles.insert(tile);
    queue->Pop();
  }
  EXPECT_EQ(all_tiles, required_for_draw_tiles);

  Region invalidation(gfx::Rect(0, 0, 500, 500));

  // Invalidate the pending tree.
  pending_layer_->set_invalidation(invalidation);
  pending_layer_->HighResTiling()->Invalidate(invalidation);
  pending_layer_->LowResTiling()->Invalidate(invalidation);

  active_layer_->ResetAllTilesPriorities();
  pending_layer_->ResetAllTilesPriorities();

  // Renew all of the tile priorities.
  gfx::Rect viewport(50, 50, 100, 100);
  pending_layer_->HighResTiling()->ComputeTilePriorityRects(viewport, 1.0f, 1.0,
                                                            Occlusion());
  pending_layer_->LowResTiling()->ComputeTilePriorityRects(viewport, 1.0f, 1.0,
                                                           Occlusion());
  active_layer_->HighResTiling()->ComputeTilePriorityRects(viewport, 1.0f, 1.0,
                                                           Occlusion());
  active_layer_->LowResTiling()->ComputeTilePriorityRects(viewport, 1.0f, 1.0,
                                                          Occlusion());

  // Populate all tiles directly from the tilings.
  all_tiles.clear();
  std::set<Tile*> high_res_tiles;
  std::vector<Tile*> pending_high_res_tiles =
      pending_layer_->HighResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < pending_high_res_tiles.size(); ++i) {
    all_tiles.insert(pending_high_res_tiles[i]);
    high_res_tiles.insert(pending_high_res_tiles[i]);
  }

  std::vector<Tile*> pending_low_res_tiles =
      pending_layer_->LowResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < pending_low_res_tiles.size(); ++i)
    all_tiles.insert(pending_low_res_tiles[i]);

  std::vector<Tile*> active_high_res_tiles =
      active_layer_->HighResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < active_high_res_tiles.size(); ++i) {
    all_tiles.insert(active_high_res_tiles[i]);
    high_res_tiles.insert(active_high_res_tiles[i]);
  }

  std::vector<Tile*> active_low_res_tiles =
      active_layer_->LowResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < active_low_res_tiles.size(); ++i)
    all_tiles.insert(active_low_res_tiles[i]);

  Tile* last_tile = NULL;
  smoothness_tiles.clear();
  tile_count = 0;
  size_t correct_order_tiles = 0u;
  // Here we expect to get increasing ACTIVE_TREE priority_bin.
  queue = host_impl_.BuildRasterQueue(SMOOTHNESS_TAKES_PRIORITY,
                                      RasterTilePriorityQueue::Type::ALL);
  std::set<Tile*> expected_required_for_draw_tiles;
  std::set<Tile*> expected_required_for_activation_tiles;
  while (!queue->IsEmpty()) {
    Tile* tile = queue->Top();
    EXPECT_TRUE(tile);

    if (!last_tile)
      last_tile = tile;

    EXPECT_LE(last_tile->priority(ACTIVE_TREE).priority_bin,
              tile->priority(ACTIVE_TREE).priority_bin);
    bool skip_updating_last_tile = false;
    if (last_tile->priority(ACTIVE_TREE).priority_bin ==
        tile->priority(ACTIVE_TREE).priority_bin) {
      correct_order_tiles +=
          last_tile->priority(ACTIVE_TREE).distance_to_visible <=
          tile->priority(ACTIVE_TREE).distance_to_visible;
    } else if (tile->priority(ACTIVE_TREE).priority_bin ==
                   TilePriority::EVENTUALLY &&
               tile->priority(PENDING_TREE).priority_bin == TilePriority::NOW) {
      // Since we'd return pending tree now tiles before the eventually tiles on
      // the active tree, update the value.
      ++correct_order_tiles;
      skip_updating_last_tile = true;
    }

    if (tile->priority(ACTIVE_TREE).priority_bin == TilePriority::NOW &&
        last_tile->priority(ACTIVE_TREE).resolution !=
            tile->priority(ACTIVE_TREE).resolution) {
      // Low resolution should come first.
      EXPECT_EQ(LOW_RESOLUTION, last_tile->priority(ACTIVE_TREE).resolution);
    }

    if (!skip_updating_last_tile)
      last_tile = tile;
    ++tile_count;
    smoothness_tiles.insert(tile);
    if (tile->required_for_draw())
      expected_required_for_draw_tiles.insert(tile);
    if (tile->required_for_activation())
      expected_required_for_activation_tiles.insert(tile);
    queue->Pop();
  }

  EXPECT_EQ(tile_count, smoothness_tiles.size());
  EXPECT_EQ(all_tiles, smoothness_tiles);
  // Since we don't guarantee increasing distance due to spiral iterator, we
  // should check that we're _mostly_ right.
  EXPECT_GT(correct_order_tiles, 3 * tile_count / 4);

  // Check that we have consistent required_for_activation tiles.
  queue = host_impl_.BuildRasterQueue(
      SMOOTHNESS_TAKES_PRIORITY,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_ACTIVATION);
  required_for_activation_tiles.clear();
  while (!queue->IsEmpty()) {
    Tile* tile = queue->Top();
    EXPECT_TRUE(tile->required_for_activation());
    required_for_activation_tiles.insert(tile);
    queue->Pop();
  }
  EXPECT_EQ(expected_required_for_activation_tiles,
            required_for_activation_tiles);
  EXPECT_NE(all_tiles, required_for_activation_tiles);

  // Check that we have consistent required_for_draw tiles.
  queue = host_impl_.BuildRasterQueue(
      SMOOTHNESS_TAKES_PRIORITY,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_DRAW);
  required_for_draw_tiles.clear();
  while (!queue->IsEmpty()) {
    Tile* tile = queue->Top();
    EXPECT_TRUE(tile->required_for_draw());
    required_for_draw_tiles.insert(tile);
    queue->Pop();
  }
  EXPECT_EQ(expected_required_for_draw_tiles, required_for_draw_tiles);
  EXPECT_NE(all_tiles, required_for_draw_tiles);

  std::set<Tile*> new_content_tiles;
  last_tile = NULL;
  size_t increasing_distance_tiles = 0u;
  // Here we expect to get increasing PENDING_TREE priority_bin.
  queue = host_impl_.BuildRasterQueue(NEW_CONTENT_TAKES_PRIORITY,
                                      RasterTilePriorityQueue::Type::ALL);
  tile_count = 0;
  while (!queue->IsEmpty()) {
    Tile* tile = queue->Top();
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
    ++tile_count;
    queue->Pop();
  }

  EXPECT_EQ(tile_count, new_content_tiles.size());
  EXPECT_EQ(high_res_tiles, new_content_tiles);
  // Since we don't guarantee increasing distance due to spiral iterator, we
  // should check that we're _mostly_ right.
  EXPECT_GE(increasing_distance_tiles, 3 * tile_count / 4);

  // Check that we have consistent required_for_activation tiles.
  queue = host_impl_.BuildRasterQueue(
      NEW_CONTENT_TAKES_PRIORITY,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_ACTIVATION);
  required_for_activation_tiles.clear();
  while (!queue->IsEmpty()) {
    Tile* tile = queue->Top();
    EXPECT_TRUE(tile->required_for_activation());
    required_for_activation_tiles.insert(tile);
    queue->Pop();
  }
  EXPECT_EQ(expected_required_for_activation_tiles,
            required_for_activation_tiles);
  EXPECT_NE(new_content_tiles, required_for_activation_tiles);

  // Check that we have consistent required_for_draw tiles.
  queue = host_impl_.BuildRasterQueue(
      NEW_CONTENT_TAKES_PRIORITY,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_DRAW);
  required_for_draw_tiles.clear();
  while (!queue->IsEmpty()) {
    Tile* tile = queue->Top();
    EXPECT_TRUE(tile->required_for_draw());
    required_for_draw_tiles.insert(tile);
    queue->Pop();
  }
  EXPECT_EQ(expected_required_for_draw_tiles, required_for_draw_tiles);
  EXPECT_NE(new_content_tiles, required_for_draw_tiles);
}

TEST_F(TileManagerTilePriorityQueueTest, RasterTilePriorityQueueInvalidation) {
  const gfx::Size layer_bounds(1000, 1000);
  host_impl_.SetViewportSize(gfx::Size(500, 500));
  SetupDefaultTrees(layer_bounds);

  // Use a tile's content rect as an invalidation. We should inset it a bit to
  // ensure that border math doesn't invalidate neighbouring tiles.
  gfx::Rect invalidation =
      pending_layer_->HighResTiling()->TileAt(1, 0)->content_rect();
  invalidation.Inset(2, 2);

  pending_layer_->set_invalidation(invalidation);
  pending_layer_->HighResTiling()->Invalidate(invalidation);
  pending_layer_->LowResTiling()->Invalidate(invalidation);

  // Sanity checks: Tile at 0, 0 should be the same on both trees, tile at 1, 0
  // should be different.
  EXPECT_TRUE(pending_layer_->HighResTiling()->TileAt(0, 0));
  EXPECT_TRUE(active_layer_->HighResTiling()->TileAt(0, 0));
  EXPECT_EQ(pending_layer_->HighResTiling()->TileAt(0, 0),
            active_layer_->HighResTiling()->TileAt(0, 0));
  EXPECT_TRUE(pending_layer_->HighResTiling()->TileAt(1, 0));
  EXPECT_TRUE(active_layer_->HighResTiling()->TileAt(1, 0));
  EXPECT_NE(pending_layer_->HighResTiling()->TileAt(1, 0),
            active_layer_->HighResTiling()->TileAt(1, 0));

  std::set<Tile*> expected_now_tiles;
  std::set<Tile*> expected_required_for_draw_tiles;
  std::set<Tile*> expected_required_for_activation_tiles;
  for (int i = 0; i <= 1; ++i) {
    for (int j = 0; j <= 1; ++j) {
      expected_now_tiles.insert(pending_layer_->HighResTiling()->TileAt(i, j));
      expected_now_tiles.insert(active_layer_->HighResTiling()->TileAt(i, j));

      expected_required_for_activation_tiles.insert(
          pending_layer_->HighResTiling()->TileAt(i, j));
      expected_required_for_draw_tiles.insert(
          active_layer_->HighResTiling()->TileAt(i, j));
    }
  }
  // Expect 3 shared tiles and 1 unshared tile in total.
  EXPECT_EQ(5u, expected_now_tiles.size());
  // Expect 4 tiles for each draw and activation, but not all the same.
  EXPECT_EQ(4u, expected_required_for_activation_tiles.size());
  EXPECT_EQ(4u, expected_required_for_draw_tiles.size());
  EXPECT_NE(expected_required_for_draw_tiles,
            expected_required_for_activation_tiles);

  std::set<Tile*> expected_all_tiles;
  for (int i = 0; i <= 3; ++i) {
    for (int j = 0; j <= 3; ++j) {
      expected_all_tiles.insert(pending_layer_->HighResTiling()->TileAt(i, j));
      expected_all_tiles.insert(active_layer_->HighResTiling()->TileAt(i, j));
    }
  }
  // Expect 15 shared tiles and 1 unshared tile.
  EXPECT_EQ(17u, expected_all_tiles.size());

  // The actual test will now build different queues and verify that the queues
  // return the same information as computed manually above.
  scoped_ptr<RasterTilePriorityQueue> queue(host_impl_.BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES, RasterTilePriorityQueue::Type::ALL));
  std::set<Tile*> actual_now_tiles;
  std::set<Tile*> actual_all_tiles;
  while (!queue->IsEmpty()) {
    Tile* tile = queue->Top();
    queue->Pop();
    if (tile->combined_priority().priority_bin == TilePriority::NOW)
      actual_now_tiles.insert(tile);
    actual_all_tiles.insert(tile);
  }
  EXPECT_EQ(expected_now_tiles, actual_now_tiles);
  EXPECT_EQ(expected_all_tiles, actual_all_tiles);

  queue = host_impl_.BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_DRAW);
  std::set<Tile*> actual_required_for_draw_tiles;
  while (!queue->IsEmpty()) {
    Tile* tile = queue->Top();
    queue->Pop();
    actual_required_for_draw_tiles.insert(tile);
  }
  EXPECT_EQ(expected_required_for_draw_tiles, actual_required_for_draw_tiles);

  queue = host_impl_.BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_ACTIVATION);
  std::set<Tile*> actual_required_for_activation_tiles;
  while (!queue->IsEmpty()) {
    Tile* tile = queue->Top();
    queue->Pop();
    actual_required_for_activation_tiles.insert(tile);
  }
  EXPECT_EQ(expected_required_for_activation_tiles,
            actual_required_for_activation_tiles);
}

TEST_F(TileManagerTilePriorityQueueTest, ActivationComesBeforeEventually) {
  base::TimeTicks time_ticks;
  time_ticks += base::TimeDelta::FromMilliseconds(1);
  host_impl_.SetCurrentBeginFrameArgs(
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, time_ticks));

  gfx::Size layer_bounds(1000, 1000);
  SetupDefaultTrees(layer_bounds);

  // Create a pending child layer.
  gfx::Size tile_size(256, 256);
  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  scoped_ptr<FakePictureLayerImpl> pending_child =
      FakePictureLayerImpl::CreateWithRasterSource(host_impl_.pending_tree(),
                                                   id_ + 1, pending_pile);
  FakePictureLayerImpl* pending_child_raw = pending_child.get();
  pending_child_raw->SetDrawsContent(true);
  pending_layer_->AddChild(pending_child.Pass());

  // Set a small viewport, so we have soon and eventually tiles.
  host_impl_.SetViewportSize(gfx::Size(200, 200));
  time_ticks += base::TimeDelta::FromMilliseconds(1);
  host_impl_.SetCurrentBeginFrameArgs(
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, time_ticks));
  bool update_lcd_text = false;
  host_impl_.pending_tree()->UpdateDrawProperties(update_lcd_text);

  host_impl_.SetRequiresHighResToDraw();
  scoped_ptr<RasterTilePriorityQueue> queue(host_impl_.BuildRasterQueue(
      SMOOTHNESS_TAKES_PRIORITY, RasterTilePriorityQueue::Type::ALL));
  EXPECT_FALSE(queue->IsEmpty());

  // Get all the tiles that are NOW or SOON and make sure they are ready to
  // draw.
  std::vector<Tile*> all_tiles;
  while (!queue->IsEmpty()) {
    Tile* tile = queue->Top();
    if (tile->combined_priority().priority_bin >= TilePriority::EVENTUALLY)
      break;

    all_tiles.push_back(tile);
    queue->Pop();
  }

  tile_manager()->InitializeTilesWithResourcesForTesting(
      std::vector<Tile*>(all_tiles.begin(), all_tiles.end()));

  // Ensure we can activate.
  EXPECT_TRUE(tile_manager()->IsReadyToActivate());
}

TEST_F(TileManagerTilePriorityQueueTest, EvictionTilePriorityQueue) {
  const gfx::Size layer_bounds(1000, 1000);
  host_impl_.SetViewportSize(layer_bounds);
  SetupDefaultTrees(layer_bounds);

  scoped_ptr<EvictionTilePriorityQueue> empty_queue(
      host_impl_.BuildEvictionQueue(SAME_PRIORITY_FOR_BOTH_TREES));
  EXPECT_TRUE(empty_queue->IsEmpty());
  std::set<Tile*> all_tiles;
  size_t tile_count = 0;

  scoped_ptr<RasterTilePriorityQueue> raster_queue(host_impl_.BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES, RasterTilePriorityQueue::Type::ALL));
  while (!raster_queue->IsEmpty()) {
    ++tile_count;
    EXPECT_TRUE(raster_queue->Top());
    all_tiles.insert(raster_queue->Top());
    raster_queue->Pop();
  }

  EXPECT_EQ(tile_count, all_tiles.size());
  EXPECT_EQ(16u, tile_count);

  tile_manager()->InitializeTilesWithResourcesForTesting(
      std::vector<Tile*>(all_tiles.begin(), all_tiles.end()));

  scoped_ptr<EvictionTilePriorityQueue> queue(
      host_impl_.BuildEvictionQueue(SMOOTHNESS_TAKES_PRIORITY));
  EXPECT_FALSE(queue->IsEmpty());

  // Sanity check, all tiles should be visible.
  std::set<Tile*> smoothness_tiles;
  while (!queue->IsEmpty()) {
    Tile* tile = queue->Top();
    EXPECT_TRUE(tile);
    EXPECT_EQ(TilePriority::NOW, tile->priority(ACTIVE_TREE).priority_bin);
    EXPECT_EQ(TilePriority::NOW, tile->priority(PENDING_TREE).priority_bin);
    EXPECT_TRUE(tile->HasResource());
    smoothness_tiles.insert(tile);
    queue->Pop();
  }
  EXPECT_EQ(all_tiles, smoothness_tiles);

  tile_manager()->ReleaseTileResourcesForTesting(
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
  pending_layer_->HighResTiling()->ComputeTilePriorityRects(viewport, 1.0f, 1.0,
                                                            Occlusion());
  pending_layer_->LowResTiling()->ComputeTilePriorityRects(viewport, 1.0f, 1.0,
                                                           Occlusion());
  active_layer_->HighResTiling()->ComputeTilePriorityRects(viewport, 1.0f, 1.0,
                                                           Occlusion());
  active_layer_->LowResTiling()->ComputeTilePriorityRects(viewport, 1.0f, 1.0,
                                                          Occlusion());

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

  tile_manager()->InitializeTilesWithResourcesForTesting(
      std::vector<Tile*>(all_tiles.begin(), all_tiles.end()));

  Tile* last_tile = NULL;
  smoothness_tiles.clear();
  tile_count = 0;
  // Here we expect to get increasing combined priority_bin.
  queue = host_impl_.BuildEvictionQueue(SMOOTHNESS_TAKES_PRIORITY);
  int distance_increasing = 0;
  int distance_decreasing = 0;
  while (!queue->IsEmpty()) {
    Tile* tile = queue->Top();
    EXPECT_TRUE(tile);
    EXPECT_TRUE(tile->HasResource());

    if (!last_tile)
      last_tile = tile;

    const TilePriority& last_priority = last_tile->combined_priority();
    const TilePriority& priority = tile->combined_priority();

    EXPECT_GE(last_priority.priority_bin, priority.priority_bin);
    if (last_priority.priority_bin == priority.priority_bin) {
      EXPECT_LE(last_tile->required_for_activation(),
                tile->required_for_activation());
      if (last_tile->required_for_activation() ==
          tile->required_for_activation()) {
        if (last_priority.distance_to_visible >= priority.distance_to_visible)
          ++distance_decreasing;
        else
          ++distance_increasing;
      }
    }

    last_tile = tile;
    ++tile_count;
    smoothness_tiles.insert(tile);
    queue->Pop();
  }

  // Ensure that the distance is decreasing many more times than increasing.
  EXPECT_EQ(3, distance_increasing);
  EXPECT_EQ(17, distance_decreasing);
  EXPECT_EQ(tile_count, smoothness_tiles.size());
  EXPECT_EQ(all_tiles, smoothness_tiles);

  std::set<Tile*> new_content_tiles;
  last_tile = NULL;
  // Again, we expect to get increasing combined priority_bin.
  queue = host_impl_.BuildEvictionQueue(NEW_CONTENT_TAKES_PRIORITY);
  distance_decreasing = 0;
  distance_increasing = 0;
  while (!queue->IsEmpty()) {
    Tile* tile = queue->Top();
    EXPECT_TRUE(tile);

    if (!last_tile)
      last_tile = tile;

    const TilePriority& last_priority = last_tile->combined_priority();
    const TilePriority& priority = tile->combined_priority();

    EXPECT_GE(last_priority.priority_bin, priority.priority_bin);
    if (last_priority.priority_bin == priority.priority_bin) {
      EXPECT_LE(last_tile->required_for_activation(),
                tile->required_for_activation());
      if (last_tile->required_for_activation() ==
          tile->required_for_activation()) {
        if (last_priority.distance_to_visible >= priority.distance_to_visible)
          ++distance_decreasing;
        else
          ++distance_increasing;
      }
    }

    last_tile = tile;
    new_content_tiles.insert(tile);
    queue->Pop();
  }

  // Ensure that the distance is decreasing many more times than increasing.
  EXPECT_EQ(3, distance_increasing);
  EXPECT_EQ(17, distance_decreasing);
  EXPECT_EQ(tile_count, new_content_tiles.size());
  EXPECT_EQ(all_tiles, new_content_tiles);
}

TEST_F(TileManagerTilePriorityQueueTest,
       EvictionTilePriorityQueueWithOcclusion) {
  base::TimeTicks time_ticks;
  time_ticks += base::TimeDelta::FromMilliseconds(1);
  host_impl_.SetCurrentBeginFrameArgs(
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, time_ticks));

  gfx::Size tile_size(102, 102);
  gfx::Size layer_bounds(1000, 1000);

  host_impl_.SetViewportSize(layer_bounds);

  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  SetupPendingTree(pending_pile);

  scoped_ptr<FakePictureLayerImpl> pending_child =
      FakePictureLayerImpl::CreateWithRasterSource(host_impl_.pending_tree(), 2,
                                                   pending_pile);
  pending_layer_->AddChild(pending_child.Pass());

  FakePictureLayerImpl* pending_child_layer =
      static_cast<FakePictureLayerImpl*>(pending_layer_->children()[0]);
  pending_child_layer->SetDrawsContent(true);

  time_ticks += base::TimeDelta::FromMilliseconds(1);
  host_impl_.SetCurrentBeginFrameArgs(
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, time_ticks));
  bool update_lcd_text = false;
  host_impl_.pending_tree()->UpdateDrawProperties(update_lcd_text);

  ActivateTree();
  SetupPendingTree(pending_pile);

  FakePictureLayerImpl* active_child_layer =
      static_cast<FakePictureLayerImpl*>(active_layer_->children()[0]);

  std::set<Tile*> all_tiles;
  size_t tile_count = 0;
  scoped_ptr<RasterTilePriorityQueue> raster_queue(host_impl_.BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES, RasterTilePriorityQueue::Type::ALL));
  while (!raster_queue->IsEmpty()) {
    ++tile_count;
    EXPECT_TRUE(raster_queue->Top());
    all_tiles.insert(raster_queue->Top());
    raster_queue->Pop();
  }
  EXPECT_EQ(tile_count, all_tiles.size());
  EXPECT_EQ(32u, tile_count);

  pending_layer_->ResetAllTilesPriorities();

  // Renew all of the tile priorities.
  gfx::Rect viewport(layer_bounds);
  pending_layer_->HighResTiling()->ComputeTilePriorityRects(viewport, 1.0f, 1.0,
                                                            Occlusion());
  pending_layer_->LowResTiling()->ComputeTilePriorityRects(viewport, 1.0f, 1.0,
                                                           Occlusion());
  pending_child_layer->HighResTiling()->ComputeTilePriorityRects(
      viewport, 1.0f, 1.0, Occlusion());
  pending_child_layer->LowResTiling()->ComputeTilePriorityRects(
      viewport, 1.0f, 1.0, Occlusion());

  active_layer_->HighResTiling()->ComputeTilePriorityRects(viewport, 1.0f, 1.0,
                                                           Occlusion());
  active_layer_->LowResTiling()->ComputeTilePriorityRects(viewport, 1.0f, 1.0,
                                                          Occlusion());
  active_child_layer->HighResTiling()->ComputeTilePriorityRects(
      viewport, 1.0f, 1.0, Occlusion());
  active_child_layer->LowResTiling()->ComputeTilePriorityRects(
      viewport, 1.0f, 1.0, Occlusion());

  // Populate all tiles directly from the tilings.
  all_tiles.clear();
  std::vector<Tile*> pending_high_res_tiles =
      pending_layer_->HighResTiling()->AllTilesForTesting();
  all_tiles.insert(pending_high_res_tiles.begin(),
                   pending_high_res_tiles.end());

  std::vector<Tile*> pending_low_res_tiles =
      pending_layer_->LowResTiling()->AllTilesForTesting();
  all_tiles.insert(pending_low_res_tiles.begin(), pending_low_res_tiles.end());

  // Set all tiles on the pending_child_layer as occluded on the pending tree.
  std::vector<Tile*> pending_child_high_res_tiles =
      pending_child_layer->HighResTiling()->AllTilesForTesting();
  pending_child_layer->HighResTiling()->SetAllTilesOccludedForTesting();
  active_child_layer->HighResTiling()->SetAllTilesOccludedForTesting();
  all_tiles.insert(pending_child_high_res_tiles.begin(),
                   pending_child_high_res_tiles.end());

  std::vector<Tile*> pending_child_low_res_tiles =
      pending_child_layer->LowResTiling()->AllTilesForTesting();
  pending_child_layer->LowResTiling()->SetAllTilesOccludedForTesting();
  active_child_layer->LowResTiling()->SetAllTilesOccludedForTesting();
  all_tiles.insert(pending_child_low_res_tiles.begin(),
                   pending_child_low_res_tiles.end());

  tile_manager()->InitializeTilesWithResourcesForTesting(
      std::vector<Tile*>(all_tiles.begin(), all_tiles.end()));

  // Verify occlusion is considered by EvictionTilePriorityQueue.
  TreePriority tree_priority = NEW_CONTENT_TAKES_PRIORITY;
  size_t occluded_count = 0u;
  Tile* last_tile = NULL;
  scoped_ptr<EvictionTilePriorityQueue> queue(
      host_impl_.BuildEvictionQueue(tree_priority));
  while (!queue->IsEmpty()) {
    Tile* tile = queue->Top();
    if (!last_tile)
      last_tile = tile;

    bool tile_is_occluded = tile->is_occluded_combined();

    // The only way we will encounter an occluded tile after an unoccluded
    // tile is if the priorty bin decreased, the tile is required for
    // activation, or the scale changed.
    if (tile_is_occluded) {
      occluded_count++;

      bool last_tile_is_occluded = last_tile->is_occluded_combined();
      if (!last_tile_is_occluded) {
        TilePriority::PriorityBin tile_priority_bin =
            tile->priority_for_tree_priority(tree_priority).priority_bin;
        TilePriority::PriorityBin last_tile_priority_bin =
            last_tile->priority_for_tree_priority(tree_priority).priority_bin;

        EXPECT_TRUE((tile_priority_bin < last_tile_priority_bin) ||
                    tile->required_for_activation() ||
                    (tile->contents_scale() != last_tile->contents_scale()));
      }
    }
    last_tile = tile;
    queue->Pop();
  }
  size_t expected_occluded_count =
      pending_child_high_res_tiles.size() + pending_child_low_res_tiles.size();
  EXPECT_EQ(expected_occluded_count, occluded_count);
}

TEST_F(TileManagerTilePriorityQueueTest,
       EvictionTilePriorityQueueWithTransparentLayer) {
  base::TimeTicks time_ticks;
  time_ticks += base::TimeDelta::FromMilliseconds(1);
  host_impl_.SetCurrentBeginFrameArgs(
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, time_ticks));

  gfx::Size tile_size(102, 102);
  gfx::Size layer_bounds(1000, 1000);

  scoped_refptr<FakePicturePileImpl> pending_pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  SetupPendingTree(pending_pile);

  scoped_ptr<FakePictureLayerImpl> pending_child =
      FakePictureLayerImpl::CreateWithRasterSource(host_impl_.pending_tree(), 2,
                                                   pending_pile);
  FakePictureLayerImpl* pending_child_layer = pending_child.get();
  pending_layer_->AddChild(pending_child.Pass());

  // Create a fully transparent child layer so that its tile priorities are not
  // considered to be valid.
  pending_child_layer->SetDrawsContent(true);

  time_ticks += base::TimeDelta::FromMilliseconds(1);
  host_impl_.SetCurrentBeginFrameArgs(
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, time_ticks));
  bool update_lcd_text = false;
  host_impl_.pending_tree()->UpdateDrawProperties(update_lcd_text);

  pending_child_layer->SetOpacity(0.0);

  time_ticks += base::TimeDelta::FromMilliseconds(1);
  host_impl_.SetCurrentBeginFrameArgs(
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, time_ticks));
  host_impl_.pending_tree()->UpdateDrawProperties(update_lcd_text);

  // Renew all of the tile priorities.
  gfx::Rect viewport(layer_bounds);
  pending_layer_->HighResTiling()->ComputeTilePriorityRects(viewport, 1.0f, 1.0,
                                                            Occlusion());
  pending_layer_->LowResTiling()->ComputeTilePriorityRects(viewport, 1.0f, 1.0,
                                                           Occlusion());
  pending_child_layer->HighResTiling()->ComputeTilePriorityRects(
      viewport, 1.0f, 1.0, Occlusion());
  pending_child_layer->LowResTiling()->ComputeTilePriorityRects(
      viewport, 1.0f, 1.0, Occlusion());

  // Populate all tiles directly from the tilings.
  std::set<Tile*> all_pending_tiles;
  std::vector<Tile*> pending_high_res_tiles =
      pending_layer_->HighResTiling()->AllTilesForTesting();
  all_pending_tiles.insert(pending_high_res_tiles.begin(),
                           pending_high_res_tiles.end());
  EXPECT_EQ(16u, pending_high_res_tiles.size());

  std::vector<Tile*> pending_low_res_tiles =
      pending_layer_->LowResTiling()->AllTilesForTesting();
  all_pending_tiles.insert(pending_low_res_tiles.begin(),
                           pending_low_res_tiles.end());
  EXPECT_EQ(1u, pending_low_res_tiles.size());

  std::set<Tile*> all_pending_child_tiles;
  std::vector<Tile*> pending_child_high_res_tiles =
      pending_child_layer->HighResTiling()->AllTilesForTesting();
  all_pending_child_tiles.insert(pending_child_high_res_tiles.begin(),
                                 pending_child_high_res_tiles.end());
  EXPECT_EQ(16u, pending_child_high_res_tiles.size());

  std::vector<Tile*> pending_child_low_res_tiles =
      pending_child_layer->LowResTiling()->AllTilesForTesting();
  all_pending_child_tiles.insert(pending_child_low_res_tiles.begin(),
                                 pending_child_low_res_tiles.end());
  EXPECT_EQ(1u, pending_child_low_res_tiles.size());

  std::set<Tile*> all_tiles = all_pending_tiles;
  all_tiles.insert(all_pending_child_tiles.begin(),
                   all_pending_child_tiles.end());

  tile_manager()->InitializeTilesWithResourcesForTesting(
      std::vector<Tile*>(all_tiles.begin(), all_tiles.end()));

  EXPECT_TRUE(pending_layer_->HasValidTilePriorities());
  EXPECT_FALSE(pending_child_layer->HasValidTilePriorities());

  // Verify that eviction queue returns tiles also from layers without valid
  // tile priorities and that the tile priority bin of those tiles is (at most)
  // EVENTUALLY.
  TreePriority tree_priority = NEW_CONTENT_TAKES_PRIORITY;
  std::set<Tile*> new_content_tiles;
  size_t tile_count = 0;
  scoped_ptr<EvictionTilePriorityQueue> queue(
      host_impl_.BuildEvictionQueue(tree_priority));
  while (!queue->IsEmpty()) {
    Tile* tile = queue->Top();
    const TilePriority& pending_priority = tile->priority(PENDING_TREE);
    EXPECT_NE(std::numeric_limits<float>::infinity(),
              pending_priority.distance_to_visible);
    if (all_pending_child_tiles.find(tile) != all_pending_child_tiles.end())
      EXPECT_EQ(TilePriority::EVENTUALLY, pending_priority.priority_bin);
    else
      EXPECT_EQ(TilePriority::NOW, pending_priority.priority_bin);
    new_content_tiles.insert(tile);
    ++tile_count;
    queue->Pop();
  }
  EXPECT_EQ(tile_count, new_content_tiles.size());
  EXPECT_EQ(all_tiles, new_content_tiles);
}

TEST_F(TileManagerTilePriorityQueueTest, RasterTilePriorityQueueEmptyLayers) {
  const gfx::Size layer_bounds(1000, 1000);
  host_impl_.SetViewportSize(layer_bounds);
  SetupDefaultTrees(layer_bounds);

  scoped_ptr<RasterTilePriorityQueue> queue(host_impl_.BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES, RasterTilePriorityQueue::Type::ALL));
  EXPECT_FALSE(queue->IsEmpty());

  size_t tile_count = 0;
  std::set<Tile*> all_tiles;
  while (!queue->IsEmpty()) {
    EXPECT_TRUE(queue->Top());
    all_tiles.insert(queue->Top());
    ++tile_count;
    queue->Pop();
  }

  EXPECT_EQ(tile_count, all_tiles.size());
  EXPECT_EQ(16u, tile_count);

  for (int i = 1; i < 10; ++i) {
    scoped_ptr<FakePictureLayerImpl> pending_layer =
        FakePictureLayerImpl::Create(host_impl_.pending_tree(), id_ + i);
    pending_layer->SetDrawsContent(true);
    pending_layer->set_has_valid_tile_priorities(true);
    pending_layer_->AddChild(pending_layer.Pass());
  }

  queue = host_impl_.BuildRasterQueue(SAME_PRIORITY_FOR_BOTH_TREES,
                                      RasterTilePriorityQueue::Type::ALL);
  EXPECT_FALSE(queue->IsEmpty());

  tile_count = 0;
  all_tiles.clear();
  while (!queue->IsEmpty()) {
    EXPECT_TRUE(queue->Top());
    all_tiles.insert(queue->Top());
    ++tile_count;
    queue->Pop();
  }
  EXPECT_EQ(tile_count, all_tiles.size());
  EXPECT_EQ(16u, tile_count);
}

TEST_F(TileManagerTilePriorityQueueTest, EvictionTilePriorityQueueEmptyLayers) {
  const gfx::Size layer_bounds(1000, 1000);
  host_impl_.SetViewportSize(layer_bounds);
  SetupDefaultTrees(layer_bounds);

  scoped_ptr<RasterTilePriorityQueue> raster_queue(host_impl_.BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES, RasterTilePriorityQueue::Type::ALL));
  EXPECT_FALSE(raster_queue->IsEmpty());

  size_t tile_count = 0;
  std::set<Tile*> all_tiles;
  while (!raster_queue->IsEmpty()) {
    EXPECT_TRUE(raster_queue->Top());
    all_tiles.insert(raster_queue->Top());
    ++tile_count;
    raster_queue->Pop();
  }
  EXPECT_EQ(tile_count, all_tiles.size());
  EXPECT_EQ(16u, tile_count);

  std::vector<Tile*> tiles(all_tiles.begin(), all_tiles.end());
  host_impl_.tile_manager()->InitializeTilesWithResourcesForTesting(tiles);

  for (int i = 1; i < 10; ++i) {
    scoped_ptr<FakePictureLayerImpl> pending_layer =
        FakePictureLayerImpl::Create(host_impl_.pending_tree(), id_ + i);
    pending_layer->SetDrawsContent(true);
    pending_layer->set_has_valid_tile_priorities(true);
    pending_layer_->AddChild(pending_layer.Pass());
  }

  scoped_ptr<EvictionTilePriorityQueue> queue(
      host_impl_.BuildEvictionQueue(SAME_PRIORITY_FOR_BOTH_TREES));
  EXPECT_FALSE(queue->IsEmpty());

  tile_count = 0;
  all_tiles.clear();
  while (!queue->IsEmpty()) {
    EXPECT_TRUE(queue->Top());
    all_tiles.insert(queue->Top());
    ++tile_count;
    queue->Pop();
  }
  EXPECT_EQ(tile_count, all_tiles.size());
  EXPECT_EQ(16u, tile_count);
}

TEST_F(TileManagerTilePriorityQueueTest,
       RasterTilePriorityQueueStaticViewport) {
  FakePictureLayerTilingClient client;

  gfx::Rect viewport(50, 50, 500, 500);
  gfx::Size layer_bounds(1600, 1600);

  float inset = PictureLayerTiling::CalculateSoonBorderDistance(viewport, 1.0f);
  gfx::Rect soon_rect = viewport;
  soon_rect.Inset(-inset, -inset);

  client.SetTileSize(gfx::Size(30, 30));
  client.set_tree(ACTIVE_TREE);
  LayerTreeSettings settings;
  settings.max_tiles_for_interest_area = 10000;

  scoped_ptr<PictureLayerTilingSet> tiling_set = PictureLayerTilingSet::Create(
      &client, settings.max_tiles_for_interest_area,
      settings.skewport_target_time_in_seconds,
      settings.skewport_extrapolation_limit_in_content_pixels);

  scoped_refptr<FakePicturePileImpl> pile =
      FakePicturePileImpl::CreateFilledPileWithDefaultTileSize(layer_bounds);
  PictureLayerTiling* tiling = tiling_set->AddTiling(1.0f, pile);
  tiling->set_resolution(HIGH_RESOLUTION);

  tiling_set->UpdateTilePriorities(viewport, 1.0f, 1.0, Occlusion(), true);
  std::vector<Tile*> all_tiles = tiling->AllTilesForTesting();
  // Sanity check.
  EXPECT_EQ(3364u, all_tiles.size());

  // The explanation of each iteration is as follows:
  // 1. First iteration tests that we can get all of the tiles correctly.
  // 2. Second iteration ensures that we can get all of the tiles again (first
  //    iteration didn't change any tiles), as well set all tiles to be ready to
  //    draw.
  // 3. Third iteration ensures that no tiles are returned, since they were all
  //    marked as ready to draw.
  for (int i = 0; i < 3; ++i) {
    scoped_ptr<TilingSetRasterQueueAll> queue(
        new TilingSetRasterQueueAll(tiling_set.get(), false));

    // There are 3 bins in TilePriority.
    bool have_tiles[3] = {};

    // On the third iteration, we should get no tiles since everything was
    // marked as ready to draw.
    if (i == 2) {
      EXPECT_TRUE(queue->IsEmpty());
      continue;
    }

    EXPECT_FALSE(queue->IsEmpty());
    std::set<Tile*> unique_tiles;
    unique_tiles.insert(queue->Top());
    Tile* last_tile = queue->Top();
    have_tiles[last_tile->priority(ACTIVE_TREE).priority_bin] = true;

    // On the second iteration, mark everything as ready to draw (solid color).
    if (i == 1) {
      TileDrawInfo& draw_info = last_tile->draw_info();
      draw_info.SetSolidColorForTesting(SK_ColorRED);
    }
    queue->Pop();
    int eventually_bin_order_correct_count = 0;
    int eventually_bin_order_incorrect_count = 0;
    while (!queue->IsEmpty()) {
      Tile* new_tile = queue->Top();
      queue->Pop();
      unique_tiles.insert(new_tile);

      TilePriority last_priority = last_tile->priority(ACTIVE_TREE);
      TilePriority new_priority = new_tile->priority(ACTIVE_TREE);
      EXPECT_LE(last_priority.priority_bin, new_priority.priority_bin);
      if (last_priority.priority_bin == new_priority.priority_bin) {
        if (last_priority.priority_bin == TilePriority::EVENTUALLY) {
          bool order_correct = last_priority.distance_to_visible <=
                               new_priority.distance_to_visible;
          eventually_bin_order_correct_count += order_correct;
          eventually_bin_order_incorrect_count += !order_correct;
        } else if (!soon_rect.Intersects(new_tile->content_rect()) &&
                   !soon_rect.Intersects(last_tile->content_rect())) {
          EXPECT_LE(last_priority.distance_to_visible,
                    new_priority.distance_to_visible);
          EXPECT_EQ(TilePriority::NOW, new_priority.priority_bin);
        } else if (new_priority.distance_to_visible > 0.f) {
          EXPECT_EQ(TilePriority::SOON, new_priority.priority_bin);
        }
      }
      have_tiles[new_priority.priority_bin] = true;

      last_tile = new_tile;

      // On the second iteration, mark everything as ready to draw (solid
      // color).
      if (i == 1) {
        TileDrawInfo& draw_info = last_tile->draw_info();
        draw_info.SetSolidColorForTesting(SK_ColorRED);
      }
    }

    EXPECT_GT(eventually_bin_order_correct_count,
              eventually_bin_order_incorrect_count);

    // We should have now and eventually tiles, as well as soon tiles from
    // the border region.
    EXPECT_TRUE(have_tiles[TilePriority::NOW]);
    EXPECT_TRUE(have_tiles[TilePriority::SOON]);
    EXPECT_TRUE(have_tiles[TilePriority::EVENTUALLY]);

    EXPECT_EQ(unique_tiles.size(), all_tiles.size());
  }
}

TEST_F(TileManagerTilePriorityQueueTest,
       RasterTilePriorityQueueMovingViewport) {
  FakePictureLayerTilingClient client;

  gfx::Rect viewport(50, 0, 100, 100);
  gfx::Rect moved_viewport(50, 0, 100, 500);
  gfx::Size layer_bounds(1000, 1000);

  client.SetTileSize(gfx::Size(30, 30));
  client.set_tree(ACTIVE_TREE);
  LayerTreeSettings settings;
  settings.max_tiles_for_interest_area = 10000;

  scoped_ptr<PictureLayerTilingSet> tiling_set = PictureLayerTilingSet::Create(
      &client, settings.max_tiles_for_interest_area,
      settings.skewport_target_time_in_seconds,
      settings.skewport_extrapolation_limit_in_content_pixels);

  scoped_refptr<FakePicturePileImpl> pile =
      FakePicturePileImpl::CreateFilledPileWithDefaultTileSize(layer_bounds);
  PictureLayerTiling* tiling = tiling_set->AddTiling(1.0f, pile);
  tiling->set_resolution(HIGH_RESOLUTION);

  tiling_set->UpdateTilePriorities(viewport, 1.0f, 1.0, Occlusion(), true);
  tiling_set->UpdateTilePriorities(moved_viewport, 1.0f, 2.0, Occlusion(),
                                   true);

  float inset =
      PictureLayerTiling::CalculateSoonBorderDistance(moved_viewport, 1.0f);
  gfx::Rect soon_rect = moved_viewport;
  soon_rect.Inset(-inset, -inset);

  // There are 3 bins in TilePriority.
  bool have_tiles[3] = {};
  Tile* last_tile = NULL;
  int eventually_bin_order_correct_count = 0;
  int eventually_bin_order_incorrect_count = 0;
  scoped_ptr<TilingSetRasterQueueAll> queue(
      new TilingSetRasterQueueAll(tiling_set.get(), false));
  for (; !queue->IsEmpty(); queue->Pop()) {
    if (!last_tile)
      last_tile = queue->Top();

    Tile* new_tile = queue->Top();

    TilePriority last_priority = last_tile->priority(ACTIVE_TREE);
    TilePriority new_priority = new_tile->priority(ACTIVE_TREE);

    have_tiles[new_priority.priority_bin] = true;

    EXPECT_LE(last_priority.priority_bin, new_priority.priority_bin);
    if (last_priority.priority_bin == new_priority.priority_bin) {
      if (last_priority.priority_bin == TilePriority::EVENTUALLY) {
        bool order_correct = last_priority.distance_to_visible <=
                             new_priority.distance_to_visible;
        eventually_bin_order_correct_count += order_correct;
        eventually_bin_order_incorrect_count += !order_correct;
      } else if (!soon_rect.Intersects(new_tile->content_rect()) &&
                 !soon_rect.Intersects(last_tile->content_rect())) {
        EXPECT_LE(last_priority.distance_to_visible,
                  new_priority.distance_to_visible);
      } else if (new_priority.distance_to_visible > 0.f) {
        EXPECT_EQ(TilePriority::SOON, new_priority.priority_bin);
      }
    }
    last_tile = new_tile;
  }

  EXPECT_GT(eventually_bin_order_correct_count,
            eventually_bin_order_incorrect_count);

  EXPECT_TRUE(have_tiles[TilePriority::NOW]);
  EXPECT_TRUE(have_tiles[TilePriority::SOON]);
  EXPECT_TRUE(have_tiles[TilePriority::EVENTUALLY]);
}

}  // namespace
}  // namespace cc
