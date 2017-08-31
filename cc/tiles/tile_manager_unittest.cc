// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/layers/recording_source.h"
#include "cc/raster/raster_buffer.h"
#include "cc/raster/raster_source.h"
#include "cc/raster/synchronous_task_graph_runner.h"
#include "cc/resources/resource_pool.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/fake_layer_tree_frame_sink_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_picture_layer_tiling_client.h"
#include "cc/test/fake_raster_source.h"
#include "cc/test/fake_recording_source.h"
#include "cc/test/fake_tile_manager.h"
#include "cc/test/fake_tile_task_manager.h"
#include "cc/test/skia_common.h"
#include "cc/test/stub_paint_image_generator.h"
#include "cc/test/test_layer_tree_host_base.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/test/test_tile_priorities.h"
#include "cc/tiles/eviction_tile_priority_queue.h"
#include "cc/tiles/raster_tile_priority_queue.h"
#include "cc/tiles/tile.h"
#include "cc/tiles/tile_priority.h"
#include "cc/tiles/tiling_set_raster_queue_all.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkImageGenerator.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

namespace cc {
namespace {

// A version of simple task runner that lets the user control if all tasks
// posted should run synchronously.
class SynchronousSimpleTaskRunner : public base::TestSimpleTaskRunner {
 public:
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    TestSimpleTaskRunner::PostDelayedTask(from_here, std::move(task), delay);
    if (run_tasks_synchronously_)
      RunUntilIdle();
    return true;
  }

  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    return PostDelayedTask(from_here, std::move(task), delay);
  }

  void set_run_tasks_synchronously(bool run_tasks_synchronously) {
    run_tasks_synchronously_ = run_tasks_synchronously;
  }

 protected:
  ~SynchronousSimpleTaskRunner() override = default;

  bool run_tasks_synchronously_ = false;
};

class TileManagerTilePriorityQueueTest : public TestLayerTreeHostBase {
 public:
  LayerTreeSettings CreateSettings() override {
    LayerTreeSettings settings;
    settings.create_low_res_tiling = true;
    settings.resource_settings.buffer_to_texture_target_map =
        viz::DefaultBufferToTextureTargetMapForTesting();
    return settings;
  }

  TileManager* tile_manager() { return host_impl()->tile_manager(); }
};

TEST_F(TileManagerTilePriorityQueueTest, RasterTilePriorityQueue) {
  const gfx::Size layer_bounds(1000, 1000);
  host_impl()->SetViewportSize(layer_bounds);
  SetupDefaultTrees(layer_bounds);

  std::unique_ptr<RasterTilePriorityQueue> queue(host_impl()->BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES, RasterTilePriorityQueue::Type::ALL));
  EXPECT_FALSE(queue->IsEmpty());

  size_t tile_count = 0;
  std::set<Tile*> all_tiles;
  while (!queue->IsEmpty()) {
    EXPECT_TRUE(queue->Top().tile());
    all_tiles.insert(queue->Top().tile());
    ++tile_count;
    queue->Pop();
  }

  EXPECT_EQ(tile_count, all_tiles.size());
  EXPECT_EQ(16u, tile_count);

  // Sanity check, all tiles should be visible.
  std::set<Tile*> smoothness_tiles;
  queue = host_impl()->BuildRasterQueue(SMOOTHNESS_TAKES_PRIORITY,
                                        RasterTilePriorityQueue::Type::ALL);
  bool had_low_res = false;
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    EXPECT_TRUE(prioritized_tile.tile());
    EXPECT_EQ(TilePriority::NOW, prioritized_tile.priority().priority_bin);
    if (prioritized_tile.priority().resolution == LOW_RESOLUTION)
      had_low_res = true;
    else
      smoothness_tiles.insert(prioritized_tile.tile());
    queue->Pop();
  }
  EXPECT_EQ(all_tiles, smoothness_tiles);
  EXPECT_TRUE(had_low_res);

  // Check that everything is required for activation.
  queue = host_impl()->BuildRasterQueue(
      SMOOTHNESS_TAKES_PRIORITY,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_ACTIVATION);
  std::set<Tile*> required_for_activation_tiles;
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    EXPECT_TRUE(prioritized_tile.tile()->required_for_activation());
    required_for_activation_tiles.insert(prioritized_tile.tile());
    queue->Pop();
  }
  EXPECT_EQ(all_tiles, required_for_activation_tiles);

  // Check that everything is required for draw.
  queue = host_impl()->BuildRasterQueue(
      SMOOTHNESS_TAKES_PRIORITY,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_DRAW);
  std::set<Tile*> required_for_draw_tiles;
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    EXPECT_TRUE(prioritized_tile.tile()->required_for_draw());
    required_for_draw_tiles.insert(prioritized_tile.tile());
    queue->Pop();
  }
  EXPECT_EQ(all_tiles, required_for_draw_tiles);

  Region invalidation(gfx::Rect(0, 0, 500, 500));

  // Invalidate the pending tree.
  pending_layer()->set_invalidation(invalidation);
  pending_layer()->HighResTiling()->Invalidate(invalidation);

  // Renew all of the tile priorities.
  gfx::Rect viewport(50, 50, 100, 100);
  pending_layer()->picture_layer_tiling_set()->UpdateTilePriorities(
      viewport, 1.0f, 1.0, Occlusion(), true);
  active_layer()->picture_layer_tiling_set()->UpdateTilePriorities(
      viewport, 1.0f, 1.0, Occlusion(), true);

  // Populate all tiles directly from the tilings.
  all_tiles.clear();
  std::set<Tile*> high_res_tiles;
  std::vector<Tile*> pending_high_res_tiles =
      pending_layer()->HighResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < pending_high_res_tiles.size(); ++i) {
    all_tiles.insert(pending_high_res_tiles[i]);
    high_res_tiles.insert(pending_high_res_tiles[i]);
  }

  std::vector<Tile*> active_high_res_tiles =
      active_layer()->HighResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < active_high_res_tiles.size(); ++i) {
    all_tiles.insert(active_high_res_tiles[i]);
    high_res_tiles.insert(active_high_res_tiles[i]);
  }

  std::vector<Tile*> active_low_res_tiles =
      active_layer()->LowResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < active_low_res_tiles.size(); ++i)
    all_tiles.insert(active_low_res_tiles[i]);

  PrioritizedTile last_tile;
  smoothness_tiles.clear();
  tile_count = 0;
  size_t correct_order_tiles = 0u;
  // Here we expect to get increasing ACTIVE_TREE priority_bin.
  queue = host_impl()->BuildRasterQueue(SMOOTHNESS_TAKES_PRIORITY,
                                        RasterTilePriorityQueue::Type::ALL);
  std::set<Tile*> expected_required_for_draw_tiles;
  std::set<Tile*> expected_required_for_activation_tiles;
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    EXPECT_TRUE(prioritized_tile.tile());

    if (!last_tile.tile())
      last_tile = prioritized_tile;

    EXPECT_LE(last_tile.priority().priority_bin,
              prioritized_tile.priority().priority_bin);
    bool skip_updating_last_tile = false;
    if (last_tile.priority().priority_bin ==
        prioritized_tile.priority().priority_bin) {
      correct_order_tiles += last_tile.priority().distance_to_visible <=
                             prioritized_tile.priority().distance_to_visible;
    } else if (prioritized_tile.priority().priority_bin == TilePriority::NOW) {
      // Since we'd return pending tree now tiles before the eventually tiles on
      // the active tree, update the value.
      ++correct_order_tiles;
      skip_updating_last_tile = true;
    }

    if (prioritized_tile.priority().priority_bin == TilePriority::NOW &&
        last_tile.priority().resolution !=
            prioritized_tile.priority().resolution) {
      // Low resolution should come first.
      EXPECT_EQ(LOW_RESOLUTION, last_tile.priority().resolution);
    }

    if (!skip_updating_last_tile)
      last_tile = prioritized_tile;
    ++tile_count;
    smoothness_tiles.insert(prioritized_tile.tile());
    if (prioritized_tile.tile()->required_for_draw())
      expected_required_for_draw_tiles.insert(prioritized_tile.tile());
    if (prioritized_tile.tile()->required_for_activation())
      expected_required_for_activation_tiles.insert(prioritized_tile.tile());
    queue->Pop();
  }

  EXPECT_EQ(tile_count, smoothness_tiles.size());
  EXPECT_EQ(all_tiles, smoothness_tiles);
  // Since we don't guarantee increasing distance due to spiral iterator, we
  // should check that we're _mostly_ right.
  EXPECT_GT(correct_order_tiles, 3 * tile_count / 4);

  // Check that we have consistent required_for_activation tiles.
  queue = host_impl()->BuildRasterQueue(
      SMOOTHNESS_TAKES_PRIORITY,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_ACTIVATION);
  required_for_activation_tiles.clear();
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    EXPECT_TRUE(prioritized_tile.tile()->required_for_activation());
    required_for_activation_tiles.insert(prioritized_tile.tile());
    queue->Pop();
  }
  EXPECT_EQ(expected_required_for_activation_tiles,
            required_for_activation_tiles);
  EXPECT_NE(all_tiles, required_for_activation_tiles);

  // Check that we have consistent required_for_draw tiles.
  queue = host_impl()->BuildRasterQueue(
      SMOOTHNESS_TAKES_PRIORITY,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_DRAW);
  required_for_draw_tiles.clear();
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    EXPECT_TRUE(prioritized_tile.tile()->required_for_draw());
    required_for_draw_tiles.insert(prioritized_tile.tile());
    queue->Pop();
  }
  EXPECT_EQ(expected_required_for_draw_tiles, required_for_draw_tiles);
  EXPECT_NE(all_tiles, required_for_draw_tiles);

  std::set<Tile*> new_content_tiles;
  last_tile = PrioritizedTile();
  size_t increasing_distance_tiles = 0u;
  // Here we expect to get increasing PENDING_TREE priority_bin.
  queue = host_impl()->BuildRasterQueue(NEW_CONTENT_TAKES_PRIORITY,
                                        RasterTilePriorityQueue::Type::ALL);
  tile_count = 0;
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    EXPECT_TRUE(prioritized_tile.tile());

    if (!last_tile.tile())
      last_tile = prioritized_tile;

    EXPECT_LE(last_tile.priority().priority_bin,
              prioritized_tile.priority().priority_bin);
    if (last_tile.priority().priority_bin ==
        prioritized_tile.priority().priority_bin) {
      increasing_distance_tiles +=
          last_tile.priority().distance_to_visible <=
          prioritized_tile.priority().distance_to_visible;
    }

    if (prioritized_tile.priority().priority_bin == TilePriority::NOW &&
        last_tile.priority().resolution !=
            prioritized_tile.priority().resolution) {
      // High resolution should come first.
      EXPECT_EQ(HIGH_RESOLUTION, last_tile.priority().resolution);
    }

    last_tile = prioritized_tile;
    new_content_tiles.insert(prioritized_tile.tile());
    ++tile_count;
    queue->Pop();
  }

  EXPECT_EQ(tile_count, new_content_tiles.size());
  EXPECT_EQ(high_res_tiles, new_content_tiles);
  // Since we don't guarantee increasing distance due to spiral iterator, we
  // should check that we're _mostly_ right.
  EXPECT_GE(increasing_distance_tiles, 3 * tile_count / 4);

  // Check that we have consistent required_for_activation tiles.
  queue = host_impl()->BuildRasterQueue(
      NEW_CONTENT_TAKES_PRIORITY,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_ACTIVATION);
  required_for_activation_tiles.clear();
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    EXPECT_TRUE(prioritized_tile.tile()->required_for_activation());
    required_for_activation_tiles.insert(prioritized_tile.tile());
    queue->Pop();
  }
  EXPECT_EQ(expected_required_for_activation_tiles,
            required_for_activation_tiles);
  EXPECT_NE(new_content_tiles, required_for_activation_tiles);

  // Check that we have consistent required_for_draw tiles.
  queue = host_impl()->BuildRasterQueue(
      NEW_CONTENT_TAKES_PRIORITY,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_DRAW);
  required_for_draw_tiles.clear();
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    EXPECT_TRUE(prioritized_tile.tile()->required_for_draw());
    required_for_draw_tiles.insert(prioritized_tile.tile());
    queue->Pop();
  }
  EXPECT_EQ(expected_required_for_draw_tiles, required_for_draw_tiles);
  EXPECT_NE(new_content_tiles, required_for_draw_tiles);
}

