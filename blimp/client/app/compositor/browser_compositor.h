// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_APP_COMPOSITOR_BROWSER_COMPOSITOR_H_
#define BLIMP_CLIENT_APP_COMPOSITOR_BROWSER_COMPOSITOR_H_

#include "base/macros.h"
#include "base/callback.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

namespace cc {
class Display;
class Layer;
class LayerTreeHost;
class SurfaceIdAllocator;
class SurfaceManager;
}  // namespace cc

namespace blimp {
namespace client {
class BlimpGpuMemoryBufferManager;

// The parent compositor that embeds the content from the BlimpCompositor for
// the current page.
class BrowserCompositor : public cc::LayerTreeHostClient,
                          public cc::LayerTreeHostSingleThreadClient {
 public:
  // TODO(dtrainor): Move these to the CompositorDeps and share them with the
  // BlimpCompositor.
  static cc::SurfaceManager* GetSurfaceManager();
  static BlimpGpuMemoryBufferManager* GetGpuMemoryBufferManager();
  static uint32_t AllocateSurfaceClientId();

  BrowserCompositor();
  ~BrowserCompositor() override;

  // Sets the layer with the content from the renderer compositor.
  void SetContentLayer(scoped_refptr<cc::Layer> content_layer);

  // Sets the size for the display. Should be in physical pixels.
  void SetSize(const gfx::Size& size_in_px);

  // Sets the widget that the |cc::Display| draws to. On proving it the widget,
  // the compositor will become visible and start drawing to the widget. When
  // the widget goes away, we become invisible and drop all resources being
  // used to draw to the screen.
  void SetAcceleratedWidget(gfx::AcceleratedWidget widget);

  // A callback to get notifed when the compositor performs a successful swap.
  void set_did_complete_swap_buffers_callback(base::Closure callback) {
    did_complete_swap_buffers_ = callback;
  }

 private:
  // LayerTreeHostClient implementation.
  void WillBeginMainFrame() override {}
  void DidBeginMainFrame() override {}
  void BeginMainFrame(const cc::BeginFrameArgs& args) override {}
  void BeginMainFrameNotExpectedSoon() override {}
  void UpdateLayerTreeHost() override {}
  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float page_scale,
                           float top_controls_delta) override {}
  void RequestNewOutputSurface() override;
  void DidInitializeOutputSurface() override;
  void DidFailToInitializeOutputSurface() override;
  void WillCommit() override {}
  void DidCommit() override {}
  void DidCommitAndDrawFrame() override {}
  void DidCompleteSwapBuffers() override;
  void DidCompletePageScaleAnimation() override {}

  // LayerTreeHostSingleThreadClient implementation.
  void DidPostSwapBuffers() override {}
  void DidAbortSwapBuffers() override {}

  void HandlePendingOutputSurfaceRequest();

  std::unique_ptr<cc::SurfaceIdAllocator> surface_id_allocator_;
  gfx::AcceleratedWidget widget_;
  bool output_surface_request_pending_;
  std::unique_ptr<cc::Display> display_;

  gfx::Size viewport_size_in_px_;

  std::unique_ptr<cc::LayerTreeHost> host_;
  scoped_refptr<cc::Layer> root_layer_;

  base::Closure did_complete_swap_buffers_;

  DISALLOW_COPY_AND_ASSIGN(BrowserCompositor);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_APP_COMPOSITOR_BROWSER_COMPOSITOR_H_
