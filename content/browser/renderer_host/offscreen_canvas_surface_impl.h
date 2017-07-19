// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_SURFACE_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_SURFACE_IMPL_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/host/frame_sink_observer.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/WebKit/public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom.h"

namespace content {

// The browser owned object for an offscreen canvas connection. Holds
// connections to both the renderer and frame sink manager.
class CONTENT_EXPORT OffscreenCanvasSurfaceImpl
    : public blink::mojom::OffscreenCanvasSurface,
      public NON_EXPORTED_BASE(viz::FrameSinkObserver) {
 public:
  using DestroyCallback = base::OnceCallback<void()>;

  OffscreenCanvasSurfaceImpl(
      viz::HostFrameSinkManager* host_frame_sink_manager,
      const viz::FrameSinkId& parent_frame_sink_id,
      const viz::FrameSinkId& frame_sink_id,
      blink::mojom::OffscreenCanvasSurfaceClientPtr client,
      blink::mojom::OffscreenCanvasSurfaceRequest request,
      DestroyCallback destroy_callback);
  ~OffscreenCanvasSurfaceImpl() override;

  const viz::FrameSinkId& frame_sink_id() const { return frame_sink_id_; }

  const viz::FrameSinkId& parent_frame_sink_id() const {
    return parent_frame_sink_id_;
  }

  const viz::LocalSurfaceId& local_surface_id() const {
    return local_surface_id_;
  }

  // Creates a CompositorFrameSink connection to FrameSinkManagerImpl for an
  // offscreen canvas client. The corresponding private interface will be owned
  // here to control CompositorFrameSink lifetime. This should only ever be
  // called once.
  void CreateCompositorFrameSink(cc::mojom::CompositorFrameSinkClientPtr client,
                                 cc::mojom::CompositorFrameSinkRequest request);

  // FrameSinkObserver implementation.
  void OnSurfaceCreated(const viz::SurfaceInfo& surface_info) override;

  // blink::mojom::OffscreenCanvasSurface implementation.
  void Require(const viz::SurfaceId& surface_id,
               const viz::SurfaceSequence& sequence) override;
  void Satisfy(const viz::SurfaceSequence& sequence) override;

 private:
  // Registered as a callback for when |binding_| is closed. Will call
  // |destroy_callback_|.
  void OnSurfaceConnectionClosed();

  viz::HostFrameSinkManager* const host_frame_sink_manager_;

  blink::mojom::OffscreenCanvasSurfaceClientPtr client_;
  mojo::Binding<blink::mojom::OffscreenCanvasSurface> binding_;

  // To be called if |binding_| is closed.
  DestroyCallback destroy_callback_;

  // Surface-related state
  const viz::FrameSinkId frame_sink_id_;
  viz::LocalSurfaceId local_surface_id_;
  const viz::FrameSinkId parent_frame_sink_id_;

  bool has_created_compositor_frame_sink_ = false;

  DISALLOW_COPY_AND_ASSIGN(OffscreenCanvasSurfaceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_SURFACE_IMPL_H_