TEST_F(TileManagerTilePriorityQueueTest,
       RasterTilePriorityQueueHighNonIdealTilings) {
  const gfx::Size layer_bounds(1000, 1000);
  const gfx::Size viewport(800, 800);
  host_impl()->SetViewportSize(viewport);
  SetupDefaultTrees(layer_bounds);

  pending_layer()->tilings()->AddTiling(
      gfx::AxisTransform2d(1.5f, gfx::Vector2dF()),
      pending_layer()->raster_source());
  active_layer()->tilings()->AddTiling(
      gfx::AxisTransform2d(1.5f, gfx::Vector2dF()),
      active_layer()->raster_source());
  pending_layer()->tilings()->AddTiling(
      gfx::AxisTransform2d(1.7f, gfx::Vector2dF()),
      pending_layer()->raster_source());
  active_layer()->tilings()->AddTiling(
      gfx::AxisTransform2d(1.7f, gfx::Vector2dF()),
      active_layer()->raster_source());

  pending_layer()->tilings()->UpdateTilePriorities(gfx::Rect(viewport), 1.f,
                                                   5.0, Occlusion(), true);
  active_layer()->tilings()->UpdateTilePriorities(gfx::Rect(viewport), 1.f, 5.0,
                                                  Occlusion(), true);

  std::set<Tile*> all_expected_tiles;
  for (size_t i = 0; i < pending_layer()->num_tilings(); ++i) {
    PictureLayerTiling* tiling = pending_layer()->tilings()->tiling_at(i);
    if (tiling->contents_scale_key() == 1.f) {
      tiling->set_resolution(HIGH_RESOLUTION);
      const auto& all_tiles = tiling->AllTilesForTesting();
      all_expected_tiles.insert(all_tiles.begin(), all_tiles.end());
    } else {
      tiling->set_resolution(NON_IDEAL_RESOLUTION);
    }
  }

  for (size_t i = 0; i < active_layer()->num_tilings(); ++i) {
    PictureLayerTiling* tiling = active_layer()->tilings()->tiling_at(i);
    if (tiling->contents_scale_key() == 1.5f) {
      tiling->set_resolution(HIGH_RESOLUTION);
      const auto& all_tiles = tiling->AllTilesForTesting();
      all_expected_tiles.insert(all_tiles.begin(), all_tiles.end());
    } else {
      tiling->set_resolution(NON_IDEAL_RESOLUTION);
      // Non ideal tilings with a high res pending twin have to be processed
      // because of possible activation tiles.
      if (tiling->contents_scale_key() == 1.f) {
        tiling->UpdateAndGetAllPrioritizedTilesForTesting();
        const auto& all_tiles = tiling->AllTilesForTesting();
        for (auto* tile : all_tiles)
          EXPECT_TRUE(tile->required_for_activation());
        all_expected_tiles.insert(all_tiles.begin(), all_tiles.end());
      }
    }
  }

  std::unique_ptr<RasterTilePriorityQueue> queue(host_impl()->BuildRasterQueue(
      SMOOTHNESS_TAKES_PRIORITY, RasterTilePriorityQueue::Type::ALL));
  EXPECT_FALSE(queue->IsEmpty());

  size_t tile_count = 0;
  std::set<Tile*> all_actual_tiles;
  while (!queue->IsEmpty()) {
    EXPECT_TRUE(queue->Top().tile());
    all_actual_tiles.insert(queue->Top().tile());
    ++tile_count;
    queue->Pop();
  }

  EXPECT_EQ(tile_count, all_actual_tiles.size());
  EXPECT_EQ(all_expected_tiles.size(), all_actual_tiles.size());
  EXPECT_EQ(all_expected_tiles, all_actual_tiles);
}

TEST_F(TileManagerTilePriorityQueueTest,
       RasterTilePriorityQueueHighLowTilings) {
  const gfx::Size layer_bounds(1000, 1000);
  const gfx::Size viewport(800, 800);
  host_impl()->SetViewportSize(viewport);
  SetupDefaultTrees(layer_bounds);

  pending_layer()->tilings()->AddTiling(
      gfx::AxisTransform2d(1.5f, gfx::Vector2dF()),
      pending_layer()->raster_source());
  active_layer()->tilings()->AddTiling(
      gfx::AxisTransform2d(1.5f, gfx::Vector2dF()),
      active_layer()->raster_source());
  pending_layer()->tilings()->AddTiling(
      gfx::AxisTransform2d(1.7f, gfx::Vector2dF()),
      pending_layer()->raster_source());
  active_layer()->tilings()->AddTiling(
      gfx::AxisTransform2d(1.7f, gfx::Vector2dF()),
      active_layer()->raster_source());

  pending_layer()->tilings()->UpdateTilePriorities(gfx::Rect(viewport), 1.f,
                                                   5.0, Occlusion(), true);
  active_layer()->tilings()->UpdateTilePriorities(gfx::Rect(viewport), 1.f, 5.0,
                                                  Occlusion(), true);

  std::set<Tile*> all_expected_tiles;
  for (size_t i = 0; i < pending_layer()->num_tilings(); ++i) {
    PictureLayerTiling* tiling = pending_layer()->tilings()->tiling_at(i);
    if (tiling->contents_scale_key() == 1.f) {
      tiling->set_resolution(HIGH_RESOLUTION);
      const auto& all_tiles = tiling->AllTilesForTesting();
      all_expected_tiles.insert(all_tiles.begin(), all_tiles.end());
    } else {
      tiling->set_resolution(NON_IDEAL_RESOLUTION);
    }
  }

  for (size_t i = 0; i < active_layer()->num_tilings(); ++i) {
    PictureLayerTiling* tiling = active_layer()->tilings()->tiling_at(i);
    if (tiling->contents_scale_key() == 1.5f) {
      tiling->set_resolution(HIGH_RESOLUTION);
      const auto& all_tiles = tiling->AllTilesForTesting();
      all_expected_tiles.insert(all_tiles.begin(), all_tiles.end());
    } else {
      tiling->set_resolution(LOW_RESOLUTION);
      // Low res tilings with a high res pending twin have to be processed
      // because of possible activation tiles.
      if (tiling->contents_scale_key() == 1.f) {
        tiling->UpdateAndGetAllPrioritizedTilesForTesting();
        const auto& all_tiles = tiling->AllTilesForTesting();
        for (auto* tile : all_tiles)
          EXPECT_TRUE(tile->required_for_activation());
        all_expected_tiles.insert(all_tiles.begin(), all_tiles.end());
      }
    }
  }

  std::unique_ptr<RasterTilePriorityQueue> queue(host_impl()->BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES, RasterTilePriorityQueue::Type::ALL));
  EXPECT_FALSE(queue->IsEmpty());

  size_t tile_count = 0;
  std::set<Tile*> all_actual_tiles;
  while (!queue->IsEmpty()) {
    EXPECT_TRUE(queue->Top().tile());
    all_actual_tiles.insert(queue->Top().tile());
    ++tile_count;
    queue->Pop();
  }

  EXPECT_EQ(tile_count, all_actual_tiles.size());
  EXPECT_EQ(all_expected_tiles.size(), all_actual_tiles.size());
  EXPECT_EQ(all_expected_tiles, all_actual_tiles);
}

TEST_F(TileManagerTilePriorityQueueTest, RasterTilePriorityQueueInvalidation) {
  const gfx::Size layer_bounds(1000, 1000);
  host_impl()->SetViewportSize(gfx::Size(500, 500));
  SetupDefaultTrees(layer_bounds);

  // Use a tile's content rect as an invalidation. We should inset it a bit to
  // ensure that border math doesn't invalidate neighbouring tiles.
  gfx::Rect invalidation =
      active_layer()->HighResTiling()->TileAt(1, 0)->content_rect();
  invalidation.Inset(2, 2);

  pending_layer()->set_invalidation(invalidation);
  pending_layer()->HighResTiling()->Invalidate(invalidation);
  pending_layer()->HighResTiling()->CreateMissingTilesInLiveTilesRect();

  // Sanity checks: Tile at 0, 0 not exist on the pending tree (it's not
  // invalidated). Tile 1, 0 should exist on both.
  EXPECT_FALSE(pending_layer()->HighResTiling()->TileAt(0, 0));
  EXPECT_TRUE(active_layer()->HighResTiling()->TileAt(0, 0));
  EXPECT_TRUE(pending_layer()->HighResTiling()->TileAt(1, 0));
  EXPECT_TRUE(active_layer()->HighResTiling()->TileAt(1, 0));
  EXPECT_NE(pending_layer()->HighResTiling()->TileAt(1, 0),
            active_layer()->HighResTiling()->TileAt(1, 0));

  std::set<Tile*> expected_now_tiles;
  std::set<Tile*> expected_required_for_draw_tiles;
  std::set<Tile*> expected_required_for_activation_tiles;
  for (int i = 0; i <= 1; ++i) {
    for (int j = 0; j <= 1; ++j) {
      bool have_pending_tile = false;
      if (pending_layer()->HighResTiling()->TileAt(i, j)) {
        expected_now_tiles.insert(
            pending_layer()->HighResTiling()->TileAt(i, j));
        expected_required_for_activation_tiles.insert(
            pending_layer()->HighResTiling()->TileAt(i, j));
        have_pending_tile = true;
      }
      Tile* active_tile = active_layer()->HighResTiling()->TileAt(i, j);
      EXPECT_TRUE(active_tile);
      expected_now_tiles.insert(active_tile);
      expected_required_for_draw_tiles.insert(active_tile);
      if (!have_pending_tile)
        expected_required_for_activation_tiles.insert(active_tile);
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
      if (pending_layer()->HighResTiling()->TileAt(i, j))
        expected_all_tiles.insert(
            pending_layer()->HighResTiling()->TileAt(i, j));
      EXPECT_TRUE(active_layer()->HighResTiling()->TileAt(i, j));
      expected_all_tiles.insert(active_layer()->HighResTiling()->TileAt(i, j));
    }
  }
  // Expect 15 shared tiles and 1 unshared tile.
  EXPECT_EQ(17u, expected_all_tiles.size());

  // The actual test will now build different queues and verify that the queues
  // return the same information as computed manually above.
  std::unique_ptr<RasterTilePriorityQueue> queue(host_impl()->BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES, RasterTilePriorityQueue::Type::ALL));
  std::set<Tile*> actual_now_tiles;
  std::set<Tile*> actual_all_tiles;
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    queue->Pop();
    if (prioritized_tile.priority().priority_bin == TilePriority::NOW)
      actual_now_tiles.insert(prioritized_tile.tile());
    actual_all_tiles.insert(prioritized_tile.tile());
  }
  EXPECT_EQ(expected_now_tiles, actual_now_tiles);
  EXPECT_EQ(expected_all_tiles, actual_all_tiles);

  queue = host_impl()->BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_DRAW);
  std::set<Tile*> actual_required_for_draw_tiles;
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    queue->Pop();
    actual_required_for_draw_tiles.insert(prioritized_tile.tile());
  }
  EXPECT_EQ(expected_required_for_draw_tiles, actual_required_for_draw_tiles);

  queue = host_impl()->BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_ACTIVATION);
  std::set<Tile*> actual_required_for_activation_tiles;
  while (!queue->IsEmpty()) {
    Tile* tile = queue->Top().tile();
    queue->Pop();
    actual_required_for_activation_tiles.insert(tile);
  }
  EXPECT_EQ(expected_required_for_activation_tiles,
            actual_required_for_activation_tiles);
}

TEST_F(TileManagerTilePriorityQueueTest, ActivationComesBeforeSoon) {
  host_impl()->AdvanceToNextFrame(base::TimeDelta::FromMilliseconds(1));

  gfx::Size layer_bounds(1000, 1000);
  SetupDefaultTrees(layer_bounds);

  // Create a pending child layer.
  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  std::unique_ptr<FakePictureLayerImpl> pending_child =
      FakePictureLayerImpl::CreateWithRasterSource(
          host_impl()->pending_tree(), layer_id() + 1, pending_raster_source);
  FakePictureLayerImpl* pending_child_raw = pending_child.get();
  pending_child_raw->SetDrawsContent(true);
  pending_layer()->test_properties()->AddChild(std::move(pending_child));

  // Set a small viewport, so we have soon and eventually tiles.
  host_impl()->SetViewportSize(gfx::Size(200, 200));
  host_impl()->AdvanceToNextFrame(base::TimeDelta::FromMilliseconds(1));
  host_impl()->pending_tree()->property_trees()->needs_rebuild = true;
  host_impl()->pending_tree()->BuildLayerListAndPropertyTreesForTesting();
  host_impl()->pending_tree()->UpdateDrawProperties();

  host_impl()->SetRequiresHighResToDraw();
  std::unique_ptr<RasterTilePriorityQueue> queue(host_impl()->BuildRasterQueue(
      SMOOTHNESS_TAKES_PRIORITY, RasterTilePriorityQueue::Type::ALL));
  EXPECT_FALSE(queue->IsEmpty());

  // Get all the tiles that are NOW and make sure they are ready to draw.
  std::vector<Tile*> all_tiles;
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    if (prioritized_tile.priority().priority_bin >= TilePriority::SOON)
      break;

    all_tiles.push_back(prioritized_tile.tile());
    queue->Pop();
  }

  tile_manager()->InitializeTilesWithResourcesForTesting(
      std::vector<Tile*>(all_tiles.begin(), all_tiles.end()));

  // Ensure we can activate.
  EXPECT_TRUE(tile_manager()->IsReadyToActivate());
}

