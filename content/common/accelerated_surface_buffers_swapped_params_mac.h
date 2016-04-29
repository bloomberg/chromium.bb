// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ACCELERATED_SURFACE_BUFFERS_SWAPPED_PARAMS_MAC_H_
#define CONTENT_COMMON_ACCELERATED_SURFACE_BUFFERS_SWAPPED_PARAMS_MAC_H_

#include "gpu/ipc/common/surface_handle.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/mac/io_surface.h"

namespace content {

struct AcceleratedSurfaceBuffersSwappedParams {
  AcceleratedSurfaceBuffersSwappedParams();
  ~AcceleratedSurfaceBuffersSwappedParams();

  gpu::SurfaceHandle surface_handle;
  CAContextID ca_context_id;
  bool fullscreen_low_power_ca_context_valid;
  CAContextID fullscreen_low_power_ca_context_id;
  gfx::ScopedRefCountedIOSurfaceMachPort io_surface;
  gfx::Size size;
  float scale_factor;
  std::vector<ui::LatencyInfo> latency_info;
};

}  // namespace content

#endif  // CONTENT_COMMON_ACCELERATED_SURFACE_BUFFERS_SWAPPED_PARAMS_MAC_H_
