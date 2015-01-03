// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_GPU_RASTERIZER_H_
#define CC_RESOURCES_GPU_RASTERIZER_H_

#include <vector>

#include "cc/base/cc_export.h"
#include "cc/resources/rasterizer.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/tile.h"
#include "third_party/skia/include/core/SkMultiPictureDraw.h"

namespace cc {

class ContextProvider;
class ResourceProvider;

class CC_EXPORT GpuRasterizer : public Rasterizer {
 public:
  ~GpuRasterizer() override;

  static scoped_ptr<GpuRasterizer> Create(ContextProvider* context_provider,
                                          ResourceProvider* resource_provider,
                                          bool use_distance_field_text,
                                          bool tile_prepare_enabled);
  PrepareTilesMode GetPrepareTilesMode() override;
  void RasterizeTiles(
      const TileVector& tiles,
      ResourcePool* resource_pool,
      const UpdateTileDrawInfoCallback& update_tile_draw_info) override;

 private:
  GpuRasterizer(ContextProvider* context_provider,
                ResourceProvider* resource_provider,
                bool use_distance_filed_text,
                bool tile_prepare_enabled);

  using ScopedResourceWriteLocks =
      ScopedPtrVector<ResourceProvider::ScopedWriteLockGr>;

  void PerformSolidColorAnalysis(const Tile* tile,
                                 RasterSource::SolidColorAnalysis* analysis);
  void AddToMultiPictureDraw(const Tile* tile,
                             const ScopedResource* resource,
                             ScopedResourceWriteLocks* locks);

  ContextProvider* context_provider_;
  ResourceProvider* resource_provider_;
  SkMultiPictureDraw multi_picture_draw_;

  bool use_distance_field_text_;
  bool tile_prepare_enabled_;

  DISALLOW_COPY_AND_ASSIGN(GpuRasterizer);
};

}  // namespace cc

#endif  // CC_RESOURCES_GPU_RASTERIZER_H_