TEST_F(TileManagerTilePriorityQueueTest, EvictionTilePriorityQueue) {
  const gfx::Size layer_bounds(1000, 1000);
  host_impl()->SetViewportSize(layer_bounds);
  SetupDefaultTrees(layer_bounds);
  ASSERT_TRUE(active_layer()->HighResTiling());
  ASSERT_TRUE(active_layer()->LowResTiling());
  ASSERT_TRUE(pending_layer()->HighResTiling());
  EXPECT_FALSE(pending_layer()->LowResTiling());

  std::unique_ptr<EvictionTilePriorityQueue> empty_queue(
      host_impl()->BuildEvictionQueue(SAME_PRIORITY_FOR_BOTH_TREES));
  EXPECT_TRUE(empty_queue->IsEmpty());
  std::set<Tile*> all_tiles;
  size_t tile_count = 0;

  std::unique_ptr<RasterTilePriorityQueue> raster_queue(
      host_impl()->BuildRasterQueue(SAME_PRIORITY_FOR_BOTH_TREES,
                                    RasterTilePriorityQueue::Type::ALL));
  while (!raster_queue->IsEmpty()) {
    ++tile_count;
    EXPECT_TRUE(raster_queue->Top().tile());
    all_tiles.insert(raster_queue->Top().tile());
    raster_queue->Pop();
  }

  EXPECT_EQ(tile_count, all_tiles.size());
  EXPECT_EQ(16u, tile_count);

  tile_manager()->InitializeTilesWithResourcesForTesting(
      std::vector<Tile*>(all_tiles.begin(), all_tiles.end()));

  std::unique_ptr<EvictionTilePriorityQueue> queue(
      host_impl()->BuildEvictionQueue(SMOOTHNESS_TAKES_PRIORITY));
  EXPECT_FALSE(queue->IsEmpty());

  // Sanity check, all tiles should be visible.
  std::set<Tile*> smoothness_tiles;
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    EXPECT_TRUE(prioritized_tile.tile());
    EXPECT_EQ(TilePriority::NOW, prioritized_tile.priority().priority_bin);
    EXPECT_TRUE(prioritized_tile.tile()->draw_info().has_resource());
    smoothness_tiles.insert(prioritized_tile.tile());
    queue->Pop();
  }
  EXPECT_EQ(all_tiles, smoothness_tiles);

  tile_manager()->ReleaseTileResourcesForTesting(
      std::vector<Tile*>(all_tiles.begin(), all_tiles.end()));

  Region invalidation(gfx::Rect(0, 0, 500, 500));

  // Invalidate the pending tree.
  pending_layer()->set_invalidation(invalidation);
  pending_layer()->HighResTiling()->Invalidate(invalidation);
  pending_layer()->HighResTiling()->CreateMissingTilesInLiveTilesRect();
  EXPECT_FALSE(pending_layer()->LowResTiling());

  // Renew all of the tile priorities.
  gfx::Rect viewport(50, 50, 100, 100);
  pending_layer()->picture_layer_tiling_set()->UpdateTilePriorities(
      viewport, 1.0f, 1.0, Occlusion(), true);
  active_layer()->picture_layer_tiling_set()->UpdateTilePriorities(
      viewport, 1.0f, 1.0, Occlusion(), true);

  // Populate all tiles directly from the tilings.
  all_tiles.clear();
  std::vector<Tile*> pending_high_res_tiles =
      pending_layer()->HighResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < pending_high_res_tiles.size(); ++i)
    all_tiles.insert(pending_high_res_tiles[i]);

  std::vector<Tile*> active_high_res_tiles =
      active_layer()->HighResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < active_high_res_tiles.size(); ++i)
    all_tiles.insert(active_high_res_tiles[i]);

  std::vector<Tile*> active_low_res_tiles =
      active_layer()->LowResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < active_low_res_tiles.size(); ++i)
    all_tiles.insert(active_low_res_tiles[i]);

  tile_manager()->InitializeTilesWithResourcesForTesting(
      std::vector<Tile*>(all_tiles.begin(), all_tiles.end()));

  PrioritizedTile last_tile;
  smoothness_tiles.clear();
  tile_count = 0;
  // Here we expect to get increasing combined priority_bin.
  queue = host_impl()->BuildEvictionQueue(SMOOTHNESS_TAKES_PRIORITY);
  int distance_increasing = 0;
  int distance_decreasing = 0;
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    Tile* tile = prioritized_tile.tile();
    EXPECT_TRUE(tile);
    EXPECT_TRUE(tile->draw_info().has_resource());

    if (!last_tile.tile())
      last_tile = prioritized_tile;

    const TilePriority& last_priority = last_tile.priority();
    const TilePriority& priority = prioritized_tile.priority();

    EXPECT_GE(last_priority.priority_bin, priority.priority_bin);
    if (last_priority.priority_bin == priority.priority_bin) {
      EXPECT_LE(last_tile.tile()->required_for_activation(),
                tile->required_for_activation());
      if (last_tile.tile()->required_for_activation() ==
          tile->required_for_activation()) {
        if (last_priority.distance_to_visible >= priority.distance_to_visible)
          ++distance_decreasing;
        else
          ++distance_increasing;
      }
    }

    last_tile = prioritized_tile;
    ++tile_count;
    smoothness_tiles.insert(tile);
    queue->Pop();
  }

  // Ensure that the distance is decreasing many more times than increasing.
  EXPECT_EQ(3, distance_increasing);
  EXPECT_EQ(16, distance_decreasing);
  EXPECT_EQ(tile_count, smoothness_tiles.size());
  EXPECT_EQ(all_tiles, smoothness_tiles);

  std::set<Tile*> new_content_tiles;
  last_tile = PrioritizedTile();
  // Again, we expect to get increasing combined priority_bin.
  queue = host_impl()->BuildEvictionQueue(NEW_CONTENT_TAKES_PRIORITY);
  distance_decreasing = 0;
  distance_increasing = 0;
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    Tile* tile = prioritized_tile.tile();
    EXPECT_TRUE(tile);

    if (!last_tile.tile())
      last_tile = prioritized_tile;

    const TilePriority& last_priority = last_tile.priority();
    const TilePriority& priority = prioritized_tile.priority();

    EXPECT_GE(last_priority.priority_bin, priority.priority_bin);
    if (last_priority.priority_bin == priority.priority_bin) {
      EXPECT_LE(last_tile.tile()->required_for_activation(),
                tile->required_for_activation());
      if (last_tile.tile()->required_for_activation() ==
          tile->required_for_activation()) {
        if (last_priority.distance_to_visible >= priority.distance_to_visible)
          ++distance_decreasing;
        else
          ++distance_increasing;
      }
    }

    last_tile = prioritized_tile;
    new_content_tiles.insert(tile);
    queue->Pop();
  }

  // Ensure that the distance is decreasing many more times than increasing.
  EXPECT_EQ(3, distance_increasing);
  EXPECT_EQ(16, distance_decreasing);
  EXPECT_EQ(tile_count, new_content_tiles.size());
  EXPECT_EQ(all_tiles, new_content_tiles);
}

TEST_F(TileManagerTilePriorityQueueTest,
       EvictionTilePriorityQueueWithOcclusion) {
  host_impl()->AdvanceToNextFrame(base::TimeDelta::FromMilliseconds(1));

  gfx::Size layer_bounds(1000, 1000);

  host_impl()->SetViewportSize(layer_bounds);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  SetupPendingTree(pending_raster_source);

  std::unique_ptr<FakePictureLayerImpl> pending_child =
      FakePictureLayerImpl::CreateWithRasterSource(host_impl()->pending_tree(),
                                                   2, pending_raster_source);
  int child_id = pending_child->id();
  pending_layer()->test_properties()->AddChild(std::move(pending_child));

  FakePictureLayerImpl* pending_child_layer =
      static_cast<FakePictureLayerImpl*>(
          pending_layer()->test_properties()->children[0]);
  pending_child_layer->SetDrawsContent(true);

  host_impl()->AdvanceToNextFrame(base::TimeDelta::FromMilliseconds(1));
  host_impl()->pending_tree()->property_trees()->needs_rebuild = true;
  host_impl()->pending_tree()->BuildLayerListAndPropertyTreesForTesting();
  host_impl()->pending_tree()->UpdateDrawProperties();

  ActivateTree();
  SetupPendingTree(pending_raster_source);

  FakePictureLayerImpl* active_child_layer = static_cast<FakePictureLayerImpl*>(
      host_impl()->active_tree()->LayerById(child_id));

  std::set<Tile*> all_tiles;
  size_t tile_count = 0;
  std::unique_ptr<RasterTilePriorityQueue> raster_queue(
      host_impl()->BuildRasterQueue(SAME_PRIORITY_FOR_BOTH_TREES,
                                    RasterTilePriorityQueue::Type::ALL));
  while (!raster_queue->IsEmpty()) {
    ++tile_count;
    EXPECT_TRUE(raster_queue->Top().tile());
    all_tiles.insert(raster_queue->Top().tile());
    raster_queue->Pop();
  }
  EXPECT_EQ(tile_count, all_tiles.size());
  EXPECT_EQ(32u, tile_count);

  // Renew all of the tile priorities.
  gfx::Rect viewport(layer_bounds);
  pending_layer()->picture_layer_tiling_set()->UpdateTilePriorities(
      viewport, 1.0f, 1.0, Occlusion(), true);
  pending_child_layer->picture_layer_tiling_set()->UpdateTilePriorities(
      viewport, 1.0f, 1.0, Occlusion(), true);

  active_layer()->picture_layer_tiling_set()->UpdateTilePriorities(
      viewport, 1.0f, 1.0, Occlusion(), true);
  active_child_layer->picture_layer_tiling_set()->UpdateTilePriorities(
      viewport, 1.0f, 1.0, Occlusion(), true);

  // Populate all tiles directly from the tilings.
  all_tiles.clear();
  std::vector<Tile*> pending_high_res_tiles =
      pending_layer()->HighResTiling()->AllTilesForTesting();
  all_tiles.insert(pending_high_res_tiles.begin(),
                   pending_high_res_tiles.end());

  // Set all tiles on the pending_child_layer as occluded on the pending tree.
  std::vector<Tile*> pending_child_high_res_tiles =
      pending_child_layer->HighResTiling()->AllTilesForTesting();
  pending_child_layer->HighResTiling()->SetAllTilesOccludedForTesting();
  active_child_layer->HighResTiling()->SetAllTilesOccludedForTesting();
  active_child_layer->LowResTiling()->SetAllTilesOccludedForTesting();

  tile_manager()->InitializeTilesWithResourcesForTesting(
      std::vector<Tile*>(all_tiles.begin(), all_tiles.end()));

  // Verify occlusion is considered by EvictionTilePriorityQueue.
  TreePriority tree_priority = NEW_CONTENT_TAKES_PRIORITY;
  size_t occluded_count = 0u;
  PrioritizedTile last_tile;
  std::unique_ptr<EvictionTilePriorityQueue> queue(
      host_impl()->BuildEvictionQueue(tree_priority));
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    if (!last_tile.tile())
      last_tile = prioritized_tile;

    bool tile_is_occluded = prioritized_tile.is_occluded();

    // The only way we will encounter an occluded tile after an unoccluded
    // tile is if the priorty bin decreased, the tile is required for
    // activation, or the scale changed.
    if (tile_is_occluded) {
      occluded_count++;

      bool last_tile_is_occluded = last_tile.is_occluded();
      if (!last_tile_is_occluded) {
        TilePriority::PriorityBin tile_priority_bin =
            prioritized_tile.priority().priority_bin;
        TilePriority::PriorityBin last_tile_priority_bin =
            last_tile.priority().priority_bin;

        EXPECT_TRUE((tile_priority_bin < last_tile_priority_bin) ||
                    prioritized_tile.tile()->required_for_activation() ||
                    (prioritized_tile.tile()->raster_transform() !=
                     last_tile.tile()->raster_transform()));
      }
    }
    last_tile = prioritized_tile;
    queue->Pop();
  }
  size_t expected_occluded_count = pending_child_high_res_tiles.size();
  EXPECT_EQ(expected_occluded_count, occluded_count);
}

TEST_F(TileManagerTilePriorityQueueTest,
       EvictionTilePriorityQueueWithTransparentLayer) {
  host_impl()->AdvanceToNextFrame(base::TimeDelta::FromMilliseconds(1));

  gfx::Size layer_bounds(1000, 1000);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  SetupPendingTree(pending_raster_source);

  std::unique_ptr<FakePictureLayerImpl> pending_child =
      FakePictureLayerImpl::CreateWithRasterSource(host_impl()->pending_tree(),
                                                   2, pending_raster_source);
  FakePictureLayerImpl* pending_child_layer = pending_child.get();
  pending_layer()->test_properties()->AddChild(std::move(pending_child));

  // Create a fully transparent child layer so that its tile priorities are not
  // considered to be valid.
  pending_child_layer->SetDrawsContent(true);
  pending_child_layer->test_properties()->force_render_surface = true;

  host_impl()->AdvanceToNextFrame(base::TimeDelta::FromMilliseconds(1));
  host_impl()->pending_tree()->property_trees()->needs_rebuild = true;
  host_impl()->pending_tree()->BuildLayerListAndPropertyTreesForTesting();
  host_impl()->pending_tree()->UpdateDrawProperties();

  host_impl()->pending_tree()->SetOpacityMutated(
      pending_child_layer->element_id(), 0.0f);

  host_impl()->AdvanceToNextFrame(base::TimeDelta::FromMilliseconds(1));
  host_impl()->pending_tree()->UpdateDrawProperties();

  // Renew all of the tile priorities.
  gfx::Rect viewport(layer_bounds);
  pending_layer()->picture_layer_tiling_set()->UpdateTilePriorities(
      viewport, 1.0f, 1.0, Occlusion(), true);
  pending_child_layer->picture_layer_tiling_set()->UpdateTilePriorities(
      viewport, 1.0f, 1.0, Occlusion(), true);

  // Populate all tiles directly from the tilings.
  std::set<Tile*> all_pending_tiles;
  std::vector<Tile*> pending_high_res_tiles =
      pending_layer()->HighResTiling()->AllTilesForTesting();
  all_pending_tiles.insert(pending_high_res_tiles.begin(),
                           pending_high_res_tiles.end());
  EXPECT_EQ(16u, pending_high_res_tiles.size());

  std::set<Tile*> all_pending_child_tiles;
  std::vector<Tile*> pending_child_high_res_tiles =
      pending_child_layer->HighResTiling()->AllTilesForTesting();
  all_pending_child_tiles.insert(pending_child_high_res_tiles.begin(),
                                 pending_child_high_res_tiles.end());
  EXPECT_EQ(16u, pending_child_high_res_tiles.size());

  std::set<Tile*> all_tiles = all_pending_tiles;
  all_tiles.insert(all_pending_child_tiles.begin(),
                   all_pending_child_tiles.end());

  tile_manager()->InitializeTilesWithResourcesForTesting(
      std::vector<Tile*>(all_tiles.begin(), all_tiles.end()));

  EXPECT_TRUE(pending_layer()->HasValidTilePriorities());
  EXPECT_FALSE(pending_child_layer->HasValidTilePriorities());

  // Verify that eviction queue returns tiles also from layers without valid
  // tile priorities and that the tile priority bin of those tiles is (at most)
  // EVENTUALLY.
  TreePriority tree_priority = NEW_CONTENT_TAKES_PRIORITY;
  std::set<Tile*> new_content_tiles;
  size_t tile_count = 0;
  std::unique_ptr<EvictionTilePriorityQueue> queue(
      host_impl()->BuildEvictionQueue(tree_priority));
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    Tile* tile = prioritized_tile.tile();
    const TilePriority& pending_priority = prioritized_tile.priority();
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
  host_impl()->SetViewportSize(layer_bounds);
  SetupDefaultTrees(layer_bounds);

  std::unique_ptr<RasterTilePriorityQueue> queue(host_impl()->BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES, RasterTilePriorityQueue::Type::ALL));
  EXPECT_FALSE(queue->IsEmpty());

  size_t tile_count = 0;
  std::set<Tile*> all_tiles;
  while (!queue->IsEmpty()) {
    EXPECT_TRUE(queue->Top().tile());
    all_tiles.insert(queue->Top().tile());
    ++tile_count;
    queue->Pop();
  }

  EXPECT_EQ(tile_count, all_tiles.size());
  EXPECT_EQ(16u, tile_count);

  for (int i = 1; i < 10; ++i) {
    std::unique_ptr<FakePictureLayerImpl> pending_child_layer =
        FakePictureLayerImpl::Create(host_impl()->pending_tree(),
                                     layer_id() + i);
    pending_child_layer->SetDrawsContent(true);
    pending_child_layer->set_has_valid_tile_priorities(true);
    pending_layer()->test_properties()->AddChild(
        std::move(pending_child_layer));
  }

  queue = host_impl()->BuildRasterQueue(SAME_PRIORITY_FOR_BOTH_TREES,
                                        RasterTilePriorityQueue::Type::ALL);
  EXPECT_FALSE(queue->IsEmpty());

  tile_count = 0;
  all_tiles.clear();
  while (!queue->IsEmpty()) {
    EXPECT_TRUE(queue->Top().tile());
    all_tiles.insert(queue->Top().tile());
    ++tile_count;
    queue->Pop();
  }
  EXPECT_EQ(tile_count, all_tiles.size());
  EXPECT_EQ(16u, tile_count);
}

