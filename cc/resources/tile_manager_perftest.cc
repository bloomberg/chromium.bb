// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "cc/resources/tile.h"
#include "cc/resources/tile_priority.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_picture_pile_impl.h"
#include "cc/test/fake_tile_manager.h"
#include "cc/test/fake_tile_manager_client.h"
#include "cc/test/impl_side_painting_settings.h"
#include "cc/test/lap_timer.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_tile_priorities.h"
#include "cc/trees/layer_tree_impl.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace cc {

namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class TileManagerPerfTest : public testing::Test {
 public:
  typedef std::vector<std::pair<scoped_refptr<Tile>, ManagedTileBin> >
      TileBinVector;

  TileManagerPerfTest()
      : timer_(kWarmupRuns,
               base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
               kTimeCheckInterval) {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    output_surface_ = FakeOutputSurface::Create3d();
    CHECK(output_surface_->BindToClient(&output_surface_client_));

    shared_bitmap_manager_.reset(new TestSharedBitmapManager());
    resource_provider_ = ResourceProvider::Create(
        output_surface_.get(), shared_bitmap_manager_.get(), 0, false, 1);
    resource_pool_ = ResourcePool::Create(
        resource_provider_.get(), GL_TEXTURE_2D, RGBA_8888);
    size_t raster_task_limit_bytes = 32 * 1024 * 1024;  // 16-64MB in practice.
    tile_manager_ = make_scoped_ptr(new FakeTileManager(
        &tile_manager_client_, resource_pool_.get(), raster_task_limit_bytes));
    picture_pile_ = FakePicturePileImpl::CreateInfiniteFilledPile();
  }

  GlobalStateThatImpactsTilePriority GlobalStateForTest() {
    GlobalStateThatImpactsTilePriority state;
    gfx::Size tile_size = settings_.default_tile_size;
    state.soft_memory_limit_in_bytes =
        10000u * 4u *
        static_cast<size_t>(tile_size.width() * tile_size.height());
    state.hard_memory_limit_in_bytes = state.soft_memory_limit_in_bytes;
    state.num_resources_limit = 10000;
    state.memory_limit_policy = ALLOW_ANYTHING;
    state.tree_priority = SMOOTHNESS_TAKES_PRIORITY;
    return state;
  }

  virtual void TearDown() OVERRIDE {
    tile_manager_.reset(NULL);
    picture_pile_ = NULL;
  }

  TilePriority GetTilePriorityFromBin(ManagedTileBin bin) {
    switch (bin) {
      case NOW_AND_READY_TO_DRAW_BIN:
      case NOW_BIN:
        return TilePriorityForNowBin();
      case SOON_BIN:
        return TilePriorityForSoonBin();
      case EVENTUALLY_AND_ACTIVE_BIN:
      case EVENTUALLY_BIN:
        return TilePriorityForEventualBin();
      case AT_LAST_BIN:
      case AT_LAST_AND_ACTIVE_BIN:
      case NEVER_BIN:
        return TilePriority();
      default:
        NOTREACHED();
        return TilePriority();
    }
  }

  ManagedTileBin GetNextBin(ManagedTileBin bin) {
    switch (bin) {
      case NOW_AND_READY_TO_DRAW_BIN:
      case NOW_BIN:
        return SOON_BIN;
      case SOON_BIN:
        return EVENTUALLY_BIN;
      case EVENTUALLY_AND_ACTIVE_BIN:
      case EVENTUALLY_BIN:
        return NEVER_BIN;
      case AT_LAST_BIN:
      case AT_LAST_AND_ACTIVE_BIN:
      case NEVER_BIN:
        return NOW_BIN;
      default:
        NOTREACHED();
        return NEVER_BIN;
    }
  }

  void CreateBinTiles(int count, ManagedTileBin bin, TileBinVector* tiles) {
    for (int i = 0; i < count; ++i) {
      scoped_refptr<Tile> tile =
          tile_manager_->CreateTile(picture_pile_.get(),
                                    settings_.default_tile_size,
                                    gfx::Rect(),
                                    gfx::Rect(),
                                    1.0,
                                    0,
                                    0,
                                    Tile::USE_LCD_TEXT);
      tile->SetPriority(ACTIVE_TREE, GetTilePriorityFromBin(bin));
      tile->SetPriority(PENDING_TREE, GetTilePriorityFromBin(bin));
      tiles->push_back(std::make_pair(tile, bin));
    }
  }

  void CreateTiles(int count, TileBinVector* tiles) {
    // Roughly an equal amount of all bins.
    int count_per_bin = count / NUM_BINS;
    CreateBinTiles(count_per_bin, NOW_BIN, tiles);
    CreateBinTiles(count_per_bin, SOON_BIN, tiles);
    CreateBinTiles(count_per_bin, EVENTUALLY_BIN, tiles);
    CreateBinTiles(count - 3 * count_per_bin, NEVER_BIN, tiles);
  }

  void RunManageTilesTest(const std::string& test_name,
                          unsigned tile_count,
                          int priority_change_percent) {
    DCHECK_GE(tile_count, 100u);
    DCHECK_GE(priority_change_percent, 0);
    DCHECK_LE(priority_change_percent, 100);
    TileBinVector tiles;
    CreateTiles(tile_count, &tiles);
    timer_.Reset();
    do {
      if (priority_change_percent > 0) {
        for (unsigned i = 0; i < tile_count;
             i += 100 / priority_change_percent) {
          Tile* tile = tiles[i].first.get();
          ManagedTileBin bin = GetNextBin(tiles[i].second);
          tile->SetPriority(ACTIVE_TREE, GetTilePriorityFromBin(bin));
          tile->SetPriority(PENDING_TREE, GetTilePriorityFromBin(bin));
          tiles[i].second = bin;
        }
      }

      tile_manager_->ManageTiles(GlobalStateForTest());
      tile_manager_->UpdateVisibleTiles();
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult(
        "manage_tiles", "", test_name, timer_.LapsPerSecond(), "runs/s", true);
  }

 private:
  FakeTileManagerClient tile_manager_client_;
  LayerTreeSettings settings_;
  scoped_ptr<FakeTileManager> tile_manager_;
  scoped_refptr<FakePicturePileImpl> picture_pile_;
  FakeOutputSurfaceClient output_surface_client_;
  scoped_ptr<FakeOutputSurface> output_surface_;
  scoped_ptr<SharedBitmapManager> shared_bitmap_manager_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<ResourcePool> resource_pool_;
  LapTimer timer_;
};

TEST_F(TileManagerPerfTest, ManageTiles) {
  RunManageTilesTest("100_0", 100, 0);
  RunManageTilesTest("1000_0", 1000, 0);
  RunManageTilesTest("10000_0", 10000, 0);
  RunManageTilesTest("100_10", 100, 10);
  RunManageTilesTest("1000_10", 1000, 10);
  RunManageTilesTest("10000_10", 10000, 10);
  RunManageTilesTest("100_100", 100, 100);
  RunManageTilesTest("1000_100", 1000, 100);
  RunManageTilesTest("10000_100", 10000, 100);
}

class TileManagerTileIteratorPerfTest : public testing::Test,
                                        public TileManagerClient {
 public:
  TileManagerTileIteratorPerfTest()
      : memory_limit_policy_(ALLOW_ANYTHING),
        max_tiles_(10000),
        ready_to_activate_(false),
        id_(7),
        proxy_(base::MessageLoopProxy::current()),
        host_impl_(ImplSidePaintingSettings(),
                   &proxy_,
                   &shared_bitmap_manager_),
        timer_(kWarmupRuns,
               base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
               kTimeCheckInterval) {}

  void SetTreePriority(TreePriority tree_priority) {
    GlobalStateThatImpactsTilePriority state;
    gfx::Size tile_size(256, 256);

    state.soft_memory_limit_in_bytes = 100 * 1000 * 1000;
    state.num_resources_limit = max_tiles_;
    state.hard_memory_limit_in_bytes = state.soft_memory_limit_in_bytes * 2;
    state.unused_memory_limit_in_bytes = state.soft_memory_limit_in_bytes;
    state.memory_limit_policy = memory_limit_policy_;
    state.tree_priority = tree_priority;

    global_state_ = state;
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

  void RunTest(const std::string& test_name, unsigned tile_count) {
    timer_.Reset();
    do {
      for (TileManager::RasterTileIterator it(tile_manager(),
                                              SAME_PRIORITY_FOR_BOTH_TREES);
           it && tile_count;
           ++it) {
        --tile_count;
      }
      ASSERT_EQ(0u, tile_count);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult("tile_manager_raster_tile_iterator",
                           "",
                           test_name,
                           timer_.LapsPerSecond(),
                           "runs/s",
                           true);
  }

  // TileManagerClient implementation.
  virtual void NotifyReadyToActivate() OVERRIDE { ready_to_activate_ = true; }

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
  LapTimer timer_;
};

TEST_F(TileManagerTileIteratorPerfTest, RasterTileIterator) {
  SetupDefaultTrees(gfx::Size(10000, 10000));
  active_layer_->CreateDefaultTilingsAndTiles();
  pending_layer_->CreateDefaultTilingsAndTiles();

  RunTest("2_16", 16);
  RunTest("2_32", 32);
  RunTest("2_64", 64);
  RunTest("2_128", 128);
}

}  // namespace

}  // namespace cc
