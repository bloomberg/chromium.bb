// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DISPLAY_COMPOSITOR_GPU_DISPLAY_COMPOSITOR_FRAME_SINK_H_
#define COMPONENTS_DISPLAY_COMPOSITOR_GPU_DISPLAY_COMPOSITOR_FRAME_SINK_H_

#include "components/display_compositor/gpu_compositor_frame_sink.h"
#include "components/display_compositor/gpu_compositor_frame_sink_delegate.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

namespace display_compositor {

class DISPLAY_COMPOSITOR_EXPORT GpuDisplayCompositorFrameSink
    : public GpuCompositorFrameSink,
      public NON_EXPORTED_BASE(cc::mojom::DisplayPrivate) {
 public:
  GpuDisplayCompositorFrameSink(
      GpuCompositorFrameSinkDelegate* delegate,
      cc::SurfaceManager* surface_manager,
      const cc::FrameSinkId& frame_sink_id,
      std::unique_ptr<cc::Display> display,
      std::unique_ptr<cc::BeginFrameSource> begin_frame_source,
      cc::mojom::MojoCompositorFrameSinkAssociatedRequest request,
      cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request,
      cc::mojom::MojoCompositorFrameSinkClientPtr client,
      cc::mojom::DisplayPrivateAssociatedRequest display_private_request);

  ~GpuDisplayCompositorFrameSink() override;

  // cc::mojom::DisplayPrivate:
  void SetDisplayVisible(bool visible) override;
  void ResizeDisplay(const gfx::Size& size) override;
  void SetDisplayColorSpace(const gfx::ColorSpace& color_space) override;
  void SetOutputIsSecure(bool secure) override;

 private:
  mojo::AssociatedBinding<cc::mojom::MojoCompositorFrameSink> binding_;
  mojo::AssociatedBinding<cc::mojom::DisplayPrivate> display_private_binding_;

  DISALLOW_COPY_AND_ASSIGN(GpuDisplayCompositorFrameSink);
};

}  // namespace display_compositor

#endif  // COMPONENTS_DISPLAY_COMPOSITOR_GPU_DISPLAY_COMPOSITOR_FRAME_SINK_H_