TEST_F(TileManagerTilePriorityQueueTest, EvictionTilePriorityQueueEmptyLayers) {
  const gfx::Size layer_bounds(1000, 1000);
  host_impl()->SetViewportSize(layer_bounds);
  SetupDefaultTrees(layer_bounds);

  std::unique_ptr<RasterTilePriorityQueue> raster_queue(
      host_impl()->BuildRasterQueue(SAME_PRIORITY_FOR_BOTH_TREES,
                                    RasterTilePriorityQueue::Type::ALL));
  EXPECT_FALSE(raster_queue->IsEmpty());

  size_t tile_count = 0;
  std::set<Tile*> all_tiles;
  while (!raster_queue->IsEmpty()) {
    EXPECT_TRUE(raster_queue->Top().tile());
    all_tiles.insert(raster_queue->Top().tile());
    ++tile_count;
    raster_queue->Pop();
  }
  EXPECT_EQ(tile_count, all_tiles.size());
  EXPECT_EQ(16u, tile_count);

  std::vector<Tile*> tiles(all_tiles.begin(), all_tiles.end());
  host_impl()->tile_manager()->InitializeTilesWithResourcesForTesting(tiles);

  for (int i = 1; i < 10; ++i) {
    std::unique_ptr<FakePictureLayerImpl> pending_child_layer =
        FakePictureLayerImpl::Create(host_impl()->pending_tree(),
                                     layer_id() + i);
    pending_child_layer->SetDrawsContent(true);
    pending_child_layer->set_has_valid_tile_priorities(true);
    pending_layer()->test_properties()->AddChild(
        std::move(pending_child_layer));
  }

  std::unique_ptr<EvictionTilePriorityQueue> queue(
      host_impl()->BuildEvictionQueue(SAME_PRIORITY_FOR_BOTH_TREES));
  EXPECT_FALSE(queue->IsEmpty());

  tile_count = 0;
  all_tiles.clear();
  while (!queue->IsEmpty()) {
    EXPECT_TRUE(queue->Top().tile());
    all_tiles.insert(queue->Top().tile());
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

  const int soon_border_outset = 312;
  gfx::Rect soon_rect = viewport;
  soon_rect.Inset(-soon_border_outset, -soon_border_outset);

  client.SetTileSize(gfx::Size(30, 30));
  LayerTreeSettings settings;

  std::unique_ptr<PictureLayerTilingSet> tiling_set =
      PictureLayerTilingSet::Create(
          ACTIVE_TREE, &client, settings.tiling_interest_area_padding,
          settings.skewport_target_time_in_seconds,
          settings.skewport_extrapolation_limit_in_screen_pixels,
          settings.max_preraster_distance_in_screen_pixels);

  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  PictureLayerTiling* tiling =
      tiling_set->AddTiling(gfx::AxisTransform2d(), raster_source);
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
    std::unique_ptr<TilingSetRasterQueueAll> queue(
        new TilingSetRasterQueueAll(tiling_set.get(), false, false));

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
    unique_tiles.insert(queue->Top().tile());
    PrioritizedTile last_tile = queue->Top();
    have_tiles[last_tile.priority().priority_bin] = true;

    // On the second iteration, mark everything as ready to draw (solid color).
    if (i == 1) {
      TileDrawInfo& draw_info = last_tile.tile()->draw_info();
      draw_info.SetSolidColorForTesting(SK_ColorRED);
    }
    queue->Pop();
    int eventually_bin_order_correct_count = 0;
    int eventually_bin_order_incorrect_count = 0;
    while (!queue->IsEmpty()) {
      PrioritizedTile new_tile = queue->Top();
      queue->Pop();
      unique_tiles.insert(new_tile.tile());

      TilePriority last_priority = last_tile.priority();
      TilePriority new_priority = new_tile.priority();
      EXPECT_LE(last_priority.priority_bin, new_priority.priority_bin);
      if (last_priority.priority_bin == new_priority.priority_bin) {
        if (last_priority.priority_bin == TilePriority::EVENTUALLY) {
          bool order_correct = last_priority.distance_to_visible <=
                               new_priority.distance_to_visible;
          eventually_bin_order_correct_count += order_correct;
          eventually_bin_order_incorrect_count += !order_correct;
        } else if (!soon_rect.Intersects(new_tile.tile()->content_rect()) &&
                   !soon_rect.Intersects(last_tile.tile()->content_rect())) {
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
        TileDrawInfo& draw_info = last_tile.tile()->draw_info();
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
  LayerTreeSettings settings;

  std::unique_ptr<PictureLayerTilingSet> tiling_set =
      PictureLayerTilingSet::Create(
          ACTIVE_TREE, &client, settings.tiling_interest_area_padding,
          settings.skewport_target_time_in_seconds,
          settings.skewport_extrapolation_limit_in_screen_pixels,
          settings.max_preraster_distance_in_screen_pixels);

  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  PictureLayerTiling* tiling =
      tiling_set->AddTiling(gfx::AxisTransform2d(), raster_source);
  tiling->set_resolution(HIGH_RESOLUTION);

  tiling_set->UpdateTilePriorities(viewport, 1.0f, 1.0, Occlusion(), true);
  tiling_set->UpdateTilePriorities(moved_viewport, 1.0f, 2.0, Occlusion(),
                                   true);

  const int soon_border_outset = 312;
  gfx::Rect soon_rect = moved_viewport;
  soon_rect.Inset(-soon_border_outset, -soon_border_outset);

  // There are 3 bins in TilePriority.
  bool have_tiles[3] = {};
  PrioritizedTile last_tile;
  int eventually_bin_order_correct_count = 0;
  int eventually_bin_order_incorrect_count = 0;
  std::unique_ptr<TilingSetRasterQueueAll> queue(
      new TilingSetRasterQueueAll(tiling_set.get(), false, false));
  for (; !queue->IsEmpty(); queue->Pop()) {
    if (!last_tile.tile())
      last_tile = queue->Top();

    const PrioritizedTile& new_tile = queue->Top();

    TilePriority last_priority = last_tile.priority();
    TilePriority new_priority = new_tile.priority();

    have_tiles[new_priority.priority_bin] = true;

    EXPECT_LE(last_priority.priority_bin, new_priority.priority_bin);
    if (last_priority.priority_bin == new_priority.priority_bin) {
      if (last_priority.priority_bin == TilePriority::EVENTUALLY) {
        bool order_correct = last_priority.distance_to_visible <=
                             new_priority.distance_to_visible;
        eventually_bin_order_correct_count += order_correct;
        eventually_bin_order_incorrect_count += !order_correct;
      } else if (!soon_rect.Intersects(new_tile.tile()->content_rect()) &&
                 !soon_rect.Intersects(last_tile.tile()->content_rect())) {
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

TEST_F(TileManagerTilePriorityQueueTest, SetIsLikelyToRequireADraw) {
  const gfx::Size layer_bounds(1000, 1000);
  host_impl()->SetViewportSize(layer_bounds);
  SetupDefaultTrees(layer_bounds);

  // Verify that the queue has a required for draw tile at Top.
  std::unique_ptr<RasterTilePriorityQueue> queue(host_impl()->BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES, RasterTilePriorityQueue::Type::ALL));
  EXPECT_FALSE(queue->IsEmpty());
  EXPECT_TRUE(queue->Top().tile()->required_for_draw());

  EXPECT_FALSE(host_impl()->is_likely_to_require_a_draw());
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  EXPECT_TRUE(host_impl()->is_likely_to_require_a_draw());
}

TEST_F(TileManagerTilePriorityQueueTest,
       SetIsLikelyToRequireADrawOnZeroMemoryBudget) {
  const gfx::Size layer_bounds(1000, 1000);
  host_impl()->SetViewportSize(layer_bounds);
  SetupDefaultTrees(layer_bounds);

  // Verify that the queue has a required for draw tile at Top.
  std::unique_ptr<RasterTilePriorityQueue> queue(host_impl()->BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES, RasterTilePriorityQueue::Type::ALL));
  EXPECT_FALSE(queue->IsEmpty());
  EXPECT_TRUE(queue->Top().tile()->required_for_draw());

  ManagedMemoryPolicy policy = host_impl()->ActualManagedMemoryPolicy();
  policy.bytes_limit_when_visible = 0;
  host_impl()->SetMemoryPolicy(policy);

  EXPECT_FALSE(host_impl()->is_likely_to_require_a_draw());
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  EXPECT_FALSE(host_impl()->is_likely_to_require_a_draw());
}

TEST_F(TileManagerTilePriorityQueueTest,
       SetIsLikelyToRequireADrawOnLimitedMemoryBudget) {
  const gfx::Size layer_bounds(1000, 1000);
  host_impl()->SetViewportSize(layer_bounds);
  SetupDefaultTrees(layer_bounds);

  // Verify that the queue has a required for draw tile at Top.
  std::unique_ptr<RasterTilePriorityQueue> queue(host_impl()->BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES, RasterTilePriorityQueue::Type::ALL));
  EXPECT_FALSE(queue->IsEmpty());
  EXPECT_TRUE(queue->Top().tile()->required_for_draw());
  EXPECT_EQ(gfx::Size(256, 256), queue->Top().tile()->desired_texture_size());
  EXPECT_EQ(viz::RGBA_8888,
            host_impl()->resource_provider()->best_texture_format());

  ManagedMemoryPolicy policy = host_impl()->ActualManagedMemoryPolicy();
  policy.bytes_limit_when_visible = ResourceUtil::UncheckedSizeInBytes<size_t>(
      gfx::Size(256, 256), viz::RGBA_8888);
  host_impl()->SetMemoryPolicy(policy);

  EXPECT_FALSE(host_impl()->is_likely_to_require_a_draw());
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  EXPECT_TRUE(host_impl()->is_likely_to_require_a_draw());

  Resource* resource = host_impl()->resource_pool()->AcquireResource(
      gfx::Size(256, 256), viz::RGBA_8888, gfx::ColorSpace());

  host_impl()->tile_manager()->CheckIfMoreTilesNeedToBePreparedForTesting();
  EXPECT_FALSE(host_impl()->is_likely_to_require_a_draw());

  host_impl()->resource_pool()->ReleaseResource(resource);
}

TEST_F(TileManagerTilePriorityQueueTest, DefaultMemoryPolicy) {
  const gfx::Size layer_bounds(1000, 1000);
  host_impl()->SetViewportSize(layer_bounds);
  SetupDefaultTrees(layer_bounds);

  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());

  // 64MB is the default mem limit.
  EXPECT_EQ(67108864u,
            host_impl()->global_tile_state().hard_memory_limit_in_bytes);
  EXPECT_EQ(TileMemoryLimitPolicy::ALLOW_ANYTHING,
            host_impl()->global_tile_state().memory_limit_policy);
  EXPECT_EQ(ManagedMemoryPolicy::kDefaultNumResourcesLimit,
            host_impl()->global_tile_state().num_resources_limit);
}

TEST_F(TileManagerTilePriorityQueueTest, RasterQueueAllUsesCorrectTileBounds) {
  // Verify that we use the real tile bounds when advancing phases during the
  // tile iteration.
  gfx::Size layer_bounds(1, 1);

  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  FakePictureLayerTilingClient pending_client;
  pending_client.SetTileSize(gfx::Size(64, 64));

  std::unique_ptr<PictureLayerTilingSet> tiling_set =
      PictureLayerTilingSet::Create(WhichTree::ACTIVE_TREE, &pending_client,
                                    1.0f, 1.0f, 1000, 1000.f);
  pending_client.set_twin_tiling_set(tiling_set.get());

  auto* tiling = tiling_set->AddTiling(gfx::AxisTransform2d(), raster_source);

  tiling->set_resolution(HIGH_RESOLUTION);
  tiling->CreateAllTilesForTesting();

  // The tile is (0, 0, 1, 1), create an intersecting and non-intersecting
  // rectangle to test the advance phase with. The tile size is (64, 64), so
  // both rectangles intersect the tile content size, but only one should
  // intersect the actual size.
  gfx::Rect non_intersecting_rect(2, 2, 10, 10);
  gfx::Rect intersecting_rect(0, 0, 10, 10);
  {
    tiling->SetTilePriorityRectsForTesting(
        non_intersecting_rect,  // Visible rect.
        intersecting_rect,      // Skewport rect.
        intersecting_rect,      // Soon rect.
        intersecting_rect);     // Eventually rect.
    std::unique_ptr<TilingSetRasterQueueAll> queue(
        new TilingSetRasterQueueAll(tiling_set.get(), false, false));
    EXPECT_FALSE(queue->IsEmpty());
  }
  {
    tiling->SetTilePriorityRectsForTesting(
        non_intersecting_rect,  // Visible rect.
        non_intersecting_rect,  // Skewport rect.
        intersecting_rect,      // Soon rect.
        intersecting_rect);     // Eventually rect.
    std::unique_ptr<TilingSetRasterQueueAll> queue(
        new TilingSetRasterQueueAll(tiling_set.get(), false, false));
    EXPECT_FALSE(queue->IsEmpty());
  }
  {
    tiling->SetTilePriorityRectsForTesting(
        non_intersecting_rect,  // Visible rect.
        non_intersecting_rect,  // Skewport rect.
        non_intersecting_rect,  // Soon rect.
        intersecting_rect);     // Eventually rect.
    std::unique_ptr<TilingSetRasterQueueAll> queue(
        new TilingSetRasterQueueAll(tiling_set.get(), false, false));
    EXPECT_FALSE(queue->IsEmpty());
  }
}

TEST_F(TileManagerTilePriorityQueueTest, NoRasterTasksforSolidColorTiles) {
  gfx::Size size(10, 10);
  const gfx::Size layer_bounds(1000, 1000);

  std::unique_ptr<FakeRecordingSource> recording_source =
      FakeRecordingSource::CreateFilledRecordingSource(layer_bounds);

  PaintFlags solid_flags;
  SkColor solid_color = SkColorSetARGB(255, 12, 23, 34);
  solid_flags.setColor(solid_color);
  recording_source->add_draw_rect_with_flags(gfx::Rect(layer_bounds),
                                             solid_flags);

  // Create non solid tile as well, otherwise tilings wouldnt be created.
  SkColor non_solid_color = SkColorSetARGB(128, 45, 56, 67);
  PaintFlags non_solid_flags;
  non_solid_flags.setColor(non_solid_color);

  recording_source->add_draw_rect_with_flags(gfx::Rect(0, 0, 10, 10),
                                             non_solid_flags);
  recording_source->Rerecord();

  scoped_refptr<RasterSource> raster_source =
      recording_source->CreateRasterSource();

  FakePictureLayerTilingClient tiling_client;
  tiling_client.SetTileSize(size);

  std::unique_ptr<PictureLayerImpl> layer_impl = PictureLayerImpl::Create(
      host_impl()->active_tree(), 1, Layer::LayerMaskType::NOT_MASK);
  layer_impl->set_contributes_to_drawn_render_surface(true);
  PictureLayerTilingSet* tiling_set = layer_impl->picture_layer_tiling_set();

  PictureLayerTiling* tiling =
      tiling_set->AddTiling(gfx::AxisTransform2d(), raster_source);
  tiling->set_resolution(HIGH_RESOLUTION);
  tiling->CreateAllTilesForTesting();
  tiling->SetTilePriorityRectsForTesting(
      gfx::Rect(layer_bounds),   // Visible rect.
      gfx::Rect(layer_bounds),   // Skewport rect.
      gfx::Rect(layer_bounds),   // Soon rect.
      gfx::Rect(layer_bounds));  // Eventually rect.

  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());

  std::vector<Tile*> tiles = tiling->AllTilesForTesting();
  for (size_t tile_idx = 0; tile_idx < tiles.size(); ++tile_idx) {
    Tile* tile = tiles[tile_idx];
    if (tile->id() == 1) {
      // Non-solid tile.
      EXPECT_TRUE(tile->HasRasterTask());
      EXPECT_EQ(TileDrawInfo::RESOURCE_MODE, tile->draw_info().mode());
    } else {
      EXPECT_FALSE(tile->HasRasterTask());
      EXPECT_EQ(TileDrawInfo::SOLID_COLOR_MODE, tile->draw_info().mode());
      EXPECT_EQ(solid_color, tile->draw_info().solid_color());
    }
  }
}

class TileManagerTest : public TestLayerTreeHostBase {
 public:
  // MockLayerTreeHostImpl allows us to intercept tile manager callbacks.
  class MockLayerTreeHostImpl : public FakeLayerTreeHostImpl {
   public:
    MockLayerTreeHostImpl(const LayerTreeSettings& settings,
                          TaskRunnerProvider* task_runner_provider,
                          TaskGraphRunner* task_graph_runner)
        : FakeLayerTreeHostImpl(settings,
                                task_runner_provider,
                                task_graph_runner) {}

    MOCK_METHOD0(NotifyReadyToActivate, void());
    MOCK_METHOD0(NotifyReadyToDraw, void());
    MOCK_METHOD0(NotifyAllTileTasksCompleted, void());
  };

  std::unique_ptr<FakeLayerTreeHostImpl> CreateHostImpl(
      const LayerTreeSettings& settings,
      TaskRunnerProvider* task_runner_provider,
      TaskGraphRunner* task_graph_runner) override {
    return std::make_unique<testing::NiceMock<MockLayerTreeHostImpl>>(
        settings, task_runner_provider, task_graph_runner);
  }

  // By default use software compositing (no context provider).
  std::unique_ptr<LayerTreeFrameSink> CreateLayerTreeFrameSink() override {
    return FakeLayerTreeFrameSink::CreateSoftware();
  }

  MockLayerTreeHostImpl& MockHostImpl() {
    return *static_cast<MockLayerTreeHostImpl*>(host_impl());
  }
};

// Test to ensure that we call NotifyAllTileTasksCompleted when PrepareTiles is
// called.
TEST_F(TileManagerTest, AllWorkFinished) {
  // Check with no tile work enqueued.
  {
    base::RunLoop run_loop;
    EXPECT_FALSE(
        host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
    EXPECT_CALL(MockHostImpl(), NotifyReadyToActivate());
    EXPECT_CALL(MockHostImpl(), NotifyReadyToDraw());
    EXPECT_CALL(MockHostImpl(), NotifyAllTileTasksCompleted())
        .WillOnce(testing::Invoke([&run_loop]() { run_loop.Quit(); }));
    host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
    EXPECT_TRUE(host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
    run_loop.Run();
  }

  // Check that the "schedule more work" path also triggers the expected
  // callback.
  {
    base::RunLoop run_loop;
    EXPECT_FALSE(
        host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
    EXPECT_CALL(MockHostImpl(), NotifyReadyToActivate());
    EXPECT_CALL(MockHostImpl(), NotifyReadyToDraw());
    EXPECT_CALL(MockHostImpl(), NotifyAllTileTasksCompleted())
        .WillOnce(testing::Invoke([&run_loop]() { run_loop.Quit(); }));
    host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
    host_impl()->tile_manager()->SetMoreTilesNeedToBeRasterizedForTesting();
    EXPECT_TRUE(host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
    run_loop.Run();
  }

  // Check that if callbacks are called by CheckIfMoreTilesNeedToBePrepared if
  // they haven't been called already.
  {
    base::RunLoop run_loop;
    EXPECT_FALSE(
        host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
    EXPECT_CALL(MockHostImpl(), NotifyReadyToActivate());
    EXPECT_CALL(MockHostImpl(), NotifyReadyToDraw());
    EXPECT_CALL(MockHostImpl(), NotifyAllTileTasksCompleted())
        .WillOnce(testing::Invoke([&run_loop]() { run_loop.Quit(); }));
    host_impl()->tile_manager()->ResetSignalsForTesting();
    host_impl()->tile_manager()->SetMoreTilesNeedToBeRasterizedForTesting();
    host_impl()->tile_manager()->CheckIfMoreTilesNeedToBePreparedForTesting();
    run_loop.Run();
  }

  // Same test as above but with SMOOTHNESS_TAKES_PRIORITY.
  {
    base::RunLoop run_loop;
    EXPECT_FALSE(
        host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
    EXPECT_CALL(MockHostImpl(), NotifyReadyToActivate());
    EXPECT_CALL(MockHostImpl(), NotifyReadyToDraw());
    EXPECT_CALL(MockHostImpl(), NotifyAllTileTasksCompleted())
        .WillOnce(testing::Invoke([&run_loop]() { run_loop.Quit(); }));
    host_impl()->tile_manager()->ResetSignalsForTesting();
    auto global_state = host_impl()->global_tile_state();
    global_state.tree_priority = SMOOTHNESS_TAKES_PRIORITY;
    host_impl()->tile_manager()->SetGlobalStateForTesting(global_state);
    host_impl()->tile_manager()->SetMoreTilesNeedToBeRasterizedForTesting();
    host_impl()->tile_manager()->CheckIfMoreTilesNeedToBePreparedForTesting();
    run_loop.Run();
  }
}

TEST_F(TileManagerTest, ActivateAndDrawWhenOOM) {
  SetupDefaultTrees(gfx::Size(1000, 1000));

  auto global_state = host_impl()->global_tile_state();
  global_state.hard_memory_limit_in_bytes = 1u;
  global_state.soft_memory_limit_in_bytes = 1u;

  {
    base::RunLoop run_loop;
    EXPECT_FALSE(
        host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
    EXPECT_CALL(MockHostImpl(), NotifyReadyToActivate());
    EXPECT_CALL(MockHostImpl(), NotifyReadyToDraw());
    EXPECT_CALL(MockHostImpl(), NotifyAllTileTasksCompleted())
        .WillOnce(testing::Invoke([&run_loop]() { run_loop.Quit(); }));
    host_impl()->tile_manager()->PrepareTiles(global_state);
    EXPECT_TRUE(host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
    run_loop.Run();
  }

  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToDraw());
  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToActivate());
  EXPECT_TRUE(host_impl()->notify_tile_state_changed_called());

  // Next PrepareTiles should skip NotifyTileStateChanged since all tiles
  // are marked oom already.
  {
    base::RunLoop run_loop;
    host_impl()->set_notify_tile_state_changed_called(false);
    EXPECT_CALL(MockHostImpl(), NotifyReadyToActivate());
    EXPECT_CALL(MockHostImpl(), NotifyReadyToDraw());
    EXPECT_CALL(MockHostImpl(), NotifyAllTileTasksCompleted())
        .WillOnce(testing::Invoke([&run_loop]() { run_loop.Quit(); }));
    host_impl()->tile_manager()->PrepareTiles(global_state);
    run_loop.Run();
    EXPECT_FALSE(host_impl()->notify_tile_state_changed_called());
  }
}

TEST_F(TileManagerTest, LowResHasNoImage) {
  gfx::Size size(10, 12);
  TileResolution resolutions[] = {HIGH_RESOLUTION, LOW_RESOLUTION};

  for (size_t i = 0; i < arraysize(resolutions); ++i) {
    SCOPED_TRACE(resolutions[i]);

    // Make a RasterSource that will draw a blue bitmap image.
    sk_sp<SkSurface> surface =
        SkSurface::MakeRasterN32Premul(size.width(), size.height());
    ASSERT_NE(surface, nullptr);
    surface->getCanvas()->clear(SK_ColorBLUE);
    sk_sp<SkImage> blue_image = surface->makeImageSnapshot();

    std::unique_ptr<FakeRecordingSource> recording_source =
        FakeRecordingSource::CreateFilledRecordingSource(size);
    recording_source->SetBackgroundColor(SK_ColorTRANSPARENT);
    recording_source->SetRequiresClear(true);
    recording_source->SetClearCanvasWithDebugColor(false);
    PaintFlags flags;
    flags.setColor(SK_ColorGREEN);
    recording_source->add_draw_rect_with_flags(gfx::Rect(size), flags);
    recording_source->add_draw_image(std::move(blue_image), gfx::Point());
    recording_source->Rerecord();
    scoped_refptr<RasterSource> raster = recording_source->CreateRasterSource();

    FakePictureLayerTilingClient tiling_client;
    tiling_client.SetTileSize(size);

    std::unique_ptr<PictureLayerImpl> layer = PictureLayerImpl::Create(
        host_impl()->active_tree(), 1, Layer::LayerMaskType::NOT_MASK);
    PictureLayerTilingSet* tiling_set = layer->picture_layer_tiling_set();
    layer->set_contributes_to_drawn_render_surface(true);

    auto* tiling = tiling_set->AddTiling(gfx::AxisTransform2d(), raster);
    tiling->set_resolution(resolutions[i]);
    tiling->CreateAllTilesForTesting();
    tiling->SetTilePriorityRectsForTesting(
        gfx::Rect(size),   // Visible rect.
        gfx::Rect(size),   // Skewport rect.
        gfx::Rect(size),   // Soon rect.
        gfx::Rect(size));  // Eventually rect.

    // SMOOTHNESS_TAKES_PRIORITY ensures that we will actually raster
    // LOW_RESOLUTION tiles, otherwise they are skipped.
    host_impl()->SetTreePriority(SMOOTHNESS_TAKES_PRIORITY);

    // Call PrepareTiles and wait for it to complete.
    auto* tile_manager = host_impl()->tile_manager();
    base::RunLoop run_loop;
    EXPECT_CALL(MockHostImpl(), NotifyAllTileTasksCompleted())
        .WillOnce(testing::Invoke([&run_loop]() { run_loop.Quit(); }));
    tile_manager->PrepareTiles(host_impl()->global_tile_state());
    run_loop.Run();
    tile_manager->CheckForCompletedTasks();

    Tile* tile = tiling->TileAt(0, 0);
    // The tile in the tiling was rastered.
    EXPECT_EQ(TileDrawInfo::RESOURCE_MODE, tile->draw_info().mode());
    EXPECT_TRUE(tile->draw_info().IsReadyToDraw());

    ResourceProvider::ScopedWriteLockSoftware lock(
        host_impl()->resource_provider(), tile->draw_info().resource_id());
    const SkBitmap bitmap = lock.sk_bitmap();
    for (int x = 0; x < size.width(); ++x) {
      for (int y = 0; y < size.height(); ++y) {
        SCOPED_TRACE(y);
        SCOPED_TRACE(x);
        if (resolutions[i] == LOW_RESOLUTION) {
          // Since it's low res, the bitmap was not drawn, and the background
          // (green) is visible instead.
          ASSERT_EQ(SK_ColorGREEN, bitmap.getColor(x, y));
        } else {
          EXPECT_EQ(HIGH_RESOLUTION, resolutions[i]);
          // Since it's high res, the bitmap (blue) was drawn, and the
          // background is not visible.
          ASSERT_EQ(SK_ColorBLUE, bitmap.getColor(x, y));
        }
      }
    }
  }
}

class ActivationTasksDoNotBlockReadyToDrawTest : public TileManagerTest {
 protected:
  std::unique_ptr<TaskGraphRunner> CreateTaskGraphRunner() override {
    return std::make_unique<SynchronousTaskGraphRunner>();
  }

  std::unique_ptr<LayerTreeFrameSink> CreateLayerTreeFrameSink() override {
    return FakeLayerTreeFrameSink::Create3d();
  }

  LayerTreeSettings CreateSettings() override {
    LayerTreeSettings settings = TileManagerTest::CreateSettings();
    settings.gpu_rasterization_forced = true;
    return settings;
  }
};

TEST_F(ActivationTasksDoNotBlockReadyToDrawTest,
       ActivationTasksDoNotBlockReadyToDraw) {
  const gfx::Size layer_bounds(1000, 1000);

  EXPECT_TRUE(host_impl()->use_gpu_rasterization());

  // Active tree has no non-solid tiles, so it will generate no tile tasks.
  std::unique_ptr<FakeRecordingSource> active_tree_recording_source =
      FakeRecordingSource::CreateFilledRecordingSource(layer_bounds);

  PaintFlags solid_flags;
  SkColor solid_color = SkColorSetARGB(255, 12, 23, 34);
  solid_flags.setColor(solid_color);
  active_tree_recording_source->add_draw_rect_with_flags(
      gfx::Rect(layer_bounds), solid_flags);

  active_tree_recording_source->Rerecord();

  // Pending tree has non-solid tiles, so it will generate tile tasks.
  std::unique_ptr<FakeRecordingSource> pending_tree_recording_source =
      FakeRecordingSource::CreateFilledRecordingSource(layer_bounds);
  SkColor non_solid_color = SkColorSetARGB(128, 45, 56, 67);
  PaintFlags non_solid_flags;
  non_solid_flags.setColor(non_solid_color);

  pending_tree_recording_source->add_draw_rect_with_flags(
      gfx::Rect(5, 5, 10, 10), non_solid_flags);
  pending_tree_recording_source->Rerecord();

  scoped_refptr<RasterSource> active_tree_raster_source =
      active_tree_recording_source->CreateRasterSource();
  scoped_refptr<RasterSource> pending_tree_raster_source =
      pending_tree_recording_source->CreateRasterSource();

  SetupTrees(pending_tree_raster_source, active_tree_raster_source);
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());

  // The first task to run should be ReadyToDraw (this should not be blocked by
  // the tasks required for activation).
  base::RunLoop run_loop;
  EXPECT_CALL(MockHostImpl(), NotifyReadyToDraw())
      .WillOnce(testing::Invoke([&run_loop]() { run_loop.Quit(); }));
  static_cast<SynchronousTaskGraphRunner*>(task_graph_runner())
      ->RunSingleTaskForTesting();
  run_loop.Run();
}

class PartialRasterTileManagerTest : public TileManagerTest {
 public:
  LayerTreeSettings CreateSettings() override {
    LayerTreeSettings settings = TileManagerTest::CreateSettings();
    settings.use_partial_raster = true;
    return settings;
  }
};

// Ensures that if a raster task is cancelled, it gets returned to the resource
// pool with an invalid content ID, not with its invalidated content ID.
TEST_F(PartialRasterTileManagerTest, CancelledTasksHaveNoContentId) {
  // Create a FakeTileTaskManagerImpl and set it on the tile manager so that all
  // scheduled work is immediately cancelled.

  host_impl()->tile_manager()->SetTileTaskManagerForTesting(
      std::make_unique<FakeTileTaskManagerImpl>());

  // Pick arbitrary IDs - they don't really matter as long as they're constant.
  const int kLayerId = 7;
  const uint64_t kInvalidatedId = 43;
  const gfx::Size kTileSize(128, 128);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(kTileSize);
  host_impl()->CreatePendingTree();
  LayerTreeImpl* pending_tree = host_impl()->pending_tree();

  // Steal from the recycled tree.
  std::unique_ptr<FakePictureLayerImpl> pending_layer =
      FakePictureLayerImpl::CreateWithRasterSource(pending_tree, kLayerId,
                                                   pending_raster_source);
  pending_layer->SetDrawsContent(true);

  // The bounds() just mirror the raster source size.
  pending_layer->SetBounds(pending_layer->raster_source()->GetSize());
  pending_tree->SetRootLayerForTesting(std::move(pending_layer));

  // Add tilings/tiles for the layer.
  host_impl()->pending_tree()->BuildLayerListAndPropertyTreesForTesting();
  host_impl()->pending_tree()->UpdateDrawProperties();

  // Build the raster queue and invalidate the top tile.
  std::unique_ptr<RasterTilePriorityQueue> queue(host_impl()->BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES, RasterTilePriorityQueue::Type::ALL));
  EXPECT_FALSE(queue->IsEmpty());
  queue->Top().tile()->SetInvalidated(gfx::Rect(), kInvalidatedId);

  // PrepareTiles to schedule tasks. Due to the FakeTileTaskManagerImpl,
  // these tasks will immediately be canceled.
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());

  // Make sure that the tile we invalidated above was not returned to the pool
  // with its invalidated resource ID.
  host_impl()->resource_pool()->CheckBusyResources();
  gfx::Rect total_invalidated_rect;
  EXPECT_FALSE(host_impl()->resource_pool()->TryAcquireResourceForPartialRaster(
      kInvalidatedId + 1, gfx::Rect(), kInvalidatedId,
      &total_invalidated_rect));
  EXPECT_EQ(gfx::Rect(), total_invalidated_rect);

  // Free our host_impl_ before the tile_task_manager we passed it, as it
  // will use that class in clean up.
  TakeHostImpl();
}

// FakeRasterBufferProviderImpl that verifies the resource content ID of raster
// tasks.
class VerifyResourceContentIdRasterBufferProvider
    : public FakeRasterBufferProviderImpl {
 public:
  explicit VerifyResourceContentIdRasterBufferProvider(
      uint64_t expected_resource_id)
      : expected_resource_id_(expected_resource_id) {}
  ~VerifyResourceContentIdRasterBufferProvider() override {}

  // RasterBufferProvider methods.
  std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const Resource* resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id) override {
    EXPECT_EQ(expected_resource_id_, resource_content_id);
    return nullptr;
  }

 private:
  uint64_t expected_resource_id_;
};

// Runs a test to ensure that partial raster is either enabled or disabled,
// depending on |partial_raster_enabled|'s value. Takes ownership of host_impl
// so that cleanup order can be controlled.
void RunPartialRasterCheck(std::unique_ptr<LayerTreeHostImpl> host_impl,
                           bool partial_raster_enabled) {
  // Pick arbitrary IDs - they don't really matter as long as they're constant.
  const int kLayerId = 7;
  const uint64_t kInvalidatedId = 43;
  const uint64_t kExpectedId = partial_raster_enabled ? kInvalidatedId : 0u;
  const gfx::Size kTileSize(128, 128);

  // Create a VerifyResourceContentIdTileTaskManager to ensure that the
  // raster task we see is created with |kExpectedId|.
  host_impl->tile_manager()->SetTileTaskManagerForTesting(
      std::make_unique<FakeTileTaskManagerImpl>());

  VerifyResourceContentIdRasterBufferProvider raster_buffer_provider(
      kExpectedId);
  host_impl->tile_manager()->SetRasterBufferProviderForTesting(
      &raster_buffer_provider);

  // Ensure there's a resource with our |kInvalidatedId| in the resource pool.
  auto* resource = host_impl->resource_pool()->AcquireResource(
      kTileSize, viz::RGBA_8888, gfx::ColorSpace());
  host_impl->resource_pool()->OnContentReplaced(resource->id(), kInvalidatedId);
  host_impl->resource_pool()->ReleaseResource(resource);
  host_impl->resource_pool()->CheckBusyResources();

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(kTileSize);
  host_impl->CreatePendingTree();
  LayerTreeImpl* pending_tree = host_impl->pending_tree();

  // Steal from the recycled tree.
  std::unique_ptr<FakePictureLayerImpl> pending_layer =
      FakePictureLayerImpl::CreateWithRasterSource(pending_tree, kLayerId,
                                                   pending_raster_source);
  pending_layer->SetDrawsContent(true);

  // The bounds() just mirror the raster source size.
  pending_layer->SetBounds(pending_layer->raster_source()->GetSize());
  pending_tree->SetRootLayerForTesting(std::move(pending_layer));

  // Add tilings/tiles for the layer.
  host_impl->pending_tree()->BuildLayerListAndPropertyTreesForTesting();
  host_impl->pending_tree()->UpdateDrawProperties();

  // Build the raster queue and invalidate the top tile.
  std::unique_ptr<RasterTilePriorityQueue> queue(host_impl->BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES, RasterTilePriorityQueue::Type::ALL));
  EXPECT_FALSE(queue->IsEmpty());
  queue->Top().tile()->SetInvalidated(gfx::Rect(), kInvalidatedId);

  // PrepareTiles to schedule tasks. Due to the
  // VerifyPreviousContentRasterBufferProvider, these tasks will verified and
  // cancelled.
  host_impl->tile_manager()->PrepareTiles(host_impl->global_tile_state());

  // Free our host_impl before the verifying_task_manager we passed it, as it
  // will use that class in clean up.
  host_impl = nullptr;
}

// Ensures that the tile manager successfully reuses tiles when partial
// raster is enabled.
TEST_F(PartialRasterTileManagerTest, PartialRasterSuccessfullyEnabled) {
  RunPartialRasterCheck(TakeHostImpl(), true /* partial_raster_enabled */);
}

// Ensures that the tile manager does not attempt to reuse tiles when partial
// raster is disabled.
TEST_F(TileManagerTest, PartialRasterSuccessfullyDisabled) {
  RunPartialRasterCheck(TakeHostImpl(), false /* partial_raster_enabled */);
}

// FakeRasterBufferProviderImpl that allows us to mock ready to draw
// functionality.
class MockReadyToDrawRasterBufferProviderImpl
    : public FakeRasterBufferProviderImpl {
 public:
  MOCK_CONST_METHOD1(IsResourceReadyToDraw, bool(viz::ResourceId resource_id));
  MOCK_CONST_METHOD3(
      SetReadyToDrawCallback,
      uint64_t(const ResourceProvider::ResourceIdArray& resource_ids,
               const base::Closure& callback,
               uint64_t pending_callback_id));

  std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const Resource* resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id) override {
    return std::make_unique<FakeRasterBuffer>();
  }

 private:
  class FakeRasterBuffer : public RasterBuffer {
   public:
    void Playback(
        const RasterSource* raster_source,
        const gfx::Rect& raster_full_rect,
        const gfx::Rect& raster_dirty_rect,
        uint64_t new_content_id,
        const gfx::AxisTransform2d& transform,
        const RasterSource::PlaybackSettings& playback_settings) override {}
  };
};

class TileManagerReadyToDrawTest : public TileManagerTest {
 public:
  ~TileManagerReadyToDrawTest() override {
    // Ensure that the host impl doesn't outlive |raster_buffer_provider_|.
    TakeHostImpl();
  }

  void SetUp() override {
    TileManagerTest::SetUp();
    host_impl()->tile_manager()->SetRasterBufferProviderForTesting(
        &mock_raster_buffer_provider_);

    const gfx::Size layer_bounds(1000, 1000);

    solid_color_recording_source_ =
        FakeRecordingSource::CreateFilledRecordingSource(layer_bounds);

    PaintFlags solid_flags;
    SkColor solid_color = SkColorSetARGB(255, 12, 23, 34);
    solid_flags.setColor(solid_color);
    solid_color_recording_source_->add_draw_rect_with_flags(
        gfx::Rect(layer_bounds), solid_flags);

    solid_color_recording_source_->Rerecord();

    recording_source_ =
        FakeRecordingSource::CreateFilledRecordingSource(layer_bounds);
    SkColor non_solid_color = SkColorSetARGB(128, 45, 56, 67);
    PaintFlags non_solid_flags;
    non_solid_flags.setColor(non_solid_color);

    for (int i = 0; i < 100; ++i) {
      for (int j = 0; j < 100; ++j) {
        recording_source_->add_draw_rect_with_flags(
            gfx::Rect(10 * i, 10 * j, 5, 5), non_solid_flags);
      }
    }
    recording_source_->Rerecord();
  }

  LayerTreeSettings CreateSettings() override {
    LayerTreeSettings settings;
    settings.resource_settings.buffer_to_texture_target_map =
        viz::DefaultBufferToTextureTargetMapForTesting();
    return settings;
  }

  void SetupTreesWithActiveTreeTiles() {
    scoped_refptr<RasterSource> active_tree_raster_source =
        recording_source_->CreateRasterSource();
    scoped_refptr<RasterSource> pending_tree_raster_source =
        solid_color_recording_source_->CreateRasterSource();

    SetupTrees(pending_tree_raster_source, active_tree_raster_source);
  }

  void SetupTreesWithPendingTreeTiles() {
    scoped_refptr<RasterSource> active_tree_raster_source =
        solid_color_recording_source_->CreateRasterSource();
    scoped_refptr<RasterSource> pending_tree_raster_source =
        recording_source_->CreateRasterSource();

    SetupTrees(pending_tree_raster_source, active_tree_raster_source);
  }

  TileManager* tile_manager() { return host_impl()->tile_manager(); }
  MockReadyToDrawRasterBufferProviderImpl* mock_raster_buffer_provider() {
    return &mock_raster_buffer_provider_;
  }

 private:
  StrictMock<MockReadyToDrawRasterBufferProviderImpl>
      mock_raster_buffer_provider_;
  std::unique_ptr<FakeRecordingSource> recording_source_;
  std::unique_ptr<FakeRecordingSource> solid_color_recording_source_;
};

TEST_F(TileManagerReadyToDrawTest, SmoothActivationWaitsOnCallback) {
  host_impl()->SetTreePriority(SMOOTHNESS_TAKES_PRIORITY);
  SetupTreesWithPendingTreeTiles();

  base::Closure callback;
  {
    base::RunLoop run_loop;

    // Until we activate our ready to draw callback, treat all resources as not
    // ready to draw.
    EXPECT_CALL(*mock_raster_buffer_provider(),
                IsResourceReadyToDraw(testing::_))
        .WillRepeatedly(Return(false));

    EXPECT_CALL(*mock_raster_buffer_provider(), SetReadyToDrawCallback(_, _, 0))
        .WillOnce(testing::Invoke([&run_loop, &callback](
            const ResourceProvider::ResourceIdArray& resource_ids,
            const base::Closure& callback_in, uint64_t pending_callback_id) {
          callback = callback_in;
          run_loop.Quit();
          return 1;
        }));
    host_impl()->tile_manager()->DidModifyTilePriorities();
    host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
    run_loop.Run();
  }

  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToDraw());
  EXPECT_FALSE(host_impl()->tile_manager()->IsReadyToActivate());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(MockHostImpl(), NotifyReadyToActivate())
        .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
    EXPECT_CALL(*mock_raster_buffer_provider(),
                IsResourceReadyToDraw(testing::_))
        .WillRepeatedly(Return(true));
    callback.Run();
    run_loop.Run();
  }

  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToDraw());
  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToActivate());
}

