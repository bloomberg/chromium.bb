// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tile_bundle.h"

#include <set>

#include "cc/test/fake_picture_pile_impl.h"
#include "cc/test/fake_tile_manager.h"
#include "cc/test/fake_tile_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(TileBundle, AddRemoveTile) {
  FakeTileManagerClient tile_manager_client;
  FakeTileManager tile_manager(&tile_manager_client);
  scoped_refptr<PicturePileImpl> picture_pile =
      FakePicturePileImpl::CreatePile();

  scoped_refptr<Tile> tile = tile_manager.CreateTile(picture_pile.get(),
                                                     gfx::Size(256, 256),
                                                     gfx::Rect(),
                                                     gfx::Rect(),
                                                     1.0,
                                                     0,
                                                     0,
                                                     true);
  scoped_refptr<TileBundle> bundle = tile_manager.CreateTileBundle(0, 0, 2, 2);

  EXPECT_FALSE(bundle->TileAt(ACTIVE_TREE, 0, 0));
  EXPECT_FALSE(bundle->TileAt(ACTIVE_TREE, 1, 0));
  EXPECT_FALSE(bundle->TileAt(ACTIVE_TREE, 0, 1));
  EXPECT_FALSE(bundle->TileAt(ACTIVE_TREE, 1, 1));
  EXPECT_TRUE(bundle->IsEmpty());

  bundle->AddTileAt(ACTIVE_TREE, 0, 1, tile);
  EXPECT_FALSE(bundle->TileAt(ACTIVE_TREE, 0, 0));
  EXPECT_FALSE(bundle->TileAt(ACTIVE_TREE, 1, 0));
  EXPECT_TRUE(bundle->TileAt(ACTIVE_TREE, 0, 1));
  EXPECT_FALSE(bundle->TileAt(ACTIVE_TREE, 1, 1));
  EXPECT_FALSE(bundle->IsEmpty());

  bundle->AddTileAt(ACTIVE_TREE, 1, 1, tile);
  EXPECT_FALSE(bundle->TileAt(ACTIVE_TREE, 0, 0));
  EXPECT_FALSE(bundle->TileAt(ACTIVE_TREE, 1, 0));
  EXPECT_TRUE(bundle->TileAt(ACTIVE_TREE, 0, 1));
  EXPECT_TRUE(bundle->TileAt(ACTIVE_TREE, 1, 1));
  EXPECT_FALSE(bundle->IsEmpty());

  bundle->RemoveTileAt(ACTIVE_TREE, 0, 1);
  EXPECT_FALSE(bundle->TileAt(ACTIVE_TREE, 0, 0));
  EXPECT_FALSE(bundle->TileAt(ACTIVE_TREE, 1, 0));
  EXPECT_FALSE(bundle->TileAt(ACTIVE_TREE, 0, 1));
  EXPECT_TRUE(bundle->TileAt(ACTIVE_TREE, 1, 1));
  EXPECT_FALSE(bundle->IsEmpty());

  bundle->RemoveTileAt(ACTIVE_TREE, 0, 1);
  EXPECT_FALSE(bundle->TileAt(ACTIVE_TREE, 0, 0));
  EXPECT_FALSE(bundle->TileAt(ACTIVE_TREE, 1, 0));
  EXPECT_FALSE(bundle->TileAt(ACTIVE_TREE, 0, 1));
  EXPECT_TRUE(bundle->TileAt(ACTIVE_TREE, 1, 1));
  EXPECT_FALSE(bundle->IsEmpty());

  bundle->RemoveTileAt(ACTIVE_TREE, 1, 1);
  EXPECT_FALSE(bundle->TileAt(ACTIVE_TREE, 0, 0));
  EXPECT_FALSE(bundle->TileAt(ACTIVE_TREE, 1, 0));
  EXPECT_FALSE(bundle->TileAt(ACTIVE_TREE, 0, 1));
  EXPECT_FALSE(bundle->TileAt(ACTIVE_TREE, 1, 1));
  EXPECT_TRUE(bundle->IsEmpty());
}

