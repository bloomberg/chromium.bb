// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_SURFACE_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_SURFACE_IMPL_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/WebKit/public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom.h"

namespace viz {
class HostFrameSinkManager;
}

namespace content {

// The browser owned object for an embedded surface in a renderer process. Both
// the embedder and embedded surface are in the same renderer. Holds a client
// connection to the renderer that is notified when a new SurfaceId activates
// for the embedded surface.
class CONTENT_EXPORT OffscreenCanvasSurfaceImpl
    : public viz::HostFrameSinkClient {
 public:
  using DestroyCallback = base::OnceCallback<void()>;

  OffscreenCanvasSurfaceImpl(
      viz::HostFrameSinkManager* host_frame_sink_manager,
      const viz::FrameSinkId& parent_frame_sink_id,
      const viz::FrameSinkId& frame_sink_id,
      blink::mojom::OffscreenCanvasSurfaceClientPtr client,
      DestroyCallback destroy_callback);
  ~OffscreenCanvasSurfaceImpl() override;

  const viz::FrameSinkId& frame_sink_id() const { return frame_sink_id_; }
  const viz::LocalSurfaceId& local_surface_id() const {
    return local_surface_id_;
  }

  // Creates a CompositorFrameSink connection to FrameSinkManagerImpl for an
  // offscreen canvas client. The corresponding private interface will be owned
  // here to control CompositorFrameSink lifetime. This should only ever be
  // called once.
  void CreateCompositorFrameSink(
      viz::mojom::CompositorFrameSinkClientPtr client,
      viz::mojom::CompositorFrameSinkRequest request);

  // viz::HostFrameSinkClient implementation.
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;
  void OnFrameTokenChanged(uint32_t frame_token) override;

 private:
  viz::HostFrameSinkManager* const host_frame_sink_manager_;

  blink::mojom::OffscreenCanvasSurfaceClientPtr client_;

  // Surface-related state
  const viz::FrameSinkId parent_frame_sink_id_;
  const viz::FrameSinkId frame_sink_id_;
  viz::LocalSurfaceId local_surface_id_;

  bool has_created_compositor_frame_sink_ = false;

  DISALLOW_COPY_AND_ASSIGN(OffscreenCanvasSurfaceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_SURFACE_IMPL_H_
