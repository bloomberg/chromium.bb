// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_COMPOSITOR_FRAME_SINK_H_
#define CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_COMPOSITOR_FRAME_SINK_H_

#include "cc/ipc/compositor_frame.mojom.h"
#include "cc/ipc/mojo_compositor_frame_sink.mojom.h"
#include "cc/resources/transferable_resource.h"
#include "cc/surfaces/compositor_frame_sink_support.h"
#include "cc/surfaces/compositor_frame_sink_support_client.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

class OffscreenCanvasCompositorFrameSinkProviderImpl;

class OffscreenCanvasCompositorFrameSink
    : public cc::CompositorFrameSinkSupportClient,
      public cc::mojom::MojoCompositorFrameSink {
 public:
  OffscreenCanvasCompositorFrameSink(
      OffscreenCanvasCompositorFrameSinkProviderImpl* provider,
      const cc::FrameSinkId& frame_sink_id,
      cc::mojom::MojoCompositorFrameSinkRequest request,
      cc::mojom::MojoCompositorFrameSinkClientPtr client);

  ~OffscreenCanvasCompositorFrameSink() override;

  // Overridden from cc::mojom::MojoCompositorFrameSink:
  void SetNeedsBeginFrame(bool needs_begin_frame) override;
  void SubmitCompositorFrame(const cc::LocalSurfaceId& local_surface_id,
                             cc::CompositorFrame frame) override;
  void EvictFrame() override;

  // Overridden from cc::CompositorFrameSinkSupportClient:
  void DidReceiveCompositorFrameAck() override;
  void OnBeginFrame(const cc::BeginFrameArgs& args) override;
  void ReclaimResources(const cc::ReturnedResourceArray& resources) override;
  void WillDrawSurface(const cc::LocalSurfaceId& local_surface_id,
                       const gfx::Rect& damage_rect) override;

 private:
  void OnClientConnectionLost();

  OffscreenCanvasCompositorFrameSinkProviderImpl* const provider_;

  cc::CompositorFrameSinkSupport support_;
  cc::mojom::MojoCompositorFrameSinkClientPtr client_;
  cc::ReturnedResourceArray surface_returned_resources_;
  mojo::Binding<cc::mojom::MojoCompositorFrameSink> binding_;

  DISALLOW_COPY_AND_ASSIGN(OffscreenCanvasCompositorFrameSink);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_COMPOSITOR_FRAME_SINK_H_
