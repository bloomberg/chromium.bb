// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

class TileManagerTest : public testing::TestWithParam<bool> {
 public:
  typedef std::vector<scoped_refptr<Tile> > TileVector;

  void Initialize(int max_tiles,
                  TileMemoryLimitPolicy memory_limit_policy,
                  TreePriority tree_priority) {
    output_surface_ = FakeOutputSurface::Create3d();
    resource_provider_ = ResourceProvider::Create(output_surface_.get(), 0);
    tile_manager_ = make_scoped_ptr(
        new FakeTileManager(&tile_manager_client_, resource_provider_.get()));

    memory_limit_policy_ = memory_limit_policy;
    max_memory_tiles_ = max_tiles;
    GlobalStateThatImpactsTilePriority state;
    gfx::Size tile_size = settings_.default_tile_size;

    // The parametrization specifies whether the max tile limit should
    // be applied to RAM or to tile limit.
    if (GetParam()) {
      state.memory_limit_in_bytes =
          max_tiles * 4 * tile_size.width() * tile_size.height();
      state.num_resources_limit = 100;
    } else {
      state.memory_limit_in_bytes = 100 * 1000 * 1000;
      state.num_resources_limit = max_tiles;
    }
    state.memory_limit_policy = memory_limit_policy;
    state.tree_priority = tree_priority;

    tile_manager_->SetGlobalState(state);
    picture_pile_ = FakePicturePileImpl::CreatePile();
  }

  void SetTreePriority(TreePriority tree_priority) {
    GlobalStateThatImpactsTilePriority state;
    gfx::Size tile_size = settings_.default_tile_size;
    state.memory_limit_in_bytes =
        max_memory_tiles_ * 4 * tile_size.width() * tile_size.height();
    state.memory_limit_policy = memory_limit_policy_;
    state.num_resources_limit = 100;
    state.tree_priority = tree_priority;
    tile_manager_->SetGlobalState(state);
  }

  virtual void TearDown() OVERRIDE {
    tile_manager_.reset(NULL);
    picture_pile_ = NULL;

    testing::Test::TearDown();
  }

  TileVector CreateTiles(int count,
                         TilePriority active_priority,
                         TilePriority pending_priority) {
    TileVector tiles;
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
      tile->SetPriority(ACTIVE_TREE, active_priority);
      tile->SetPriority(PENDING_TREE, pending_priority);
      tiles.push_back(tile);
    }
    return tiles;
  }

  FakeTileManager* tile_manager() {
    return tile_manager_.get();
  }

  int AssignedMemoryCount(const TileVector& tiles) {
    int has_memory_count = 0;
    for (TileVector::const_iterator it = tiles.begin();
         it != tiles.end();
         ++it) {
      if (tile_manager_->HasBeenAssignedMemory(*it))
        ++has_memory_count;
    }
    return has_memory_count;
  }

  int TilesWithLCDCount(const TileVector& tiles) {
    int has_lcd_count = 0;
    for (TileVector::const_iterator it = tiles.begin();
         it != tiles.end();
         ++it) {
      if ((*it)->GetRasterModeForTesting() == HIGH_QUALITY_RASTER_MODE)
        ++has_lcd_count;
    }
    return has_lcd_count;
  }

 private:
  FakeTileManagerClient tile_manager_client_;
  LayerTreeSettings settings_;
  scoped_ptr<FakeTileManager> tile_manager_;
  scoped_refptr<FakePicturePileImpl> picture_pile_;
  scoped_ptr<FakeOutputSurface> output_surface_;
  scoped_ptr<ResourceProvider> resource_provider_;
  TileMemoryLimitPolicy memory_limit_policy_;
  int max_memory_tiles_;
};

