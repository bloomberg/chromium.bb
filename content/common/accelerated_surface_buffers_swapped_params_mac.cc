// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/accelerated_surface_buffers_swapped_params_mac.h"

namespace content {
AcceleratedSurfaceBuffersSwappedParams::AcceleratedSurfaceBuffersSwappedParams()
    : surface_handle(gpu::kNullSurfaceHandle),
      ca_context_id(0),
      scale_factor(1.f) {}

AcceleratedSurfaceBuffersSwappedParams::
    ~AcceleratedSurfaceBuffersSwappedParams() {}

}  // namespace content