TEST_F(TileManagerReadyToDrawTest, NonSmoothActivationDoesNotWaitOnCallback) {
  SetupTreesWithPendingTreeTiles();

  // We're using a StrictMock on the RasterBufferProvider, so any function call
  // will cause a test failure.
  base::RunLoop run_loop;

  host_impl()->tile_manager()->DidModifyTilePriorities();
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  EXPECT_CALL(MockHostImpl(), NotifyReadyToActivate())
      .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
  run_loop.Run();

  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToDraw());
  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToActivate());
}

TEST_F(TileManagerReadyToDrawTest, SmoothDrawWaitsOnCallback) {
  host_impl()->SetTreePriority(SMOOTHNESS_TAKES_PRIORITY);
  SetupTreesWithActiveTreeTiles();

  base::Closure callback;
  {
    base::RunLoop run_loop;

    // Until we activate our ready to draw callback, treat all resources as not
    // ready to draw.
    EXPECT_CALL(*mock_raster_buffer_provider(),
                IsResourceReadyToDraw(testing::_))
        .WillRepeatedly(Return(false));

    EXPECT_CALL(*mock_raster_buffer_provider(), SetReadyToDrawCallback(_, _, 0))
        .WillOnce(Invoke([&run_loop, &callback](
            const ResourceProvider::ResourceIdArray& resource_ids,
            const base::Closure& callback_in, uint64_t pending_callback_id) {
          callback = callback_in;
          run_loop.Quit();
          return 1;
        }));
    host_impl()->tile_manager()->DidModifyTilePriorities();
    host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
    run_loop.Run();
  }

  EXPECT_FALSE(host_impl()->tile_manager()->IsReadyToDraw());
  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToActivate());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(MockHostImpl(), NotifyReadyToDraw())
        .WillOnce(testing::Invoke([&run_loop]() { run_loop.Quit(); }));
    EXPECT_CALL(*mock_raster_buffer_provider(),
                IsResourceReadyToDraw(testing::_))
        .WillRepeatedly(Return(true));
    callback.Run();
    run_loop.Run();
  }

  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToDraw());
  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToActivate());
}

