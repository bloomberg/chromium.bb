// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tile.h"
#include "cc/resources/tile_priority.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_picture_pile_impl.h"
#include "cc/test/fake_tile_manager.h"
#include "cc/test/test_tile_priorities.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class TileManagerTest : public testing::TestWithParam<bool>,
                        public TileManagerClient {
 public:
  typedef std::vector<scoped_refptr<Tile> > TileVector;

  TileManagerTest()
      : memory_limit_policy_(ALLOW_ANYTHING),
        max_tiles_(0),
        ready_to_activate_(false) {}

  void Initialize(int max_tiles,
                  TileMemoryLimitPolicy memory_limit_policy,
                  TreePriority tree_priority,
                  bool allow_on_demand_raster = true) {
    output_surface_ = FakeOutputSurface::Create3d();
    CHECK(output_surface_->BindToClient(&output_surface_client_));

    resource_provider_ =
        ResourceProvider::Create(output_surface_.get(), NULL, 0, false, 1);
    tile_manager_ = make_scoped_ptr(new FakeTileManager(
        this, resource_provider_.get(), allow_on_demand_raster));

    memory_limit_policy_ = memory_limit_policy;
    max_tiles_ = max_tiles;
    picture_pile_ = FakePicturePileImpl::CreatePile();

    SetTreePriority(tree_priority);
  }

  void SetTreePriority(TreePriority tree_priority) {
    GlobalStateThatImpactsTilePriority state;
    gfx::Size tile_size = settings_.default_tile_size;

    if (UsingMemoryLimit()) {
      state.soft_memory_limit_in_bytes =
          max_tiles_ * 4 * tile_size.width() * tile_size.height();
      state.num_resources_limit = 100;
    } else {
      state.soft_memory_limit_in_bytes = 100 * 1000 * 1000;
      state.num_resources_limit = max_tiles_;
    }
    state.hard_memory_limit_in_bytes = state.soft_memory_limit_in_bytes * 2;
    state.unused_memory_limit_in_bytes = state.soft_memory_limit_in_bytes;
    state.memory_limit_policy = memory_limit_policy_;
    state.tree_priority = tree_priority;

    global_state_ = state;
    tile_manager_->SetGlobalStateForTesting(state);
  }

  virtual void TearDown() OVERRIDE {
    tile_manager_.reset(NULL);
    picture_pile_ = NULL;

    testing::Test::TearDown();
  }

  // TileManagerClient implementation.
  virtual void NotifyReadyToActivate() OVERRIDE { ready_to_activate_ = true; }

  TileVector CreateTilesWithSize(int count,
                                 TilePriority active_priority,
                                 TilePriority pending_priority,
                                 const gfx::Size& tile_size) {
    TileVector tiles;
    for (int i = 0; i < count; ++i) {
      scoped_refptr<Tile> tile = tile_manager_->CreateTile(picture_pile_.get(),
                                                           tile_size,
                                                           gfx::Rect(),
                                                           gfx::Rect(),
                                                           1.0,
                                                           0,
                                                           0,
                                                           Tile::USE_LCD_TEXT);
      tile->SetPriority(ACTIVE_TREE, active_priority);
      tile->SetPriority(PENDING_TREE, pending_priority);
      tiles.push_back(tile);
    }
    return tiles;
  }

  TileVector CreateTiles(int count,
                         TilePriority active_priority,
                         TilePriority pending_priority) {
    return CreateTilesWithSize(
        count, active_priority, pending_priority, settings_.default_tile_size);
  }

  FakeTileManager* tile_manager() { return tile_manager_.get(); }

  int AssignedMemoryCount(const TileVector& tiles) {
    int has_memory_count = 0;
    for (TileVector::const_iterator it = tiles.begin(); it != tiles.end();
         ++it) {
      if (tile_manager_->HasBeenAssignedMemory(*it))
        ++has_memory_count;
    }
    return has_memory_count;
  }

  int TilesWithLCDCount(const TileVector& tiles) {
    int has_lcd_count = 0;
    for (TileVector::const_iterator it = tiles.begin(); it != tiles.end();
         ++it) {
      if ((*it)->GetRasterModeForTesting() == HIGH_QUALITY_RASTER_MODE)
        ++has_lcd_count;
    }
    return has_lcd_count;
  }

  bool ready_to_activate() const { return ready_to_activate_; }

  // The parametrization specifies whether the max tile limit should
  // be applied to memory or resources.
  bool UsingResourceLimit() { return !GetParam(); }
  bool UsingMemoryLimit() { return GetParam(); }

 protected:
  GlobalStateThatImpactsTilePriority global_state_;

 private:
  LayerTreeSettings settings_;
  scoped_ptr<FakeTileManager> tile_manager_;
  scoped_refptr<FakePicturePileImpl> picture_pile_;
  FakeOutputSurfaceClient output_surface_client_;
  scoped_ptr<FakeOutputSurface> output_surface_;
  scoped_ptr<ResourceProvider> resource_provider_;
  TileMemoryLimitPolicy memory_limit_policy_;
  int max_tiles_;
  bool ready_to_activate_;
};

