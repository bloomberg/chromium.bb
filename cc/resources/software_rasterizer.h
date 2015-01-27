// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_SOFTWARE_RASTERIZER_H_
#define CC_RESOURCES_SOFTWARE_RASTERIZER_H_

#include <vector>

#include "cc/base/cc_export.h"
#include "cc/resources/rasterizer.h"

namespace cc {

// This class only returns |PrepareTilesMode::RASTERIZE_PRIORITIZED_TILES| in
// |GetPrepareTilesMode()| to tell rasterize as scheduled tasks.
class CC_EXPORT SoftwareRasterizer : public Rasterizer {
 public:
  ~SoftwareRasterizer() override;

  static scoped_ptr<SoftwareRasterizer> Create();

  PrepareTilesMode GetPrepareTilesMode() override;
  void RasterizeTiles(
      const TileVector& tiles,
      ResourcePool* resource_pool,
      ResourceFormat resource_format,
      const UpdateTileDrawInfoCallback& update_tile_draw_info) override;

 private:
  SoftwareRasterizer();

  DISALLOW_COPY_AND_ASSIGN(SoftwareRasterizer);
};

}  // namespace cc

#endif  // CC_RESOURCES_SOFTWARE_RASTERIZER_H_
