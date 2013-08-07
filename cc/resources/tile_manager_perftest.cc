// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "cc/resources/tile.h"
#include "cc/resources/tile_priority.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_picture_pile_impl.h"
#include "cc/test/fake_tile_manager.h"
#include "cc/test/fake_tile_manager_client.h"
#include "cc/test/test_tile_priorities.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class TileManagerPerfTest : public testing::Test {
 public:
  typedef std::vector<scoped_refptr<Tile> > TileVector;

  TileManagerPerfTest() : num_runs_(0) {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    output_surface_ = FakeOutputSurface::Create3d();
    resource_provider_ = ResourceProvider::Create(output_surface_.get(), 0);
    tile_manager_ = make_scoped_ptr(
        new FakeTileManager(&tile_manager_client_, resource_provider_.get()));

    GlobalStateThatImpactsTilePriority state;
    gfx::Size tile_size = settings_.default_tile_size;
    state.memory_limit_in_bytes =
        10000 * 4 * tile_size.width() * tile_size.height();
    state.memory_limit_policy = ALLOW_ANYTHING;
    state.tree_priority = SMOOTHNESS_TAKES_PRIORITY;

    tile_manager_->SetGlobalState(state);
    picture_pile_ = FakePicturePileImpl::CreatePile();
  }

  virtual void TearDown() OVERRIDE {
    tile_manager_.reset(NULL);
    picture_pile_ = NULL;
  }

  void EndTest() {
    elapsed_ = base::TimeTicks::HighResNow() - start_time_;
  }

  void AfterTest(const std::string test_name) {
    // Format matches chrome/test/perf/perf_test.h:PrintResult
    printf("*RESULT %s: %.2f runs/s\n",
           test_name.c_str(),
           num_runs_ / elapsed_.InSecondsF());
  }

  bool DidRun() {
    ++num_runs_;
    if (num_runs_ == kWarmupRuns)
      start_time_ = base::TimeTicks::HighResNow();

    if (!start_time_.is_null() && (num_runs_ % kTimeCheckInterval) == 0) {
      base::TimeDelta elapsed = base::TimeTicks::HighResNow() - start_time_;
      if (elapsed >= base::TimeDelta::FromMilliseconds(kTimeLimitMillis)) {
        elapsed_ = elapsed;
        return false;
      }
    }

    return true;
  }

  void CreateBinTiles(int count, TilePriority priority, TileVector* tiles) {
    for (int i = 0; i < count; ++i) {
      scoped_refptr<Tile> tile =
          make_scoped_refptr(new Tile(tile_manager_.get(),
                                      picture_pile_.get(),
                                      settings_.default_tile_size,
                                      gfx::Rect(),
                                      gfx::Rect(),
                                      1.0,
                                      0,
                                      0,
                                      true));
      tile->SetPriority(ACTIVE_TREE, priority);
      tile->SetPriority(PENDING_TREE, priority);
      tiles->push_back(tile);
    }
  }

  void CreateTiles(int count, TileVector* tiles) {
    // Roughly an equal amount of all bins.
    int count_per_bin = count / NUM_BINS;
    CreateBinTiles(count_per_bin, TilePriorityForNowBin(), tiles);
    CreateBinTiles(count_per_bin, TilePriorityForSoonBin(), tiles);
    CreateBinTiles(count_per_bin, TilePriorityForEventualBin(), tiles);
    CreateBinTiles(count - 3 * count_per_bin, TilePriority(), tiles);
  }

  void RunManageTilesTest(const std::string test_name,
                          unsigned tile_count) {
    start_time_ = base::TimeTicks();
    num_runs_ = 0;
    TileVector tiles;
    CreateTiles(tile_count, &tiles);
    do {
      tile_manager_->ManageTiles();
    } while (DidRun());

    AfterTest(test_name);
  }

 private:
  FakeTileManagerClient tile_manager_client_;
  LayerTreeSettings settings_;
  scoped_ptr<FakeTileManager> tile_manager_;
  scoped_refptr<FakePicturePileImpl> picture_pile_;
  scoped_ptr<FakeOutputSurface> output_surface_;
  scoped_ptr<ResourceProvider> resource_provider_;

  base::TimeTicks start_time_;
  base::TimeDelta elapsed_;
  int num_runs_;
};

TEST_F(TileManagerPerfTest, ManageTiles) {
  RunManageTilesTest("manage_tiles_100", 100);
  RunManageTilesTest("manage_tiles_1000", 1000);
  RunManageTilesTest("manage_tiles_10000", 10000);
}

}  // namespace

}  // namespace cc
