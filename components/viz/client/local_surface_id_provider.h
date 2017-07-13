// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_CLIENT_LOCAL_SURFACE_ID_PROVIDER_H_
#define COMPONENTS_VIZ_CLIENT_LOCAL_SURFACE_ID_PROVIDER_H_

#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/local_surface_id_allocator.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class CompositorFrame;
}

namespace viz {

class LocalSurfaceIdProvider {
 public:
  LocalSurfaceIdProvider();
  virtual ~LocalSurfaceIdProvider();

  virtual const LocalSurfaceId& GetLocalSurfaceIdForFrame(
      const cc::CompositorFrame& frame) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalSurfaceIdProvider);
};

class DefaultLocalSurfaceIdProvider : public LocalSurfaceIdProvider {
 public:
  DefaultLocalSurfaceIdProvider();

  const LocalSurfaceId& GetLocalSurfaceIdForFrame(
      const cc::CompositorFrame& frame) override;

 private:
  LocalSurfaceId local_surface_id_;
  gfx::Size surface_size_;
  float device_scale_factor_ = 0;
  LocalSurfaceIdAllocator local_surface_id_allocator_;

  DISALLOW_COPY_AND_ASSIGN(DefaultLocalSurfaceIdProvider);
};

}  //  namespace viz

#endif  // COMPONENTS_VIZ_CLIENT_LOCAL_SURFACE_ID_PROVIDER_H_
