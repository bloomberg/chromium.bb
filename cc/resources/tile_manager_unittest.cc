// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tile.h"
#include "cc/test/fake_tile_manager.h"
#include "cc/test/fake_tile_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class FakePicturePileImpl : public PicturePileImpl {
 public:
  FakePicturePileImpl() : PicturePileImpl(false) {
    gfx::Size size(std::numeric_limits<int>::max(),
                   std::numeric_limits<int>::max());
    Resize(size);
    recorded_region_ = Region(gfx::Rect(size));
  }

 protected:
  virtual ~FakePicturePileImpl() {}
};

class TilePriorityForEventualBin : public TilePriority {
 public:
    TilePriorityForEventualBin() : TilePriority(
            NON_IDEAL_RESOLUTION,
            1.0,
            315.0) { }
};

class TilePriorityForNowBin : public TilePriority {
 public:
    TilePriorityForNowBin() : TilePriority(
            HIGH_RESOLUTION,
            0,
            0) { }
};

TEST(TileManagerTest, OOM) {
    // Init TileManager
    FakeTileManagerClient client;
    LayerTreeSettings setting;
    FakeTileManager manager(&client);

    // Memory limit to supply 8 tiles of RGBA
    GlobalStateThatImpactsTilePriority state;
    gfx::Size tile_size = setting.default_tile_size;
    state.memory_limit_in_bytes =
        8 * 4 * tile_size.width() * tile_size.height();
    state.memory_limit_policy = ALLOW_ANYTHING;
    state.tree_priority = SMOOTHNESS_TAKES_PRIORITY;
    manager.SetGlobalState(state);

    // Create tiles
    TilePriorityForEventualBin eventual_prio;
    TilePriorityForNowBin now_prio;
    typedef std::vector<scoped_refptr<Tile> > TileVector;
    TileVector tiles_on_active_tree;
    TileVector tiles_on_pending_tree;
    scoped_refptr<FakePicturePileImpl> pile = new FakePicturePileImpl();
    // Register 5 tiles for active tree requiring eventual bin
    for (int i = 0; i < 5; ++i) {
        scoped_refptr<Tile> tile = make_scoped_refptr(
                    new Tile(&manager,
                        pile.get(),
                        tile_size,
                        gfx::Rect(),
                        gfx::Rect(),
                        1.0,
                        0));
        tile->SetPriority(PENDING_TREE, TilePriority());
        tile->SetPriority(ACTIVE_TREE, eventual_prio);
        tiles_on_active_tree.push_back(tile);
    }
    // Register 5 tiles for pending tree requiring now bin
    for (int i = 0; i < 5; ++i) {
        scoped_refptr<Tile> tile = make_scoped_refptr(
                    new Tile(&manager,
                        pile.get(),
                        setting.default_tile_size,
                        gfx::Rect(),
                        gfx::Rect(),
                        1.0,
                        0));
        tile->SetPriority(PENDING_TREE, now_prio);
        tile->SetPriority(ACTIVE_TREE, TilePriority());
        tiles_on_pending_tree.push_back(tile);
    }
    // Try to allocate memory to tiles in OOM situation
    manager.ManageTiles();

    // Check if pending tiles get memory
    for (TileVector::const_iterator it = tiles_on_pending_tree.begin();
         it != tiles_on_pending_tree.end() ; ++it) {
         EXPECT_TRUE((*it)->IsAssignedGpuMemory());
    }
}

}  // namespace
}  // namespace cc
