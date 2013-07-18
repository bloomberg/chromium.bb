// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/delegated_compositor_output_surface.h"

namespace content {

DelegatedCompositorOutputSurface::DelegatedCompositorOutputSurface(
    int32 routing_id,
    uint32 output_surface_id,
    WebGraphicsContext3DCommandBufferImpl* context3d,
    cc::SoftwareOutputDevice* software)
    : CompositorOutputSurface(routing_id,
                              output_surface_id,
                              context3d,
                              software,
                              true) {
  capabilities_.delegated_rendering = true;
  capabilities_.max_frames_pending = 1;
}

}  // namespace content
