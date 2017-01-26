// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DISPLAY_COMPOSITOR_GPU_OFFSCREEN_COMPOSITOR_FRAME_SINK_H_
#define COMPONENTS_DISPLAY_COMPOSITOR_GPU_OFFSCREEN_COMPOSITOR_FRAME_SINK_H_

#include "components/display_compositor/display_compositor_export.h"
#include "components/display_compositor/gpu_compositor_frame_sink.h"
#include "components/display_compositor/gpu_compositor_frame_sink_delegate.h"

namespace display_compositor {

class DISPLAY_COMPOSITOR_EXPORT GpuOffscreenCompositorFrameSink
    : public GpuCompositorFrameSink {
 public:
  GpuOffscreenCompositorFrameSink(
      GpuCompositorFrameSinkDelegate* delegate,
      cc::SurfaceManager* surface_manager,
      const cc::FrameSinkId& frame_sink_id,
      cc::mojom::MojoCompositorFrameSinkRequest request,
      cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request,
      cc::mojom::MojoCompositorFrameSinkClientPtr client);

  ~GpuOffscreenCompositorFrameSink() override;

 private:
  mojo::Binding<cc::mojom::MojoCompositorFrameSink> binding_;

  DISALLOW_COPY_AND_ASSIGN(GpuOffscreenCompositorFrameSink);
};

}  // namespace display_compositor

#endif  // COMPONENTS_DISPLAY_COMPOSITOR_GPU_OFFSCREEN_COMPOSITOR_FRAME_SINK_H_