TEST_F(TileManagerReadyToDrawTest, NonSmoothDrawDoesNotWaitOnCallback) {
  SetupTreesWithActiveTreeTiles();

  // We're using a StrictMock on the RasterBufferProvider, so any function call
  // will cause a test failure.
  base::RunLoop run_loop;

  host_impl()->tile_manager()->DidModifyTilePriorities();
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  EXPECT_CALL(MockHostImpl(), NotifyReadyToDraw())
      .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
  run_loop.Run();

  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToDraw());
  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToActivate());
}

TEST_F(TileManagerReadyToDrawTest, NoCallbackWhenAlreadyReadyToDraw) {
  host_impl()->SetTreePriority(SMOOTHNESS_TAKES_PRIORITY);
  SetupTreesWithPendingTreeTiles();

  base::RunLoop run_loop;
  host_impl()->tile_manager()->DidModifyTilePriorities();
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  EXPECT_CALL(MockHostImpl(), NotifyReadyToActivate())
      .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
  EXPECT_CALL(*mock_raster_buffer_provider(), IsResourceReadyToDraw(_))
      .WillRepeatedly(Return(true));
  run_loop.Run();

  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToDraw());
  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToActivate());
}

void UpdateVisibleRect(FakePictureLayerImpl* layer,
                       const gfx::Rect visible_rect) {
  PictureLayerTilingSet* tiling_set = layer->tilings();
  for (size_t j = 0; j < tiling_set->num_tilings(); ++j) {
    PictureLayerTiling* tiling = tiling_set->tiling_at(j);
    tiling->SetTilePriorityRectsForTesting(
        visible_rect,                  // Visible rect.
        visible_rect,                  // Skewport rect.
        visible_rect,                  // Soon rect.
        gfx::Rect(0, 0, 1000, 1000));  // Eventually rect.
  }
}