TEST(TileBundle, DidBecomeActiveSwapsTiles) {
  FakeTileManagerClient tile_manager_client;
  FakeTileManager tile_manager(&tile_manager_client);
  scoped_refptr<PicturePileImpl> picture_pile =
      FakePicturePileImpl::CreatePile();

  scoped_refptr<Tile> tile = tile_manager.CreateTile(picture_pile.get(),
                                                     gfx::Size(256, 256),
                                                     gfx::Rect(),
                                                     gfx::Rect(),
                                                     1.0,
                                                     0,
                                                     0,
                                                     true);
  scoped_refptr<Tile> other_tile = tile_manager.CreateTile(picture_pile.get(),
                                                           gfx::Size(256, 256),
                                                           gfx::Rect(),
                                                           gfx::Rect(),
                                                           1.0,
                                                           0,
                                                           0,
                                                           true);

  scoped_refptr<TileBundle> bundle = tile_manager.CreateTileBundle(0, 0, 2, 2);
  bundle->AddTileAt(ACTIVE_TREE, 0, 0, tile);
  bundle->AddTileAt(PENDING_TREE, 1, 1, other_tile);

  EXPECT_EQ(bundle->TileAt(ACTIVE_TREE, 0, 0), tile.get());
  EXPECT_EQ(bundle->TileAt(PENDING_TREE, 1, 1), other_tile.get());

  bundle->DidBecomeActive();

  EXPECT_EQ(bundle->TileAt(PENDING_TREE, 0, 0), tile.get());
  EXPECT_FALSE(bundle->TileAt(ACTIVE_TREE, 0, 0));
  EXPECT_EQ(bundle->TileAt(ACTIVE_TREE, 1, 1), other_tile.get());
  EXPECT_FALSE(bundle->TileAt(PENDING_TREE, 1, 1));

  bundle->SwapTilesIfRequired();

  EXPECT_EQ(bundle->TileAt(PENDING_TREE, 0, 0), tile.get());
  EXPECT_FALSE(bundle->TileAt(ACTIVE_TREE, 0, 0));
  EXPECT_EQ(bundle->TileAt(ACTIVE_TREE, 1, 1), other_tile.get());
  EXPECT_FALSE(bundle->TileAt(PENDING_TREE, 1, 1));
}

TEST(TileBundle, DidBecomeRecycledMarksTilesForSwap) {
  FakeTileManagerClient tile_manager_client;
  FakeTileManager tile_manager(&tile_manager_client);
  scoped_refptr<PicturePileImpl> picture_pile =
      FakePicturePileImpl::CreatePile();

  scoped_refptr<Tile> tile = tile_manager.CreateTile(picture_pile.get(),
                                                     gfx::Size(256, 256),
                                                     gfx::Rect(),
                                                     gfx::Rect(),
                                                     1.0,
                                                     0,
                                                     0,
                                                     true);
  scoped_refptr<Tile> other_tile = tile_manager.CreateTile(picture_pile.get(),
                                                           gfx::Size(256, 256),
                                                           gfx::Rect(),
                                                           gfx::Rect(),
                                                           1.0,
                                                           0,
                                                           0,
                                                           true);

  scoped_refptr<TileBundle> bundle = tile_manager.CreateTileBundle(0, 0, 2, 2);
  bundle->AddTileAt(ACTIVE_TREE, 0, 0, tile);
  bundle->AddTileAt(PENDING_TREE, 1, 1, other_tile);

  EXPECT_EQ(bundle->TileAt(ACTIVE_TREE, 0, 0), tile.get());
  EXPECT_EQ(bundle->TileAt(PENDING_TREE, 1, 1), other_tile.get());

  bundle->DidBecomeRecycled();
  bundle->DidBecomeRecycled();

  EXPECT_EQ(bundle->TileAt(ACTIVE_TREE, 0, 0), tile.get());
  EXPECT_FALSE(bundle->TileAt(PENDING_TREE, 0, 0));
  EXPECT_EQ(bundle->TileAt(PENDING_TREE, 1, 1), other_tile.get());
  EXPECT_FALSE(bundle->TileAt(ACTIVE_TREE, 1, 1));

  bundle->DidBecomeRecycled();
  bundle->SwapTilesIfRequired();

  EXPECT_EQ(bundle->TileAt(PENDING_TREE, 0, 0), tile.get());
  EXPECT_FALSE(bundle->TileAt(ACTIVE_TREE, 0, 0));
  EXPECT_EQ(bundle->TileAt(ACTIVE_TREE, 1, 1), other_tile.get());
  EXPECT_FALSE(bundle->TileAt(PENDING_TREE, 1, 1));
}

