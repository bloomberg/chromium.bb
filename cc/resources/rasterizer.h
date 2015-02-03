// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RASTERIZER_H_
#define CC_RESOURCES_RASTERIZER_H_

#include <vector>

#include "cc/base/cc_export.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/tile.h"

namespace cc {
class TileManager;

enum class PrepareTilesMode {
  RASTERIZE_PRIORITIZED_TILES,
  PREPARE_NONE
};

class CC_EXPORT Rasterizer {
 public:
  using TileVector = std::vector<Tile*>;
  using UpdateTileDrawInfoCallback =
      base::Callback<void(Tile*,
                          scoped_ptr<ScopedResource>,
                          const RasterSource::SolidColorAnalysis&)>;

  virtual ~Rasterizer() {}
  virtual PrepareTilesMode GetPrepareTilesMode() = 0;
  virtual void RasterizeTiles(
      const TileVector& tiles,
      ResourcePool* resource_pool,
      ResourceFormat resource_format,
      const UpdateTileDrawInfoCallback& update_tile_draw_info) = 0;

 protected:
  Rasterizer() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Rasterizer);
};

}  // namespace cc

#endif  // CC_RESOURCES_RASTERIZER_H_
