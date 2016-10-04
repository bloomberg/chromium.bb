// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_ID_ALLOCATOR_H_
#define CC_SURFACES_SURFACE_ID_ALLOCATOR_H_

#include <stdint.h>

#include "base/macros.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surfaces_export.h"

namespace cc {

// This is a helper class for generating surface IDs within a specified
// namespace.  This is not threadsafe, to use from multiple threads wrap this
// class in a mutex.
class CC_SURFACES_EXPORT SurfaceIdAllocator {
 public:
  explicit SurfaceIdAllocator(const FrameSinkId& frame_sink_id);
  ~SurfaceIdAllocator();

  SurfaceId GenerateId();

 private:
  const FrameSinkId frame_sink_id_;

  uint32_t next_id_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceIdAllocator);
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_ID_ALLOCATOR_H_