TEST(TileBundle, EmptyIterator) {
  FakeTileManagerClient tile_manager_client;
  FakeTileManager tile_manager(&tile_manager_client);
  scoped_refptr<TileBundle> bundle = tile_manager.CreateTileBundle(0, 0, 2, 2);

  TileBundle::Iterator it(bundle.get());
  EXPECT_FALSE(it);

  TileBundle::Iterator active_it(bundle.get(), ACTIVE_TREE);
  EXPECT_FALSE(active_it);

  TileBundle::Iterator pending_it(bundle.get(), PENDING_TREE);
  EXPECT_FALSE(pending_it);
}

TEST(TileBundle, NonEmptyIterator) {
  FakeTileManagerClient tile_manager_client;
  FakeTileManager tile_manager(&tile_manager_client);
  scoped_refptr<PicturePileImpl> picture_pile =
      FakePicturePileImpl::CreatePile();
  scoped_refptr<TileBundle> bundle = tile_manager.CreateTileBundle(0, 0, 2, 2);

  scoped_refptr<Tile> tile = tile_manager.CreateTile(picture_pile.get(),
                                                     gfx::Size(256, 256),
                                                     gfx::Rect(),
                                                     gfx::Rect(),
                                                     1.0,
                                                     0,
                                                     0,
                                                     true);
  scoped_refptr<Tile> other_tile = tile_manager.CreateTile(picture_pile.get(),
                                                           gfx::Size(256, 256),
                                                           gfx::Rect(),
                                                           gfx::Rect(),
                                                           1.0,
                                                           0,
                                                           0,
                                                           true);
  scoped_refptr<Tile> third_tile = tile_manager.CreateTile(picture_pile.get(),
                                                           gfx::Size(256, 256),
                                                           gfx::Rect(),
                                                           gfx::Rect(),
                                                           1.0,
                                                           0,
                                                           0,
                                                           true);

  bundle->AddTileAt(ACTIVE_TREE, 0, 0, tile);
  bundle->AddTileAt(PENDING_TREE, 1, 1, other_tile);
  bundle->AddTileAt(ACTIVE_TREE, 1, 1, third_tile);

  TileBundle::Iterator it(bundle.get());
  EXPECT_TRUE(it);
  std::set<Tile*> tiles;
  size_t count = 0;
  while (it) {
    tiles.insert(*it);
    ++it;
    ++count;
  }

  EXPECT_EQ(count, tiles.size());
  EXPECT_TRUE(tiles.count(tile.get()));
  EXPECT_TRUE(tiles.count(other_tile.get()));
  EXPECT_TRUE(tiles.count(third_tile.get()));

  TileBundle::Iterator active_it(bundle.get(), ACTIVE_TREE);
  EXPECT_TRUE(active_it);
  count = 0;
  tiles.clear();
  while (active_it) {
    tiles.insert(*active_it);
    ++active_it;
    ++count;
  }

  EXPECT_EQ(count, tiles.size());
  EXPECT_TRUE(tiles.count(tile.get()));
  EXPECT_FALSE(tiles.count(other_tile.get()));
  EXPECT_TRUE(tiles.count(third_tile.get()));

  TileBundle::Iterator pending_it(bundle.get(), PENDING_TREE);
  EXPECT_TRUE(pending_it);
  EXPECT_EQ(*pending_it, other_tile.get());
  ++pending_it;
  EXPECT_FALSE(pending_it);
}


}  // namespace
}  // namespace cc
