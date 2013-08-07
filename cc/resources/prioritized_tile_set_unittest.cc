// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "cc/resources/managed_tile_state.h"
#include "cc/resources/prioritized_tile_set.h"
#include "cc/resources/tile.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_picture_pile_impl.h"
#include "cc/test/fake_tile_manager.h"
#include "cc/test/fake_tile_manager_client.h"
#include "cc/test/test_tile_priorities.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class BinComparator {
 public:
  bool operator()(const scoped_refptr<Tile>& a,
                  const scoped_refptr<Tile>& b) const {
    const ManagedTileState& ams = a->managed_state();
    const ManagedTileState& bms = b->managed_state();

    if (ams.bin[LOW_PRIORITY_BIN] != bms.bin[LOW_PRIORITY_BIN])
      return ams.bin[LOW_PRIORITY_BIN] < bms.bin[LOW_PRIORITY_BIN];

    if (ams.required_for_activation != bms.required_for_activation)
      return ams.required_for_activation;

    if (ams.resolution != bms.resolution)
      return ams.resolution < bms.resolution;

    if (ams.time_to_needed_in_seconds !=  bms.time_to_needed_in_seconds)
      return ams.time_to_needed_in_seconds < bms.time_to_needed_in_seconds;

    if (ams.distance_to_visible_in_pixels !=
        bms.distance_to_visible_in_pixels) {
      return ams.distance_to_visible_in_pixels <
             bms.distance_to_visible_in_pixels;
    }

    gfx::Rect a_rect = a->content_rect();
    gfx::Rect b_rect = b->content_rect();
    if (a_rect.y() != b_rect.y())
      return a_rect.y() < b_rect.y();
    return a_rect.x() < b_rect.x();
  }
};

namespace {

class PrioritizedTileSetTest : public testing::Test {
 public:
  PrioritizedTileSetTest()
      : output_surface_(FakeOutputSurface::Create3d()),
        resource_provider_(ResourceProvider::Create(output_surface_.get(), 0)),
        tile_manager_(new FakeTileManager(&tile_manager_client_,
                                          resource_provider_.get())),
        picture_pile_(FakePicturePileImpl::CreatePile()) {}

  scoped_refptr<Tile> CreateTile() {
    return make_scoped_refptr(new Tile(tile_manager_.get(),
                                       picture_pile_.get(),
                                       settings_.default_tile_size,
                                       gfx::Rect(),
                                       gfx::Rect(),
                                       1.0,
                                       0,
                                       0,
                                       true));
  }

