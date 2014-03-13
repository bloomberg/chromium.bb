// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "cc/resources/tile.h"
#include "cc/resources/tile_priority.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_picture_pile_impl.h"
#include "cc/test/fake_tile_manager.h"
#include "cc/test/fake_tile_manager_client.h"
#include "cc/test/lap_timer.h"
#include "cc/test/test_tile_priorities.h"

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

    resource_provider_ =
        ResourceProvider::Create(output_surface_.get(), NULL, 0, false, 1);
    size_t raster_task_limit_bytes = 32 * 1024 * 1024;  // 16-64MB in practice.
    tile_manager_ =
        make_scoped_ptr(new FakeTileManager(&tile_manager_client_,
                                            resource_provider_.get(),
                                            raster_task_limit_bytes));
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
  scoped_ptr<ResourceProvider> resource_provider_;
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

}  // namespace

}  // namespace cc
