// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_HITTEST_H_
#define CC_SURFACES_SURFACE_HITTEST_H_

#include <set>

#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surfaces_export.h"

namespace gfx {
class Point;
}

namespace cc {
class RenderPass;
class SurfaceManager;

// Performs a hittest in surface quads.
class CC_SURFACES_EXPORT SurfaceHittest {
 public:
  explicit SurfaceHittest(SurfaceManager* manager);
  ~SurfaceHittest();

  // Hittests against Surface with SurfaceId |surface_id|, return the contained
  // surface that the point is hitting and the |transformed_point| in the
  // surface space.
  SurfaceId Hittest(SurfaceId surface_id,
                    const gfx::Point& point,
                    gfx::Point* transformed_point);

 private:
  bool HittestInternal(SurfaceId surface_id,
                       const RenderPass* render_pass,
                       const gfx::Point& point,
                       SurfaceId* out_surface_id,
                       gfx::Point* out_transformed_point);

  SurfaceManager* const manager_;
  std::set<const RenderPass*> referenced_passes_;
};
}  // namespace cc

#endif  // CC_SURFACES_SURFACE_HITTEST_H_
