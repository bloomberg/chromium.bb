// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_PRIORITIZED_TILE_H_
#define CC_RESOURCES_PRIORITIZED_TILE_H_

#include "cc/base/cc_export.h"
#include "cc/resources/tile.h"
#include "cc/resources/tile_priority.h"

namespace cc {
class PictureLayerTiling;

class CC_EXPORT PrioritizedTile {
 public:
  // This class is constructed and returned by a |PictureLayerTiling|, and
  // represents a tile and its priority.
  PrioritizedTile();
  ~PrioritizedTile();

  Tile* tile() const { return tile_; }
  const TilePriority& priority() const { return priority_; }
  bool is_occluded() const { return is_occluded_; }

 private:
  friend class PictureLayerTiling;

  PrioritizedTile(Tile* tile, const TilePriority priority, bool is_occluded);

  Tile* tile_;
  TilePriority priority_;
  bool is_occluded_;
};

}  // namespace cc

#endif  // CC_RESOURCES_PRIORITIZED_TILE_H_