TEST_P(TileManagerTest, EnoughMemoryAllowAnything) {
  // A few tiles of each type of priority, with enough memory for all tiles.

  Initialize(10, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_now =
      CreateTiles(3, TilePriorityForNowBin(), TilePriority());
  TileVector pending_now =
      CreateTiles(3, TilePriority(), TilePriorityForNowBin());
  TileVector active_pending_soon = CreateTiles(
      3, TilePriorityForSoonBin(), TilePriorityForSoonBin());
  TileVector never_bin = CreateTiles(1, TilePriority(), TilePriority());

  tile_manager()->AssignMemoryToTiles();

  EXPECT_EQ(3, AssignedMemoryCount(active_now));
  EXPECT_EQ(3, AssignedMemoryCount(pending_now));
  EXPECT_EQ(3, AssignedMemoryCount(active_pending_soon));
  EXPECT_EQ(0, AssignedMemoryCount(never_bin));
}

TEST_P(TileManagerTest, EnoughMemoryAllowPrepaintOnly) {
  // A few tiles of each type of priority, with enough memory for all tiles,
  // with the exception of never bin.

  Initialize(10, ALLOW_PREPAINT_ONLY, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_now =
      CreateTiles(3, TilePriorityForNowBin(), TilePriority());
  TileVector pending_now =
      CreateTiles(3, TilePriority(), TilePriorityForNowBin());
  TileVector active_pending_soon = CreateTiles(
      3, TilePriorityForSoonBin(), TilePriorityForSoonBin());
  TileVector never_bin = CreateTiles(1, TilePriority(), TilePriority());

  tile_manager()->AssignMemoryToTiles();

  EXPECT_EQ(3, AssignedMemoryCount(active_now));
  EXPECT_EQ(3, AssignedMemoryCount(pending_now));
  EXPECT_EQ(3, AssignedMemoryCount(active_pending_soon));
  EXPECT_EQ(0, AssignedMemoryCount(never_bin));
}

TEST_P(TileManagerTest, EnoughMemoryAllowAbsoluteMinimum) {
  // A few tiles of each type of priority, with enough memory for all tiles,
  // with the exception of never and soon bins.

  Initialize(10, ALLOW_ABSOLUTE_MINIMUM, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_now =
      CreateTiles(3, TilePriorityForNowBin(), TilePriority());
  TileVector pending_now =
      CreateTiles(3, TilePriority(), TilePriorityForNowBin());
  TileVector active_pending_soon = CreateTiles(
      3, TilePriorityForSoonBin(), TilePriorityForSoonBin());
  TileVector never_bin = CreateTiles(1, TilePriority(), TilePriority());

  tile_manager()->AssignMemoryToTiles();

  EXPECT_EQ(3, AssignedMemoryCount(active_now));
  EXPECT_EQ(3, AssignedMemoryCount(pending_now));
  EXPECT_EQ(0, AssignedMemoryCount(active_pending_soon));
  EXPECT_EQ(0, AssignedMemoryCount(never_bin));
}

TEST_P(TileManagerTest, EnoughMemoryAllowNothing) {
  // A few tiles of each type of priority, with enough memory for all tiles,
  // but allow nothing should not assign any memory.

  Initialize(10, ALLOW_NOTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_now =
      CreateTiles(3, TilePriorityForNowBin(), TilePriority());
  TileVector pending_now =
      CreateTiles(3, TilePriority(), TilePriorityForNowBin());
  TileVector active_pending_soon = CreateTiles(
      3, TilePriorityForSoonBin(), TilePriorityForSoonBin());
  TileVector never_bin = CreateTiles(1, TilePriority(), TilePriority());

  tile_manager()->AssignMemoryToTiles();

  EXPECT_EQ(0, AssignedMemoryCount(active_now));
  EXPECT_EQ(0, AssignedMemoryCount(pending_now));
  EXPECT_EQ(0, AssignedMemoryCount(active_pending_soon));
  EXPECT_EQ(0, AssignedMemoryCount(never_bin));
}

TEST_P(TileManagerTest, PartialOOMMemoryToPending) {
  // 5 tiles on active tree eventually bin, 5 tiles on pending tree that are
  // required for activation, but only enough memory for 8 tiles. The result
  // is all pending tree tiles get memory, and 3 of the active tree tiles
  // get memory.

  Initialize(8, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(5, TilePriorityForEventualBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(5, TilePriority(), TilePriorityRequiredForActivation());

  tile_manager()->AssignMemoryToTiles();

  EXPECT_EQ(5, AssignedMemoryCount(active_tree_tiles));
  EXPECT_EQ(3, AssignedMemoryCount(pending_tree_tiles));

  SetTreePriority(SAME_PRIORITY_FOR_BOTH_TREES);
  tile_manager()->AssignMemoryToTiles();

  EXPECT_EQ(3, AssignedMemoryCount(active_tree_tiles));
  EXPECT_EQ(5, AssignedMemoryCount(pending_tree_tiles));
}

TEST_P(TileManagerTest, PartialOOMMemoryToActive) {
  // 5 tiles on active tree eventually bin, 5 tiles on pending tree now bin,
  // but only enough memory for 8 tiles. The result is all active tree tiles
  // get memory, and 3 of the pending tree tiles get memory.

  Initialize(8, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(5, TilePriorityForNowBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(5, TilePriority(), TilePriorityForNowBin());

  tile_manager()->AssignMemoryToTiles();

  EXPECT_EQ(5, AssignedMemoryCount(active_tree_tiles));
  EXPECT_EQ(3, AssignedMemoryCount(pending_tree_tiles));
}

TEST_P(TileManagerTest, TotalOOMMemoryToPending) {
  // 5 tiles on active tree eventually bin, 5 tiles on pending tree that are
  // required for activation, but only enough memory for 4 tiles. The result
  // is 4 pending tree tiles get memory, and none of the active tree tiles
  // get memory.

  Initialize(4, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(5, TilePriorityForEventualBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(5, TilePriority(), TilePriorityRequiredForActivation());

  tile_manager()->AssignMemoryToTiles();

  EXPECT_EQ(4, AssignedMemoryCount(active_tree_tiles));
  EXPECT_EQ(0, AssignedMemoryCount(pending_tree_tiles));

  SetTreePriority(SAME_PRIORITY_FOR_BOTH_TREES);
  tile_manager()->AssignMemoryToTiles();

  EXPECT_EQ(0, AssignedMemoryCount(active_tree_tiles));
  EXPECT_EQ(4, AssignedMemoryCount(pending_tree_tiles));
}

TEST_P(TileManagerTest, TotalOOMActiveSoonMemoryToPending) {
  // 5 tiles on active tree soon bin, 5 tiles on pending tree that are
  // required for activation, but only enough memory for 4 tiles. The result
  // is 4 pending tree tiles get memory, and none of the active tree tiles
  // get memory.

  Initialize(4, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(5, TilePriorityForSoonBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(5, TilePriority(), TilePriorityRequiredForActivation());

  tile_manager()->AssignMemoryToTiles();

  EXPECT_EQ(4, AssignedMemoryCount(active_tree_tiles));
  EXPECT_EQ(0, AssignedMemoryCount(pending_tree_tiles));

  SetTreePriority(SAME_PRIORITY_FOR_BOTH_TREES);
  tile_manager()->AssignMemoryToTiles();

  EXPECT_EQ(0, AssignedMemoryCount(active_tree_tiles));
  EXPECT_EQ(4, AssignedMemoryCount(pending_tree_tiles));
}

TEST_P(TileManagerTest, TotalOOMMemoryToActive) {
  // 5 tiles on active tree eventually bin, 5 tiles on pending tree now bin,
  // but only enough memory for 4 tiles. The result is 5 active tree tiles
  // get memory, and none of the pending tree tiles get memory.

  Initialize(4, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(5, TilePriorityForNowBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(5, TilePriority(), TilePriorityForNowBin());

  tile_manager()->AssignMemoryToTiles();

  EXPECT_EQ(4, AssignedMemoryCount(active_tree_tiles));
  EXPECT_EQ(0, AssignedMemoryCount(pending_tree_tiles));
}



TEST_P(TileManagerTest, RasterAsLCD) {
  Initialize(20, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(5, TilePriorityForNowBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(5, TilePriority(), TilePriorityForNowBin());

  tile_manager()->ManageTiles();

  EXPECT_EQ(5, TilesWithLCDCount(active_tree_tiles));
  EXPECT_EQ(5, TilesWithLCDCount(pending_tree_tiles));
}

TEST_P(TileManagerTest, RasterAsNoLCD) {
  Initialize(20, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(5, TilePriorityForNowBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(5, TilePriority(), TilePriorityForNowBin());

  for (TileVector::iterator it = active_tree_tiles.begin();
       it != active_tree_tiles.end();
       ++it) {
    (*it)->set_can_use_lcd_text(false);
  }
  for (TileVector::iterator it = pending_tree_tiles.begin();
       it != pending_tree_tiles.end();
       ++it) {
    (*it)->set_can_use_lcd_text(false);
  }

  tile_manager()->ManageTiles();

  EXPECT_EQ(0, TilesWithLCDCount(active_tree_tiles));
  EXPECT_EQ(0, TilesWithLCDCount(pending_tree_tiles));
}

TEST_P(TileManagerTest, ReRasterAsNoLCD) {
  Initialize(20, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(5, TilePriorityForNowBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(5, TilePriority(), TilePriorityForNowBin());

  tile_manager()->ManageTiles();

  EXPECT_EQ(5, TilesWithLCDCount(active_tree_tiles));
  EXPECT_EQ(5, TilesWithLCDCount(pending_tree_tiles));

  for (TileVector::iterator it = active_tree_tiles.begin();
       it != active_tree_tiles.end();
       ++it) {
    (*it)->set_can_use_lcd_text(false);
  }
  for (TileVector::iterator it = pending_tree_tiles.begin();
       it != pending_tree_tiles.end();
       ++it) {
    (*it)->set_can_use_lcd_text(false);
  }

  tile_manager()->ManageTiles();

  EXPECT_EQ(0, TilesWithLCDCount(active_tree_tiles));
  EXPECT_EQ(0, TilesWithLCDCount(pending_tree_tiles));
}

TEST_P(TileManagerTest, NoTextDontReRasterAsNoLCD) {
  Initialize(20, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(5, TilePriorityForNowBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(5, TilePriority(), TilePriorityForNowBin());

  tile_manager()->ManageTiles();

  EXPECT_EQ(5, TilesWithLCDCount(active_tree_tiles));
  EXPECT_EQ(5, TilesWithLCDCount(pending_tree_tiles));

  for (TileVector::iterator it = active_tree_tiles.begin();
       it != active_tree_tiles.end();
       ++it) {
    ManagedTileState::TileVersion& tile_version =
        (*it)->GetTileVersionForTesting(HIGH_QUALITY_RASTER_MODE);
    tile_version.SetSolidColorForTesting(SkColorSetARGB(0, 0, 0, 0));
    (*it)->set_can_use_lcd_text(false);
    EXPECT_TRUE((*it)->IsReadyToDraw());
  }
  for (TileVector::iterator it = pending_tree_tiles.begin();
       it != pending_tree_tiles.end();
       ++it) {
    ManagedTileState::TileVersion& tile_version =
        (*it)->GetTileVersionForTesting(HIGH_QUALITY_RASTER_MODE);
    tile_version.SetSolidColorForTesting(SkColorSetARGB(0, 0, 0, 0));
    (*it)->set_can_use_lcd_text(false);
    EXPECT_TRUE((*it)->IsReadyToDraw());
  }

  tile_manager()->ManageTiles();

  EXPECT_EQ(5, TilesWithLCDCount(active_tree_tiles));
  EXPECT_EQ(5, TilesWithLCDCount(pending_tree_tiles));
}

TEST_P(TileManagerTest, TextReRasterAsNoLCD) {
  Initialize(20, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(5, TilePriorityForNowBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(5, TilePriority(), TilePriorityForNowBin());

  tile_manager()->ManageTiles();

  EXPECT_EQ(5, TilesWithLCDCount(active_tree_tiles));
  EXPECT_EQ(5, TilesWithLCDCount(pending_tree_tiles));

  for (TileVector::iterator it = active_tree_tiles.begin();
       it != active_tree_tiles.end();
       ++it) {
    ManagedTileState::TileVersion& tile_version =
        (*it)->GetTileVersionForTesting(HIGH_QUALITY_RASTER_MODE);
    tile_version.SetSolidColorForTesting(SkColorSetARGB(0, 0, 0, 0));
    tile_version.SetHasTextForTesting(true);
    (*it)->set_can_use_lcd_text(false);

    EXPECT_TRUE((*it)->IsReadyToDraw());
  }
  for (TileVector::iterator it = pending_tree_tiles.begin();
       it != pending_tree_tiles.end();
       ++it) {
    ManagedTileState::TileVersion& tile_version =
        (*it)->GetTileVersionForTesting(HIGH_QUALITY_RASTER_MODE);
    tile_version.SetSolidColorForTesting(
        SkColorSetARGB(0, 0, 0, 0));
    tile_version.SetHasTextForTesting(true);
    (*it)->set_can_use_lcd_text(false);

    EXPECT_TRUE((*it)->IsReadyToDraw());
  }

  tile_manager()->ManageTiles();

  EXPECT_EQ(0, TilesWithLCDCount(active_tree_tiles));
  EXPECT_EQ(0, TilesWithLCDCount(pending_tree_tiles));
}

// If true, the max tile limit should be applied as bytes; if false,
// as num_resources_limit.
INSTANTIATE_TEST_CASE_P(TileManagerTests,
                        TileManagerTest,
                        ::testing::Values(true, false));

}  // namespace
}  // namespace cc