TEST_F(TileManagerReadyToDrawTest, ReadyToDrawRespectsRequirementChange) {
  host_impl()->SetTreePriority(SMOOTHNESS_TAKES_PRIORITY);
  SetupTreesWithPendingTreeTiles();

  // Initially create a tiling with a visible rect of (0, 0, 100, 100) and
  // a soon rect of the rest of the layer.
  UpdateVisibleRect(pending_layer(), gfx::Rect(0, 0, 100, 100));

  // Mark all these tiles as ready to draw.
  base::RunLoop run_loop;
  host_impl()->tile_manager()->DidModifyTilePriorities();
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  EXPECT_CALL(MockHostImpl(), NotifyReadyToActivate())
      .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
  EXPECT_CALL(*mock_raster_buffer_provider(), IsResourceReadyToDraw(_))
      .WillRepeatedly(Return(true));
  run_loop.Run();

  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToDraw());
  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToActivate());

  // Move the viewport to (900, 900, 100, 100), so that we need a different set
  // of tilings.
  UpdateVisibleRect(pending_layer(), gfx::Rect(900, 900, 100, 100));

  EXPECT_CALL(*mock_raster_buffer_provider(), IsResourceReadyToDraw(testing::_))
      .WillRepeatedly(Return(false));

  base::Closure callback;
  {
    base::RunLoop run_loop;

    EXPECT_CALL(*mock_raster_buffer_provider(), SetReadyToDrawCallback(_, _, 0))
        .WillOnce(testing::Invoke([&run_loop, &callback](
            const ResourceProvider::ResourceIdArray& resource_ids,
            const base::Closure& callback_in, uint64_t pending_callback_id) {
          callback = callback_in;
          run_loop.Quit();
          return 1;
        }));
    host_impl()->tile_manager()->DidModifyTilePriorities();
    host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
    run_loop.Run();
  }

  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToDraw());
  EXPECT_FALSE(host_impl()->tile_manager()->IsReadyToActivate());

  // Now switch back to our original tiling. We should be immediately able to
  // activate, as we still have the original tile, and no longer need the
  // tiles from the previous callback.
  UpdateVisibleRect(pending_layer(), gfx::Rect(0, 0, 100, 100));

  {
    base::RunLoop run_loop;
    host_impl()->tile_manager()->DidModifyTilePriorities();
    host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
    EXPECT_CALL(MockHostImpl(), NotifyReadyToActivate())
        .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
    run_loop.Run();
  }

  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToDraw());
  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToActivate());
}

class CheckerImagingTileManagerTest : public TestLayerTreeHostBase {
 public:
  class MockImageGenerator : public StubPaintImageGenerator {
   public:
    explicit MockImageGenerator(const gfx::Size& size)
        : StubPaintImageGenerator(
              SkImageInfo::MakeN32Premul(size.width(), size.height())) {}

    MOCK_METHOD5(GetPixels,
                 bool(const SkImageInfo&, void*, size_t, size_t, uint32_t));
  };

  void TearDown() override {
    // Allow all tasks on the image worker to run now. Any scheduled decodes
    // will be aborted.
    task_runner_->set_run_tasks_synchronously(true);
  }

  LayerTreeSettings CreateSettings() override {
    LayerTreeSettings settings;
    settings.commit_to_active_tree = false;
    settings.enable_checker_imaging = true;
    settings.resource_settings.buffer_to_texture_target_map =
        viz::DefaultBufferToTextureTargetMapForTesting();
    settings.min_image_bytes_to_checker = 512 * 1024;
    return settings;
  }

  std::unique_ptr<FakeLayerTreeHostImpl> CreateHostImpl(
      const LayerTreeSettings& settings,
      TaskRunnerProvider* task_runner_provider,
      TaskGraphRunner* task_graph_runner) override {
    task_runner_ = make_scoped_refptr(new SynchronousSimpleTaskRunner);
    return std::make_unique<FakeLayerTreeHostImpl>(
        settings, task_runner_provider, task_graph_runner, task_runner_);
  }

  std::unique_ptr<TaskGraphRunner> CreateTaskGraphRunner() override {
    return std::make_unique<SynchronousTaskGraphRunner>();
  }

  void FlushDecodeTasks() {
    while (task_runner_->HasPendingTask()) {
      task_runner_->RunUntilIdle();
      base::RunLoop().RunUntilIdle();
    }
  }

  void CleanUpTileManager() {
    task_runner_->set_run_tasks_synchronously(true);
    host_impl()->tile_manager()->FinishTasksAndCleanUp();
    task_runner_->set_run_tasks_synchronously(false);
  }

 private:
  scoped_refptr<SynchronousSimpleTaskRunner> task_runner_;
};

