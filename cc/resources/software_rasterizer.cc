// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/software_rasterizer.h"

namespace cc {

scoped_ptr<SoftwareRasterizer> SoftwareRasterizer::Create() {
  return make_scoped_ptr<SoftwareRasterizer>(new SoftwareRasterizer());
}

SoftwareRasterizer::SoftwareRasterizer() {
}

SoftwareRasterizer::~SoftwareRasterizer() {
}

PrepareTilesMode SoftwareRasterizer::GetPrepareTilesMode() {
  return PrepareTilesMode::RASTERIZE_PRIORITIZED_TILES;
}

void SoftwareRasterizer::RasterizeTiles(
    const TileVector& tiles,
    ResourcePool* resource_pool,
    ResourceFormat resource_format,
    const UpdateTileDrawInfoCallback& update_tile_draw_info) {
}

}  // namespace cc
