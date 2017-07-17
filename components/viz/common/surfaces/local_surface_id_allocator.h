// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_SURFACES_LOCAL_SURFACE_ID_ALLOCATOR_H_
#define COMPONENTS_VIZ_COMMON_SURFACES_LOCAL_SURFACE_ID_ALLOCATOR_H_

#include <stdint.h>

#include "base/macros.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {

// This is a helper class for generating local surface IDs for a single
// FrameSink. This is not threadsafe, to use from multiple threads wrap this
// class in a mutex.
class VIZ_COMMON_EXPORT LocalSurfaceIdAllocator {
 public:
  LocalSurfaceIdAllocator();
  ~LocalSurfaceIdAllocator();

  LocalSurfaceId GenerateId();

 private:
  uint32_t next_id_;

  DISALLOW_COPY_AND_ASSIGN(LocalSurfaceIdAllocator);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_SURFACES_LOCAL_SURFACE_ID_ALLOCATOR_H_