TEST_P(TileManagerTest, EnoughMemoryAllowAnything) {
  // A few tiles of each type of priority, with enough memory for all tiles.

  Initialize(10, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_now =
      CreateTiles(3, TilePriorityForNowBin(), TilePriority());
  TileVector pending_now =
      CreateTiles(3, TilePriority(), TilePriorityForNowBin());
  TileVector active_pending_soon =
      CreateTiles(3, TilePriorityForSoonBin(), TilePriorityForSoonBin());
  TileVector never_bin = CreateTiles(1, TilePriority(), TilePriority());

  tile_manager()->AssignMemoryToTiles(global_state_);

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
  TileVector active_pending_soon =
      CreateTiles(3, TilePriorityForSoonBin(), TilePriorityForSoonBin());
  TileVector never_bin = CreateTiles(1, TilePriority(), TilePriority());

  tile_manager()->AssignMemoryToTiles(global_state_);

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
  TileVector active_pending_soon =
      CreateTiles(3, TilePriorityForSoonBin(), TilePriorityForSoonBin());
  TileVector never_bin = CreateTiles(1, TilePriority(), TilePriority());

  tile_manager()->AssignMemoryToTiles(global_state_);

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
  TileVector active_pending_soon =
      CreateTiles(3, TilePriorityForSoonBin(), TilePriorityForSoonBin());
  TileVector never_bin = CreateTiles(1, TilePriority(), TilePriority());

  tile_manager()->AssignMemoryToTiles(global_state_);

  EXPECT_EQ(0, AssignedMemoryCount(active_now));
  EXPECT_EQ(0, AssignedMemoryCount(pending_now));
  EXPECT_EQ(0, AssignedMemoryCount(active_pending_soon));
  EXPECT_EQ(0, AssignedMemoryCount(never_bin));
}

TEST_P(TileManagerTest, PartialOOMMemoryToPending) {
  // 5 tiles on active tree eventually bin, 5 tiles on pending tree that are
  // required for activation, but only enough memory for 8 tiles. The result
  // is all pending tree tiles get memory, and 3 of the active tree tiles
  // get memory. None of these tiles is needed to avoid calimity (flickering or
  // raster-on-demand) so the soft memory limit is used.

  Initialize(8, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(5, TilePriorityForEventualBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(5, TilePriority(), TilePriorityRequiredForActivation());
  tile_manager()->AssignMemoryToTiles(global_state_);

  EXPECT_EQ(5, AssignedMemoryCount(active_tree_tiles));
  EXPECT_EQ(3, AssignedMemoryCount(pending_tree_tiles));

  SetTreePriority(SAME_PRIORITY_FOR_BOTH_TREES);
  tile_manager()->AssignMemoryToTiles(global_state_);

  EXPECT_EQ(3, AssignedMemoryCount(active_tree_tiles));
  EXPECT_EQ(5, AssignedMemoryCount(pending_tree_tiles));
}

TEST_P(TileManagerTest, PartialOOMMemoryToActive) {
  // 5 tiles on active tree eventually bin, 5 tiles on pending tree now bin,
  // but only enough memory for 8 tiles. The result is all active tree tiles
  // get memory, and 3 of the pending tree tiles get memory.
  // The pending tiles are not needed to avoid calimity (flickering or
  // raster-on-demand) and the active tiles fit, so the soft limit is used.

  Initialize(8, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(5, TilePriorityForNowBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(5, TilePriority(), TilePriorityForNowBin());

  tile_manager()->AssignMemoryToTiles(global_state_);

  EXPECT_EQ(5, AssignedMemoryCount(active_tree_tiles));
  EXPECT_EQ(3, AssignedMemoryCount(pending_tree_tiles));
}

TEST_P(TileManagerTest, TotalOOMMemoryToPending) {
  // 10 tiles on active tree eventually bin, 10 tiles on pending tree that are
  // required for activation, but only enough tiles for 4 tiles. The result
  // is 4 pending tree tiles get memory, and none of the active tree tiles
  // get memory.

  Initialize(4, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(10, TilePriorityForEventualBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(10, TilePriority(), TilePriorityRequiredForActivation());

  tile_manager()->AssignMemoryToTiles(global_state_);

  EXPECT_EQ(4, AssignedMemoryCount(active_tree_tiles));
  EXPECT_EQ(0, AssignedMemoryCount(pending_tree_tiles));

  SetTreePriority(SAME_PRIORITY_FOR_BOTH_TREES);
  tile_manager()->AssignMemoryToTiles(global_state_);

  if (UsingResourceLimit()) {
    EXPECT_EQ(0, AssignedMemoryCount(active_tree_tiles));
    EXPECT_EQ(4, AssignedMemoryCount(pending_tree_tiles));
  } else {
    // Pending tiles are now required to avoid calimity (flickering or
    // raster-on-demand). Hard-limit is used and double the tiles fit.
    EXPECT_EQ(0, AssignedMemoryCount(active_tree_tiles));
    EXPECT_EQ(8, AssignedMemoryCount(pending_tree_tiles));
  }
}

TEST_P(TileManagerTest, TotalOOMActiveSoonMemoryToPending) {
  // 10 tiles on active tree soon bin, 10 tiles on pending tree that are
  // required for activation, but only enough tiles for 4 tiles. The result
  // is 4 pending tree tiles get memory, and none of the active tree tiles
  // get memory.

  Initialize(4, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(10, TilePriorityForSoonBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(10, TilePriority(), TilePriorityRequiredForActivation());

  tile_manager()->AssignMemoryToTiles(global_state_);

  EXPECT_EQ(4, AssignedMemoryCount(active_tree_tiles));
  EXPECT_EQ(0, AssignedMemoryCount(pending_tree_tiles));

  SetTreePriority(SAME_PRIORITY_FOR_BOTH_TREES);
  tile_manager()->AssignMemoryToTiles(global_state_);

  if (UsingResourceLimit()) {
    EXPECT_EQ(0, AssignedMemoryCount(active_tree_tiles));
    EXPECT_EQ(4, AssignedMemoryCount(pending_tree_tiles));
  } else {
    // Pending tiles are now required to avoid calimity (flickering or
    // raster-on-demand). Hard-limit is used and double the tiles fit.
    EXPECT_EQ(0, AssignedMemoryCount(active_tree_tiles));
    EXPECT_EQ(8, AssignedMemoryCount(pending_tree_tiles));
  }
}

TEST_P(TileManagerTest, TotalOOMMemoryToActive) {
  // 10 tiles on active tree eventually bin, 10 tiles on pending tree now bin,
  // but only enough memory for 4 tiles. The result is 4 active tree tiles
  // get memory, and none of the pending tree tiles get memory.

  Initialize(4, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(10, TilePriorityForNowBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(10, TilePriority(), TilePriorityForNowBin());

  tile_manager()->AssignMemoryToTiles(global_state_);

  if (UsingResourceLimit()) {
    EXPECT_EQ(4, AssignedMemoryCount(active_tree_tiles));
    EXPECT_EQ(0, AssignedMemoryCount(pending_tree_tiles));
  } else {
    // Active tiles are required to avoid calimity (flickering or
    // raster-on-demand). Hard-limit is used and double the tiles fit.
    EXPECT_EQ(8, AssignedMemoryCount(active_tree_tiles));
    EXPECT_EQ(0, AssignedMemoryCount(pending_tree_tiles));
  }
}

TEST_P(TileManagerTest, TotalOOMMemoryToNewContent) {
  // 10 tiles on active tree now bin, 10 tiles on pending tree now bin,
  // but only enough memory for 8 tiles. Any tile missing would cause
  // a calamity (flickering or raster-on-demand). Depending on mode,
  // we should use varying amounts of the higher hard memory limit.
  if (UsingResourceLimit())
    return;

  Initialize(8, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(10, TilePriorityForNowBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(10, TilePriority(), TilePriorityForNowBin());

  // Active tiles are required to avoid calimity. The hard-limit is used and all
  // active-tiles fit. No pending tiles are needed to avoid calamity so only 10
  // tiles total are used.
  tile_manager()->AssignMemoryToTiles(global_state_);
  EXPECT_EQ(10, AssignedMemoryCount(active_tree_tiles));
  EXPECT_EQ(0, AssignedMemoryCount(pending_tree_tiles));

  // Even the hard-limit won't save us now. All tiles are required to avoid
  // a clamity but we only have 16. The tiles will be distribted randomly
  // given they are identical, in practice depending on their screen location.
  SetTreePriority(SAME_PRIORITY_FOR_BOTH_TREES);
  tile_manager()->AssignMemoryToTiles(global_state_);
  EXPECT_EQ(16,
            AssignedMemoryCount(active_tree_tiles) +
                AssignedMemoryCount(pending_tree_tiles));

  // The pending tree is now more important. Active tiles will take higher
  // priority if they are ready-to-draw in practice. Importantly though,
  // pending tiles also utilize the hard-limit.
  SetTreePriority(NEW_CONTENT_TAKES_PRIORITY);
  tile_manager()->AssignMemoryToTiles(global_state_);
  EXPECT_EQ(0, AssignedMemoryCount(active_tree_tiles));
  EXPECT_EQ(10, AssignedMemoryCount(pending_tree_tiles));
}

TEST_P(TileManagerTest, RasterAsLCD) {
  Initialize(20, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(5, TilePriorityForNowBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(5, TilePriority(), TilePriorityForNowBin());

  tile_manager()->AssignMemoryToTiles(global_state_);

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

  tile_manager()->AssignMemoryToTiles(global_state_);

  EXPECT_EQ(0, TilesWithLCDCount(active_tree_tiles));
  EXPECT_EQ(0, TilesWithLCDCount(pending_tree_tiles));
}

TEST_P(TileManagerTest, ReRasterAsNoLCD) {
  Initialize(20, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(5, TilePriorityForNowBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(5, TilePriority(), TilePriorityForNowBin());

  tile_manager()->AssignMemoryToTiles(global_state_);

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

  tile_manager()->AssignMemoryToTiles(global_state_);

  EXPECT_EQ(0, TilesWithLCDCount(active_tree_tiles));
  EXPECT_EQ(0, TilesWithLCDCount(pending_tree_tiles));
}

TEST_P(TileManagerTest, NoTextDontReRasterAsNoLCD) {
  Initialize(20, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(5, TilePriorityForNowBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(5, TilePriority(), TilePriorityForNowBin());

  tile_manager()->AssignMemoryToTiles(global_state_);

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

  tile_manager()->AssignMemoryToTiles(global_state_);

  EXPECT_EQ(5, TilesWithLCDCount(active_tree_tiles));
  EXPECT_EQ(5, TilesWithLCDCount(pending_tree_tiles));
}

TEST_P(TileManagerTest, TextReRasterAsNoLCD) {
  Initialize(20, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(5, TilePriorityForNowBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(5, TilePriority(), TilePriorityForNowBin());

  tile_manager()->AssignMemoryToTiles(global_state_);

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
    tile_version.SetSolidColorForTesting(SkColorSetARGB(0, 0, 0, 0));
    tile_version.SetHasTextForTesting(true);
    (*it)->set_can_use_lcd_text(false);

    EXPECT_TRUE((*it)->IsReadyToDraw());
  }

  tile_manager()->AssignMemoryToTiles(global_state_);

  EXPECT_EQ(0, TilesWithLCDCount(active_tree_tiles));
  EXPECT_EQ(0, TilesWithLCDCount(pending_tree_tiles));
}

TEST_P(TileManagerTest, RespectMemoryLimit) {
  if (UsingResourceLimit())
    return;

  Initialize(5, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);

  // We use double the tiles since the hard-limit is double.
  TileVector large_tiles =
      CreateTiles(10, TilePriorityForNowBin(), TilePriority());

  size_t memory_required_bytes;
  size_t memory_nice_to_have_bytes;
  size_t memory_allocated_bytes;
  size_t memory_used_bytes;

  tile_manager()->AssignMemoryToTiles(global_state_);
  tile_manager()->GetMemoryStats(&memory_required_bytes,
                                 &memory_nice_to_have_bytes,
                                 &memory_allocated_bytes,
                                 &memory_used_bytes);
  // Allocated bytes should never be more than the memory limit.
  EXPECT_LE(memory_allocated_bytes, global_state_.hard_memory_limit_in_bytes);

  // Finish raster of large tiles.
  tile_manager()->UpdateVisibleTiles();

  // Remove all large tiles. This will leave the memory currently
  // used by these tiles as unused when AssignMemoryToTiles() is called.
  large_tiles.clear();

  // Create a new set of tiles using a different size. These tiles
  // can use the memory currently assigned to the large tiles but
  // they can't use the same resources as the size doesn't match.
  TileVector small_tiles = CreateTilesWithSize(
      10, TilePriorityForNowBin(), TilePriority(), gfx::Size(128, 128));

  tile_manager()->AssignMemoryToTiles(global_state_);
  tile_manager()->GetMemoryStats(&memory_required_bytes,
                                 &memory_nice_to_have_bytes,
                                 &memory_allocated_bytes,
                                 &memory_used_bytes);
  // Allocated bytes should never be more than the memory limit.
  EXPECT_LE(memory_allocated_bytes, global_state_.hard_memory_limit_in_bytes);
}

TEST_P(TileManagerTest, AllowRasterizeOnDemand) {
  // Not enough memory to initialize tiles required for activation.
  Initialize(0, ALLOW_ANYTHING, SAME_PRIORITY_FOR_BOTH_TREES);
  TileVector tiles =
      CreateTiles(2, TilePriority(), TilePriorityRequiredForActivation());

  tile_manager()->AssignMemoryToTiles(global_state_);

  // This should make required tiles ready to draw by marking them as
  // required tiles for on-demand raster.
  tile_manager()->DidFinishRunningTasksForTesting();

  EXPECT_TRUE(ready_to_activate());
  for (TileVector::iterator it = tiles.begin(); it != tiles.end(); ++it)
    EXPECT_TRUE((*it)->IsReadyToDraw());
}

TEST_P(TileManagerTest, PreventRasterizeOnDemand) {
  // Not enough memory to initialize tiles required for activation.
  Initialize(0, ALLOW_ANYTHING, SAME_PRIORITY_FOR_BOTH_TREES, false);
  TileVector tiles =
      CreateTiles(2, TilePriority(), TilePriorityRequiredForActivation());

  tile_manager()->AssignMemoryToTiles(global_state_);

  // This should make required tiles ready to draw by marking them as
  // required tiles for on-demand raster.
  tile_manager()->DidFinishRunningTasksForTesting();

  EXPECT_TRUE(ready_to_activate());
  for (TileVector::iterator it = tiles.begin(); it != tiles.end(); ++it)
    EXPECT_FALSE((*it)->IsReadyToDraw());
}

// If true, the max tile limit should be applied as bytes; if false,
// as num_resources_limit.
INSTANTIATE_TEST_CASE_P(TileManagerTests,
                        TileManagerTest,
                        ::testing::Values(true, false));

}  // namespace
}  // namespace cc
