// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TILE_BUNDLE_H_
#define CC_RESOURCES_TILE_BUNDLE_H_

#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "cc/base/ref_counted_managed.h"
#include "cc/resources/tile_priority.h"

namespace cc {

class TileManager;
class Tile;
class PictureLayerTiling;

class CC_EXPORT TileBundle : public RefCountedManaged<TileBundle> {
 public:
  typedef uint64 Id;
  typedef std::vector<scoped_refptr<Tile> > TileVector;
  typedef std::vector<TileVector> TileGrid;

  inline Id id() const { return id_; }

  Tile* TileAt(WhichTree tree, int index_x, int index_y);
  bool RemoveTileAt(WhichTree tree, int index_x, int index_y);
  void AddTileAt(WhichTree tree,
                 int index_x,
                 int index_y,
                 scoped_refptr<Tile> tile);
  bool IsEmpty() const { return conservative_tile_count_ == 0; }

  void DidBecomeRecycled();
  void DidBecomeActive();

  void SetPriority(WhichTree tree, const TilePriority& priority);
  TilePriority GetPriority(WhichTree tree) const;

  void SwapTilesIfRequired();

  class CC_EXPORT Iterator {
   public:
    explicit Iterator(TileBundle* bundle);
    Iterator(TileBundle* bundle, WhichTree tree);
    ~Iterator();

    Iterator& operator++();

    inline Tile* operator*() { return current_tile_; }
    inline Tile* operator->() { return current_tile_; }
    inline operator bool() const {
      return index_y_ < bundle_->index_count_y_;
    }

    inline int index_x() {
      return index_x_ + bundle_->index_offset_x_;
    }
    inline int index_y() {
      return index_y_ + bundle_->index_offset_y_;
    }

    TilePriority priority(WhichTree tree) const {
      return bundle_->GetPriority(tree);
    }

   private:
    bool InitializeNewTile();

    TileBundle* bundle_;
    Tile* current_tile_;
    int index_x_;
    int index_y_;
    WhichTree current_tree_;

    WhichTree min_tree_;
    WhichTree max_tree_;
  };

 private:
  friend class TileManager;
  friend class Iterator;

  TileBundle(TileManager* tile_manager,
             int offset_x,
             int offset_y,
             int width,
             int height);
  ~TileBundle();

  void UpdateToLocalIndex(int* index_x, int* index_y);
  Tile* TileAtLocalIndex(WhichTree tree, int x, int y);

  TileManager* tile_manager_;

  TileGrid tiles_[NUM_TREES];
  int index_offset_x_;
  int index_offset_y_;
  int index_count_x_;
  int index_count_y_;

  TilePriority priority_[NUM_TREES];
  int conservative_tile_count_;
  bool needs_tile_swap_;

  Id id_;
  static Id s_next_id_;
};

}  // namespace cc

#endif  // CC_RESOURCES_TILE_BUNDLE_H_
