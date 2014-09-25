// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_HTML_VIEWER_WEBLAYERTREEVIEW_IMPL_H_
#define MOJO_SERVICES_HTML_VIEWER_WEBLAYERTREEVIEW_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "cc/trees/layer_tree_host_client.h"
#include "mojo/cc/output_surface_mojo.h"
#include "mojo/services/public/interfaces/gpu/gpu.mojom.h"
#include "mojo/services/public/interfaces/surfaces/surfaces_service.mojom.h"
#include "third_party/WebKit/public/platform/WebLayerTreeView.h"

namespace base {
class MessageLoopProxy;
}

namespace blink {
class WebWidget;
}

namespace cc {
class LayerTreeHost;
}

namespace mojo {
class View;

class WebLayerTreeViewImpl : public blink::WebLayerTreeView,
                             public cc::LayerTreeHostClient,
                             public OutputSurfaceMojoClient {
 public:
  WebLayerTreeViewImpl(
      scoped_refptr<base::MessageLoopProxy> compositor_message_loop_proxy,
      SurfacesServicePtr surfaces_service,
      GpuPtr gpu_service);
  virtual ~WebLayerTreeViewImpl();

  void set_widget(blink::WebWidget* widget) { widget_ = widget; }
  void set_view(View* view) { view_ = view; }

  // cc::LayerTreeHostClient implementation.
  virtual void WillBeginMainFrame(int frame_id) OVERRIDE;
  virtual void DidBeginMainFrame() OVERRIDE;
  virtual void BeginMainFrame(const cc::BeginFrameArgs& args) OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void ApplyViewportDeltas(const gfx::Vector2d& scroll_delta,
                                   float page_scale,
                                   float top_controls_delta) OVERRIDE;
  virtual void RequestNewOutputSurface(bool fallback) OVERRIDE;
  virtual void DidInitializeOutputSurface() OVERRIDE;
  virtual void WillCommit() OVERRIDE;
  virtual void DidCommit() OVERRIDE;
  virtual void DidCommitAndDrawFrame() OVERRIDE;
  virtual void DidCompleteSwapBuffers() OVERRIDE;
  virtual void RateLimitSharedMainThreadContext() OVERRIDE {}

  // blink::WebLayerTreeView implementation.
  virtual void setSurfaceReady() OVERRIDE;
  virtual void setRootLayer(const blink::WebLayer& layer) OVERRIDE;
  virtual void clearRootLayer() OVERRIDE;
  virtual void setViewportSize(
      const blink::WebSize& device_viewport_size) OVERRIDE;
  virtual blink::WebSize deviceViewportSize() const OVERRIDE;
  virtual void setDeviceScaleFactor(float) OVERRIDE;
  virtual float deviceScaleFactor() const OVERRIDE;
  virtual void setBackgroundColor(blink::WebColor color) OVERRIDE;
  virtual void setHasTransparentBackground(
      bool has_transparent_background) OVERRIDE;
  virtual void setOverhangBitmap(const SkBitmap& bitmap) OVERRIDE;
  virtual void setVisible(bool visible) OVERRIDE;
  virtual void setPageScaleFactorAndLimits(float page_scale_factor,
                                           float minimum,
                                           float maximum) OVERRIDE;
  virtual void startPageScaleAnimation(const blink::WebPoint& destination,
                                       bool use_anchor,
                                       float new_page_scale,
                                       double duration_sec) OVERRIDE;
  virtual void heuristicsForGpuRasterizationUpdated(bool matches_heuristic) {}
  virtual void setTopControlsContentOffset(float offset) {}
  virtual void setNeedsAnimate() OVERRIDE;
  virtual bool commitRequested() const OVERRIDE;
  virtual void didStopFlinging() {}
  virtual void compositeAndReadbackAsync(
      blink::WebCompositeAndReadbackAsyncCallback* callback) {}
  virtual void finishAllRendering() OVERRIDE;
  virtual void setDeferCommits(bool defer_commits) {}
  virtual void registerForAnimations(blink::WebLayer* layer) OVERRIDE;
  virtual void registerViewportLayers(
      const blink::WebLayer* page_scale_layer,
      const blink::WebLayer* inner_viewport_scroll_layer,
      const blink::WebLayer* outer_viewport_scroll_layer) OVERRIDE;
  virtual void clearViewportLayers() OVERRIDE;
  virtual void registerSelection(const blink::WebSelectionBound& start,
                                 const blink::WebSelectionBound& end) {}
  virtual void clearSelection() {}
  virtual void setShowFPSCounter(bool) {}
  virtual void setShowPaintRects(bool) {}
  virtual void setShowDebugBorders(bool) {}
  virtual void setContinuousPaintingEnabled(bool) {}
  virtual void setShowScrollBottleneckRects(bool) {}

  // OutputSurfaceMojoClient implementation.
  virtual void DidCreateSurface(cc::SurfaceId id) OVERRIDE;

 private:
  void OnSurfaceConnectionCreated(SurfacePtr surface, uint32_t id_namespace);
  void DidCreateSurfaceOnMainThread(cc::SurfaceId id);

  // widget_ and view_ will outlive us.
  blink::WebWidget* widget_;
  View* view_;
  scoped_ptr<cc::LayerTreeHost> layer_tree_host_;
  SurfacesServicePtr surfaces_service_;
  scoped_ptr<cc::OutputSurface> output_surface_;
  GpuPtr gpu_service_;
  scoped_refptr<base::SingleThreadTaskRunner>
      main_thread_compositor_task_runner_;
  base::WeakPtr<WebLayerTreeViewImpl> main_thread_bound_weak_ptr_;

  base::WeakPtrFactory<WebLayerTreeViewImpl> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(WebLayerTreeViewImpl);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_HTML_VIEWER_WEBLAYERTREEVIEW_IMPL_H_