TEST_F(CheckerImagingTileManagerTest,
       NoImageDecodeDependencyForCheckeredTiles) {
  const gfx::Size layer_bounds(512, 512);
  SetupDefaultTrees(layer_bounds);

  std::unique_ptr<FakeRecordingSource> recording_source =
      FakeRecordingSource::CreateFilledRecordingSource(layer_bounds);
  recording_source->set_fill_with_nonsolid_color(true);

  auto generator =
      sk_make_sp<testing::StrictMock<MockImageGenerator>>(gfx::Size(512, 512));
  PaintImage image = PaintImageBuilder()
                         .set_id(PaintImage::GetNextId())
                         .set_paint_image_generator(generator)
                         .TakePaintImage();
  recording_source->add_draw_image(image, gfx::Point(0, 0));

  recording_source->Rerecord();
  scoped_refptr<RasterSource> raster_source =
      recording_source->CreateRasterSource();

  std::unique_ptr<PictureLayerImpl> layer_impl = PictureLayerImpl::Create(
      host_impl()->pending_tree(), 1, Layer::LayerMaskType::NOT_MASK);
  layer_impl->set_contributes_to_drawn_render_surface(true);
  PictureLayerTilingSet* tiling_set = layer_impl->picture_layer_tiling_set();

  PictureLayerTiling* tiling =
      tiling_set->AddTiling(gfx::AxisTransform2d(), raster_source);
  tiling->set_resolution(HIGH_RESOLUTION);
  tiling->CreateAllTilesForTesting();
  tiling->SetTilePriorityRectsForTesting(
      gfx::Rect(layer_bounds),   // Visible rect.
      gfx::Rect(layer_bounds),   // Skewport rect.
      gfx::Rect(layer_bounds),   // Soon rect.
      gfx::Rect(layer_bounds));  // Eventually rect.
  tiling->set_can_require_tiles_for_activation(true);

  // PrepareTiles and synchronously run all tasks added to the TaskGraph. Since
  // we are using a strict mock for the SkImageGenerator, if the decode runs as
  // a part of raster tasks, the test should fail.
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  EXPECT_TRUE(host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
  static_cast<SynchronousTaskGraphRunner*>(task_graph_runner())->RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
}

TEST_F(CheckerImagingTileManagerTest, BuildsImageDecodeQueueAsExpected) {
  const gfx::Size layer_bounds(900, 900);

  std::unique_ptr<FakeRecordingSource> recording_source =
      FakeRecordingSource::CreateFilledRecordingSource(layer_bounds);
  recording_source->set_fill_with_nonsolid_color(true);

  int dimension = 450;
  PaintImage image1 =
      CreateDiscardablePaintImage(gfx::Size(dimension, dimension));
  PaintImage image2 =
      CreateDiscardablePaintImage(gfx::Size(dimension, dimension));
  PaintImage image3 =
      CreateDiscardablePaintImage(gfx::Size(dimension, dimension));
  recording_source->add_draw_image(image1, gfx::Point(0, 0));
  recording_source->add_draw_image(image2, gfx::Point(600, 0));
  recording_source->add_draw_image(image3, gfx::Point(0, 600));

  recording_source->Rerecord();
  scoped_refptr<RasterSource> raster_source =
      recording_source->CreateRasterSource();

  gfx::Size tile_size(500, 500);
  Region invalidation((gfx::Rect(layer_bounds)));
  SetupPendingTree(raster_source, tile_size, invalidation);

  PictureLayerTilingSet* tiling_set =
      pending_layer()->picture_layer_tiling_set();
  PictureLayerTiling* pending_tiling = tiling_set->tiling_at(0);
  pending_tiling->set_resolution(HIGH_RESOLUTION);
  pending_tiling->CreateAllTilesForTesting();
  pending_tiling->SetTilePriorityRectsForTesting(
      gfx::Rect(layer_bounds),   // Visible rect.
      gfx::Rect(layer_bounds),   // Skewport rect.
      gfx::Rect(layer_bounds),   // Soon rect.
      gfx::Rect(layer_bounds));  // Eventually rect.

  // PrepareTiles and make sure we account correctly for tiles that have been
  // scheduled with checkered images.
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  EXPECT_TRUE(host_impl()->tile_manager()->HasScheduledTileTasksForTesting());

  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 2; j++) {
      const Tile* tile = pending_tiling->TileAt(i, j);
      EXPECT_TRUE(tile->HasRasterTask());
      if (i == 1 && j == 1)
        EXPECT_FALSE(tile->raster_task_scheduled_with_checker_images());
      else
        EXPECT_TRUE(tile->raster_task_scheduled_with_checker_images());
    }
  }
  EXPECT_EQ(host_impl()->tile_manager()->num_of_tiles_with_checker_images(), 3);

  // Now raster all the tiles and make sure these tiles are still accounted for
  // with checkered images.
  static_cast<SynchronousTaskGraphRunner*>(task_graph_runner())->RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 2; j++) {
      const Tile* tile = pending_tiling->TileAt(i, j);
      EXPECT_FALSE(tile->HasRasterTask());
      EXPECT_FALSE(tile->raster_task_scheduled_with_checker_images());
      EXPECT_TRUE(tile->draw_info().has_resource());
      if (i == 1 && j == 1)
        EXPECT_FALSE(tile->draw_info().is_checker_imaged());
      else
        EXPECT_TRUE(tile->draw_info().is_checker_imaged());
    }
  }
  EXPECT_EQ(host_impl()->tile_manager()->num_of_tiles_with_checker_images(), 3);

  // Activate the pending tree.
  ActivateTree();

  // Set empty tile priority rects so an empty image decode queue is used.
  gfx::Rect empty_rect;
  PictureLayerTiling* active_tiling =
      active_layer()->picture_layer_tiling_set()->tiling_at(0);
  active_tiling->SetTilePriorityRectsForTesting(
      gfx::Rect(empty_rect),   // Visible rect.
      gfx::Rect(empty_rect),   // Skewport rect.
      gfx::Rect(empty_rect),   // Soon rect.
      gfx::Rect(empty_rect));  // Eventually rect.
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());

  // Run the decode tasks. Since the first decode is always scheduled, the
  // completion for it should be triggered.
  FlushDecodeTasks();

  // Create a new pending tree to invalidate tiles for decoded images and verify
  // that only tiles for |image1| are invalidated.
  EXPECT_TRUE(host_impl()->client()->did_request_impl_side_invalidation());
  PerformImplSideInvalidation();
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 2; j++) {
      const Tile* tile = pending_tiling->TileAt(i, j);
      if (i == 0 && j == 0)
        EXPECT_TRUE(tile);
      else
        EXPECT_FALSE(tile);
    }
  }
  host_impl()->client()->reset_did_request_impl_side_invalidation();

  // Activating the tree replaces the checker-imaged tile.
  EXPECT_EQ(host_impl()->tile_manager()->num_of_tiles_with_checker_images(), 3);
  ActivateTree();
  EXPECT_EQ(host_impl()->tile_manager()->num_of_tiles_with_checker_images(), 2);

  // Set the tile priority rects such that only the tile with the second image
  // is scheduled for decodes, since it is checker-imaged.
  gfx::Rect rect_to_raster(600, 0, 300, 900);
  active_tiling->SetTilePriorityRectsForTesting(
      gfx::Rect(rect_to_raster),   // Visible rect.
      gfx::Rect(rect_to_raster),   // Skewport rect.
      gfx::Rect(rect_to_raster),   // Soon rect.
      gfx::Rect(rect_to_raster));  // Eventually rect.
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());

  // Finish all raster and dispatch completion callback so that the decode work
  // for checkered images can be scheduled.
  static_cast<SynchronousTaskGraphRunner*>(task_graph_runner())->RunUntilIdle();
  base::RunLoop().RunUntilIdle();

  // Run decode tasks to trigger completion of any pending decodes.
  FlushDecodeTasks();

  // Create a new pending tree to invalidate tiles for decoded images and verify
  // that only tiles for |image2| are invalidated.
  EXPECT_TRUE(host_impl()->client()->did_request_impl_side_invalidation());
  PerformImplSideInvalidation();
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 2; j++) {
      const Tile* tile = pending_tiling->TileAt(i, j);
      if (i == 1 && j == 0)
        EXPECT_TRUE(tile);
      else
        EXPECT_FALSE(tile);
    }
  }
  host_impl()->client()->reset_did_request_impl_side_invalidation();

  // Activating the tree replaces the checker-imaged tile.
  EXPECT_EQ(host_impl()->tile_manager()->num_of_tiles_with_checker_images(), 2);
  ActivateTree();
  EXPECT_EQ(host_impl()->tile_manager()->num_of_tiles_with_checker_images(), 1);

  // Set the tile priority rects to cover the complete tiling and change the
  // visibility. While |image3| has not yet been decoded, since we are
  // invisible no decodes should have been scheduled.
  active_tiling->SetTilePriorityRectsForTesting(
      gfx::Rect(layer_bounds),   // Visible rect.
      gfx::Rect(layer_bounds),   // Skewport rect.
      gfx::Rect(layer_bounds),   // Soon rect.
      gfx::Rect(layer_bounds));  // Eventually rect.
  host_impl()->SetVisible(false);
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  FlushDecodeTasks();
  EXPECT_FALSE(host_impl()->client()->did_request_impl_side_invalidation());
}

TEST_F(CheckerImagingTileManagerTest,
       TileManagerCleanupClearsCheckerImagedDecodes) {
  const gfx::Size layer_bounds(512, 512);

  std::unique_ptr<FakeRecordingSource> recording_source =
      FakeRecordingSource::CreateFilledRecordingSource(layer_bounds);
  recording_source->set_fill_with_nonsolid_color(true);
  PaintImage image = CreateDiscardablePaintImage(gfx::Size(512, 512));
  recording_source->add_draw_image(image, gfx::Point(0, 0));
  recording_source->Rerecord();
  scoped_refptr<RasterSource> raster_source =
      recording_source->CreateRasterSource();

  SetupPendingTree(raster_source, gfx::Size(100, 100),
                   Region(gfx::Rect(0, 0, 500, 500)));

  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  // Finish all raster and dispatch completion callback so that the decode work
  // for checkered images can be scheduled.
  static_cast<SynchronousTaskGraphRunner*>(task_graph_runner())->RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  FlushDecodeTasks();

  EXPECT_TRUE(host_impl()
                  ->tile_manager()
                  ->checker_image_tracker()
                  .has_locked_decodes_for_testing());

  host_impl()->pending_tree()->ReleaseTileResources();
  CleanUpTileManager();

  EXPECT_FALSE(host_impl()
                   ->tile_manager()
                   ->checker_image_tracker()
                   .has_locked_decodes_for_testing());
  EXPECT_TRUE(
      host_impl()->tile_manager()->TakeImagesToInvalidateOnSyncTree().empty());
}

TEST_F(CheckerImagingTileManagerTest,
       TileManagerCorrectlyPrioritizesCheckerImagedDecodes) {
  gfx::Size layer_bounds(500, 500);

  std::unique_ptr<FakeRecordingSource> recording_source =
      FakeRecordingSource::CreateFilledRecordingSource(layer_bounds);
  recording_source->set_fill_with_nonsolid_color(true);
  PaintImage image = CreateDiscardablePaintImage(gfx::Size(512, 512));
  recording_source->add_draw_image(image, gfx::Point(0, 0));
  recording_source->Rerecord();
  scoped_refptr<RasterSource> raster_source =
      recording_source->CreateRasterSource();

  // Required for activation tiles block checker-imaged decodes.
  SetupPendingTree(raster_source, gfx::Size(100, 100),
                   Region(gfx::Rect(0, 0, 500, 500)));
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  EXPECT_TRUE(host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
  EXPECT_TRUE(host_impl()
                  ->tile_manager()
                  ->checker_image_tracker()
                  .no_decodes_allowed_for_testing());
  while (!host_impl()->client()->ready_to_activate()) {
    static_cast<SynchronousTaskGraphRunner*>(task_graph_runner())
        ->RunSingleTaskForTesting();
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_EQ(host_impl()
                ->tile_manager()
                ->checker_image_tracker()
                .decode_priority_allowed_for_testing(),
            CheckerImageTracker::DecodeType::kRaster);

  // Finishing all tasks allows pre-decodes.
  static_cast<SynchronousTaskGraphRunner*>(task_graph_runner())->RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(host_impl()
                ->tile_manager()
                ->checker_image_tracker()
                .decode_priority_allowed_for_testing(),
            CheckerImageTracker::DecodeType::kPreDecode);

  // Required for draw tiles block checker-imaged decodes.
  // Free all tile resources and perform another PrepareTiles.
  ActivateTree();
  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToDraw());
  host_impl()->tile_manager()->PrepareTiles(
      GlobalStateThatImpactsTilePriority());
  EXPECT_FALSE(host_impl()->tile_manager()->IsReadyToDraw());

  host_impl()->client()->reset_ready_to_draw();
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  EXPECT_TRUE(host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
  EXPECT_TRUE(host_impl()
                  ->tile_manager()
                  ->checker_image_tracker()
                  .no_decodes_allowed_for_testing());
  while (!host_impl()->client()->ready_to_draw()) {
    static_cast<SynchronousTaskGraphRunner*>(task_graph_runner())
        ->RunSingleTaskForTesting();
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_EQ(host_impl()
                ->tile_manager()
                ->checker_image_tracker()
                .decode_priority_allowed_for_testing(),
            CheckerImageTracker::DecodeType::kRaster);
}

class CheckerImagingTileManagerMemoryTest
    : public CheckerImagingTileManagerTest {
 public:
  std::unique_ptr<FakeLayerTreeHostImpl> CreateHostImpl(
      const LayerTreeSettings& settings,
      TaskRunnerProvider* task_runner_provider,
      TaskGraphRunner* task_graph_runner) override {
    LayerTreeSettings new_settings = settings;
    new_settings.gpu_memory_policy.num_resources_limit = 4;
    return CheckerImagingTileManagerTest::CreateHostImpl(
        new_settings, task_runner_provider, task_graph_runner);
  }
};

TEST_F(CheckerImagingTileManagerMemoryTest, AddsAllNowTilesToImageDecodeQueue) {
  const gfx::Size layer_bounds(900, 1400);

  std::unique_ptr<FakeRecordingSource> recording_source =
      FakeRecordingSource::CreateFilledRecordingSource(layer_bounds);
  recording_source->set_fill_with_nonsolid_color(true);

  int dimension = 450;
  PaintImage image1 =
      CreateDiscardablePaintImage(gfx::Size(dimension, dimension));
  PaintImage image2 =
      CreateDiscardablePaintImage(gfx::Size(dimension, dimension));
  recording_source->add_draw_image(image1, gfx::Point(0, 515));
  recording_source->add_draw_image(image2, gfx::Point(515, 515));

  recording_source->Rerecord();
  scoped_refptr<RasterSource> raster_source =
      recording_source->CreateRasterSource();

  gfx::Size tile_size(500, 500);
  Region invalidation((gfx::Rect(layer_bounds)));
  SetupPendingTree(raster_source, tile_size, invalidation);

  PictureLayerTilingSet* tiling_set =
      pending_layer()->picture_layer_tiling_set();
  PictureLayerTiling* pending_tiling = tiling_set->tiling_at(0);
  pending_tiling->set_resolution(HIGH_RESOLUTION);
  pending_tiling->CreateAllTilesForTesting();

  // Use a rect that only rasterizes the bottom 2 rows of tiles.
  gfx::Rect rect_to_raster(0, 500, 900, 900);
  pending_tiling->SetTilePriorityRectsForTesting(
      rect_to_raster,   // Visible rect.
      rect_to_raster,   // Skewport rect.
      rect_to_raster,   // Soon rect.
      rect_to_raster);  // Eventually rect.

  // PrepareTiles, rasterize all scheduled tiles and activate while no images
  // have been decoded.
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  static_cast<SynchronousTaskGraphRunner*>(task_graph_runner())->RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  ActivateTree();

  // Expand the visible rect to include the complete tiling. The tile iteration
  // will not go beyond the first tile since there are no resources with a lower
  // priority that can be evicted. But we should still see image decodes
  // scheduled for all visible tiles.
  gfx::Rect complete_tiling_rect(layer_bounds);
  PictureLayerTiling* active_tiling =
      active_layer()->picture_layer_tiling_set()->tiling_at(0);
  active_tiling->SetTilePriorityRectsForTesting(
      complete_tiling_rect,   // Visible rect.
      complete_tiling_rect,   // Skewport rect.
      complete_tiling_rect,   // Soon rect.
      complete_tiling_rect);  // Eventually rect.
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());

  // Finish all raster work so the decode work for checkered images can be
  // scheduled.
  static_cast<SynchronousTaskGraphRunner*>(task_graph_runner())->RunUntilIdle();
  base::RunLoop().RunUntilIdle();

  // Flush all decode tasks. The tiles with checkered images should be
  // invalidated.
  FlushDecodeTasks();
  EXPECT_TRUE(host_impl()->client()->did_request_impl_side_invalidation());
  PerformImplSideInvalidation();
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 3; j++) {
      const Tile* tile = pending_tiling->TileAt(i, j);
      if (j == 1)
        EXPECT_TRUE(tile);
      else
        EXPECT_FALSE(tile);
    }
  }
  host_impl()->client()->reset_did_request_impl_side_invalidation();
}

}  // namespace
}  // namespace cc