 private:
  FakeTileManagerClient tile_manager_client_;
  LayerTreeSettings settings_;
  scoped_ptr<FakeOutputSurface> output_surface_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<FakeTileManager> tile_manager_;
  scoped_refptr<FakePicturePileImpl> picture_pile_;
};

TEST_F(PrioritizedTileSetTest, EmptyIterator) {
  PrioritizedTileSet set;
  set.Sort();

  PrioritizedTileSet::PriorityIterator it(&set);
  EXPECT_FALSE(it);
}

TEST_F(PrioritizedTileSetTest, NonEmptyIterator) {
  PrioritizedTileSet set;
  scoped_refptr<Tile> tile = CreateTile();
  set.InsertTile(tile, NOW_BIN);
  set.Sort();

  PrioritizedTileSet::PriorityIterator it(&set);
  EXPECT_TRUE(it);
  EXPECT_TRUE(*it == tile.get());
  ++it;
  EXPECT_FALSE(it);
}

TEST_F(PrioritizedTileSetTest, NowAndReadyToDrawBin) {
  PrioritizedTileSet set;
  TilePriority priorities[4] = {
      TilePriorityForEventualBin(),
      TilePriorityForNowBin(),
      TilePriority(),
      TilePriorityForSoonBin()};

  std::vector<scoped_refptr<Tile> > tiles;
  for (int priority = 0; priority < 4; ++priority) {
    for (int i = 0; i < 5; ++i) {
      scoped_refptr<Tile> tile = CreateTile();
      tile->SetPriority(ACTIVE_TREE, priorities[priority]);
      tile->SetPriority(PENDING_TREE, priorities[priority]);
      tiles.push_back(tile);
      set.InsertTile(tile, NOW_AND_READY_TO_DRAW_BIN);
    }
  }

  set.Sort();

  // Tiles should appear in the same order as inserted.
  int i = 0;
  for (PrioritizedTileSet::PriorityIterator it(&set);
       it;
       ++it) {
    EXPECT_TRUE(*it == tiles[i].get());
    ++i;
  }
  EXPECT_EQ(20, i);
}

TEST_F(PrioritizedTileSetTest, NowBin) {
  PrioritizedTileSet set;
  TilePriority priorities[4] = {
      TilePriorityForEventualBin(),
      TilePriorityForNowBin(),
      TilePriority(),
      TilePriorityForSoonBin()};

  std::vector<scoped_refptr<Tile> > tiles;
  for (int priority = 0; priority < 4; ++priority) {
    for (int i = 0; i < 5; ++i) {
      scoped_refptr<Tile> tile = CreateTile();
      tile->SetPriority(ACTIVE_TREE, priorities[priority]);
      tile->SetPriority(PENDING_TREE, priorities[priority]);
      tiles.push_back(tile);
      set.InsertTile(tile, NOW_BIN);
    }
  }

  set.Sort();

  // Tiles should appear in BinComparator order.
  std::sort(tiles.begin(), tiles.end(), BinComparator());

  int i = 0;
  for (PrioritizedTileSet::PriorityIterator it(&set);
       it;
       ++it) {
    EXPECT_TRUE(*it == tiles[i].get());
    ++i;
  }
  EXPECT_EQ(20, i);
}

TEST_F(PrioritizedTileSetTest, SoonBin) {
  PrioritizedTileSet set;
  TilePriority priorities[4] = {
      TilePriorityForEventualBin(),
      TilePriorityForNowBin(),
      TilePriority(),
      TilePriorityForSoonBin()};

  std::vector<scoped_refptr<Tile> > tiles;
  for (int priority = 0; priority < 4; ++priority) {
    for (int i = 0; i < 5; ++i) {
      scoped_refptr<Tile> tile = CreateTile();
      tile->SetPriority(ACTIVE_TREE, priorities[priority]);
      tile->SetPriority(PENDING_TREE, priorities[priority]);
      tiles.push_back(tile);
      set.InsertTile(tile, SOON_BIN);
    }
  }

  set.Sort();

  // Tiles should appear in BinComparator order.
  std::sort(tiles.begin(), tiles.end(), BinComparator());

  int i = 0;
  for (PrioritizedTileSet::PriorityIterator it(&set);
       it;
       ++it) {
    EXPECT_TRUE(*it == tiles[i].get());
    ++i;
  }
  EXPECT_EQ(20, i);
}

TEST_F(PrioritizedTileSetTest, EventuallyAndActiveBin) {
  PrioritizedTileSet set;
  TilePriority priorities[4] = {
      TilePriorityForEventualBin(),
      TilePriorityForNowBin(),
      TilePriority(),
      TilePriorityForSoonBin()};

  std::vector<scoped_refptr<Tile> > tiles;
  for (int priority = 0; priority < 4; ++priority) {
    for (int i = 0; i < 5; ++i) {
      scoped_refptr<Tile> tile = CreateTile();
      tile->SetPriority(ACTIVE_TREE, priorities[priority]);
      tile->SetPriority(PENDING_TREE, priorities[priority]);
      tiles.push_back(tile);
      set.InsertTile(tile, EVENTUALLY_AND_ACTIVE_BIN);
    }
  }

  set.Sort();

  // Tiles should appear in BinComparator order.
  std::sort(tiles.begin(), tiles.end(), BinComparator());

  int i = 0;
  for (PrioritizedTileSet::PriorityIterator it(&set);
       it;
       ++it) {
    EXPECT_TRUE(*it == tiles[i].get());
    ++i;
  }
  EXPECT_EQ(20, i);
}

TEST_F(PrioritizedTileSetTest, EventuallyBin) {
  PrioritizedTileSet set;
  TilePriority priorities[4] = {
      TilePriorityForEventualBin(),
      TilePriorityForNowBin(),
      TilePriority(),
      TilePriorityForSoonBin()};

  std::vector<scoped_refptr<Tile> > tiles;
  for (int priority = 0; priority < 4; ++priority) {
    for (int i = 0; i < 5; ++i) {
      scoped_refptr<Tile> tile = CreateTile();
      tile->SetPriority(ACTIVE_TREE, priorities[priority]);
      tile->SetPriority(PENDING_TREE, priorities[priority]);
      tiles.push_back(tile);
      set.InsertTile(tile, EVENTUALLY_BIN);
    }
  }

  set.Sort();

  // Tiles should appear in BinComparator order.
  std::sort(tiles.begin(), tiles.end(), BinComparator());

  int i = 0;
  for (PrioritizedTileSet::PriorityIterator it(&set);
       it;
       ++it) {
    EXPECT_TRUE(*it == tiles[i].get());
    ++i;
  }
  EXPECT_EQ(20, i);
}

TEST_F(PrioritizedTileSetTest, NeverAndActiveBin) {
  PrioritizedTileSet set;
  TilePriority priorities[4] = {
      TilePriorityForEventualBin(),
      TilePriorityForNowBin(),
      TilePriority(),
      TilePriorityForSoonBin()};

  std::vector<scoped_refptr<Tile> > tiles;
  for (int priority = 0; priority < 4; ++priority) {
    for (int i = 0; i < 5; ++i) {
      scoped_refptr<Tile> tile = CreateTile();
      tile->SetPriority(ACTIVE_TREE, priorities[priority]);
      tile->SetPriority(PENDING_TREE, priorities[priority]);
      tiles.push_back(tile);
      set.InsertTile(tile, NEVER_AND_ACTIVE_BIN);
    }
  }

  set.Sort();

  // Tiles should appear in BinComparator order.
  std::sort(tiles.begin(), tiles.end(), BinComparator());

  int i = 0;
  for (PrioritizedTileSet::PriorityIterator it(&set);
       it;
       ++it) {
    EXPECT_TRUE(*it == tiles[i].get());
    ++i;
  }
  EXPECT_EQ(20, i);
}

TEST_F(PrioritizedTileSetTest, NeverBin) {
  PrioritizedTileSet set;
  TilePriority priorities[4] = {
      TilePriorityForEventualBin(),
      TilePriorityForNowBin(),
      TilePriority(),
      TilePriorityForSoonBin()};

  std::vector<scoped_refptr<Tile> > tiles;
  for (int priority = 0; priority < 4; ++priority) {
    for (int i = 0; i < 5; ++i) {
      scoped_refptr<Tile> tile = CreateTile();
      tile->SetPriority(ACTIVE_TREE, priorities[priority]);
      tile->SetPriority(PENDING_TREE, priorities[priority]);
      tiles.push_back(tile);
      set.InsertTile(tile, NEVER_BIN);
    }
  }

  set.Sort();

  // Tiles should appear in BinComparator order.
  std::sort(tiles.begin(), tiles.end(), BinComparator());

  int i = 0;
  for (PrioritizedTileSet::PriorityIterator it(&set);
       it;
       ++it) {
    EXPECT_TRUE(*it == tiles[i].get());
    ++i;
  }
  EXPECT_EQ(20, i);
}

TEST_F(PrioritizedTileSetTest, TilesForEachBin) {
  scoped_refptr<Tile> now_and_ready_to_draw_bin = CreateTile();
  scoped_refptr<Tile> now_bin = CreateTile();
  scoped_refptr<Tile> soon_bin = CreateTile();
  scoped_refptr<Tile> eventually_and_active_bin = CreateTile();
  scoped_refptr<Tile> eventually_bin = CreateTile();
  scoped_refptr<Tile> never_bin = CreateTile();
  scoped_refptr<Tile> never_and_active_bin = CreateTile();

  PrioritizedTileSet set;
  set.InsertTile(soon_bin, SOON_BIN);
  set.InsertTile(never_and_active_bin, NEVER_AND_ACTIVE_BIN);
  set.InsertTile(eventually_bin, EVENTUALLY_BIN);
  set.InsertTile(now_bin, NOW_BIN);
  set.InsertTile(eventually_and_active_bin, EVENTUALLY_AND_ACTIVE_BIN);
  set.InsertTile(never_bin, NEVER_BIN);
  set.InsertTile(now_and_ready_to_draw_bin, NOW_AND_READY_TO_DRAW_BIN);

  set.Sort();

  // Tiles should appear in order.
  PrioritizedTileSet::PriorityIterator it(&set);
  EXPECT_TRUE(*it == now_and_ready_to_draw_bin.get());
  ++it;
  EXPECT_TRUE(*it == now_bin.get());
  ++it;
  EXPECT_TRUE(*it == soon_bin.get());
  ++it;
  EXPECT_TRUE(*it == eventually_and_active_bin.get());
  ++it;
  EXPECT_TRUE(*it == eventually_bin.get());
  ++it;
  EXPECT_TRUE(*it == never_and_active_bin.get());
  ++it;
  EXPECT_TRUE(*it == never_bin.get());
  ++it;
  EXPECT_FALSE(it);
}

TEST_F(PrioritizedTileSetTest, TilesForFirstAndLastBins) {
  scoped_refptr<Tile> now_and_ready_to_draw_bin = CreateTile();
  scoped_refptr<Tile> never_bin = CreateTile();

  PrioritizedTileSet set;
  set.InsertTile(never_bin, NEVER_BIN);
  set.InsertTile(now_and_ready_to_draw_bin, NOW_AND_READY_TO_DRAW_BIN);

  set.Sort();

  // Only two tiles should appear and they should appear in order.
  PrioritizedTileSet::PriorityIterator it(&set);
  EXPECT_TRUE(*it == now_and_ready_to_draw_bin.get());
  ++it;
  EXPECT_TRUE(*it == never_bin.get());
  ++it;
  EXPECT_FALSE(it);
}

TEST_F(PrioritizedTileSetTest, MultipleIterators) {
  scoped_refptr<Tile> now_and_ready_to_draw_bin = CreateTile();
  scoped_refptr<Tile> now_bin = CreateTile();
  scoped_refptr<Tile> soon_bin = CreateTile();
  scoped_refptr<Tile> eventually_bin = CreateTile();
  scoped_refptr<Tile> never_bin = CreateTile();

  PrioritizedTileSet set;
  set.InsertTile(soon_bin, SOON_BIN);
  set.InsertTile(eventually_bin, EVENTUALLY_BIN);
  set.InsertTile(now_bin, NOW_BIN);
  set.InsertTile(never_bin, NEVER_BIN);
  set.InsertTile(now_and_ready_to_draw_bin, NOW_AND_READY_TO_DRAW_BIN);

  set.Sort();

  // Tiles should appear in order.
  PrioritizedTileSet::PriorityIterator it(&set);
  EXPECT_TRUE(*it == now_and_ready_to_draw_bin.get());
  ++it;
  EXPECT_TRUE(*it == now_bin.get());
  ++it;
  EXPECT_TRUE(*it == soon_bin.get());
  ++it;
  EXPECT_TRUE(*it == eventually_bin.get());
  ++it;
  EXPECT_TRUE(*it == never_bin.get());
  ++it;
  EXPECT_FALSE(it);

  // Creating multiple iterators shouldn't affect old iterators.
  PrioritizedTileSet::PriorityIterator second_it(&set);
  EXPECT_TRUE(second_it);
  EXPECT_FALSE(it);

  ++second_it;
  EXPECT_TRUE(second_it);
  ++second_it;
  EXPECT_TRUE(second_it);
  EXPECT_FALSE(it);

  PrioritizedTileSet::PriorityIterator third_it(&set);
  EXPECT_TRUE(third_it);
  ++second_it;
  ++second_it;
  EXPECT_TRUE(second_it);
  EXPECT_TRUE(third_it);
  EXPECT_FALSE(it);

  ++third_it;
  ++third_it;
  EXPECT_TRUE(third_it);
  EXPECT_TRUE(*third_it == soon_bin.get());
  EXPECT_TRUE(second_it);
  EXPECT_TRUE(*second_it == never_bin.get());
  EXPECT_FALSE(it);

  ++second_it;
  EXPECT_TRUE(third_it);
  EXPECT_FALSE(second_it);
  EXPECT_FALSE(it);

  set.Clear();

  PrioritizedTileSet::PriorityIterator empty_it(&set);
  EXPECT_FALSE(empty_it);
}

}  // namespace
}  // namespace cc

