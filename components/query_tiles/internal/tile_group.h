// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_TILE_GROUP_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_TILE_GROUP_H_

#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/query_tiles/tile.h"

namespace query_tiles {

// A group of query tiles and metadata.
struct TileGroup {
  TileGroup();
  TileGroup(const TileGroup& other);
  TileGroup(TileGroup&& other);

  ~TileGroup();

  TileGroup& operator=(const TileGroup& other);
  TileGroup& operator=(TileGroup&& other);

  bool operator==(const TileGroup& other) const;
  bool operator!=(const TileGroup& other) const;

  // Unique id for the group.
  std::string id;

  // Locale setting of this group.
  std::string locale;

  // Last updated timestamp in milliseconds.
  base::Time last_updated_ts;

  // Top level tiles.
  std::vector<std::unique_ptr<Tile>> tiles;
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_TILE_GROUP_H_
