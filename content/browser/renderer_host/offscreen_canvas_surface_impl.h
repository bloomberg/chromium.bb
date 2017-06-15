// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_SURFACE_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_SURFACE_IMPL_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "cc/surfaces/frame_sink_id.h"
#include "cc/surfaces/surface_info.h"
#include "components/viz/host/frame_sink_manager_host.h"
#include "components/viz/host/frame_sink_observer.h"
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
      viz::FrameSinkManagerHost* frame_sink_manager_host,
      const cc::FrameSinkId& parent_frame_sink_id,
      const cc::FrameSinkId& frame_sink_id,
      blink::mojom::OffscreenCanvasSurfaceClientPtr client,
      blink::mojom::OffscreenCanvasSurfaceRequest request,
      DestroyCallback destroy_callback);
  ~OffscreenCanvasSurfaceImpl() override;

  const cc::FrameSinkId& frame_sink_id() const { return frame_sink_id_; }

  const cc::FrameSinkId& parent_frame_sink_id() const {
    return parent_frame_sink_id_;
  }

  const cc::LocalSurfaceId& local_surface_id() const {
    return local_surface_id_;
  }

  // Creates a MojoCompositorFrameSink connection to FrameSinkManager for an
  // offscreen canvas client. The corresponding private interface will be owned
  // here to control CompositorFrameSink lifetime. This should only ever be
  // called once.
  void CreateCompositorFrameSink(
      cc::mojom::MojoCompositorFrameSinkClientPtr client,
      cc::mojom::MojoCompositorFrameSinkRequest request);

  // FrameSinkObserver implementation.
  void OnSurfaceCreated(const cc::SurfaceInfo& surface_info) override;

  // blink::mojom::OffscreenCanvasSurface implementation.
  void Require(const cc::SurfaceId& surface_id,
               const cc::SurfaceSequence& sequence) override;
  void Satisfy(const cc::SurfaceSequence& sequence) override;

 private:
  // Registered as a callback for when |binding_| is closed. Will call
  // |destroy_callback_|.
  void OnSurfaceConnectionClosed();

  viz::FrameSinkManagerHost* const frame_sink_manager_host_;

  blink::mojom::OffscreenCanvasSurfaceClientPtr client_;
  mojo::Binding<blink::mojom::OffscreenCanvasSurface> binding_;

  // To be called if |binding_| is closed.
  DestroyCallback destroy_callback_;

  // Surface-related state
  const cc::FrameSinkId frame_sink_id_;
  cc::LocalSurfaceId local_surface_id_;
  const cc::FrameSinkId parent_frame_sink_id_;

  bool has_created_compositor_frame_sink_ = false;

  DISALLOW_COPY_AND_ASSIGN(OffscreenCanvasSurfaceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_SURFACE_IMPL_H_
