// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_WEB_LAYER_TREE_VIEW_IMPL_H_
#define COMPONENTS_HTML_VIEWER_WEB_LAYER_TREE_VIEW_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "cc/trees/layer_tree_host_client.h"
#include "components/mus/public/cpp/output_surface.h"
#include "components/mus/public/interfaces/gpu.mojom.h"
#include "third_party/WebKit/public/platform/WebLayerTreeView.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
class WebWidget;
}

namespace cc {
class LayerTreeHost;
class TaskGraphRunner;
}

namespace gpu {
class GpuMemoryBufferManager;
}

namespace html_viewer {

class WebLayerTreeViewImpl : public blink::WebLayerTreeView,
                             public cc::LayerTreeHostClient {
 public:
  WebLayerTreeViewImpl(
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      cc::TaskGraphRunner* task_graph_runner);
  ~WebLayerTreeViewImpl() override;

  void Initialize(mus::mojom::GpuPtr gpu_service,
                  mus::Window* window,
                  blink::WebWidget* widget);

  // cc::LayerTreeHostClient implementation.
  void WillBeginMainFrame() override;
  void DidBeginMainFrame() override;
  void BeginMainFrame(const cc::BeginFrameArgs& args) override;
  void BeginMainFrameNotExpectedSoon() override;
  void UpdateLayerTreeHost() override;
  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float page_scale,
                           float top_controls_delta) override;
  void RequestNewOutputSurface() override;
  void DidFailToInitializeOutputSurface() override;
  void DidInitializeOutputSurface() override;
  void WillCommit() override;
  void DidCommit() override;
  void DidCommitAndDrawFrame() override;
  void DidCompleteSwapBuffers() override;
  void DidCompletePageScaleAnimation() override {}
  void RecordFrameTimingEvents(
      scoped_ptr<cc::FrameTimingTracker::CompositeTimingSet> composite_events,
      scoped_ptr<cc::FrameTimingTracker::MainFrameTimingSet> main_frame_events)
      override {}

  // blink::WebLayerTreeView implementation.
  virtual void setRootLayer(const blink::WebLayer& layer);
  virtual void clearRootLayer();
  virtual void setViewportSize(const blink::WebSize& device_viewport_size);
  virtual void setDeviceScaleFactor(float);
  virtual float deviceScaleFactor() const;
  virtual void setBackgroundColor(blink::WebColor color);
  virtual void setHasTransparentBackground(bool has_transparent_background);
  virtual void setVisible(bool visible);
  virtual void setPageScaleFactorAndLimits(float page_scale_factor,
                                           float minimum,
                                           float maximum);
  virtual void startPageScaleAnimation(const blink::WebPoint& destination,
                                       bool use_anchor,
                                       float new_page_scale,
                                       double duration_sec);
  virtual void heuristicsForGpuRasterizationUpdated(bool matches_heuristic) {}
  virtual void setNeedsAnimate();
  virtual void didStopFlinging() {}
  virtual void compositeAndReadbackAsync(
      blink::WebCompositeAndReadbackAsyncCallback* callback) {}
  virtual void setDeferCommits(bool defer_commits) {}
  virtual void registerForAnimations(blink::WebLayer* layer);
  virtual void registerViewportLayers(
      const blink::WebLayer* overscrollElasticityLayer,
      const blink::WebLayer* pageScaleLayerLayer,
      const blink::WebLayer* innerViewportScrollLayer,
      const blink::WebLayer* outerViewportScrollLayer);
  virtual void clearViewportLayers();
  virtual void registerSelection(const blink::WebSelection& selection) {}
  virtual void clearSelection() {}
  virtual void setShowFPSCounter(bool) {}
  virtual void setShowPaintRects(bool) {}
  virtual void setShowDebugBorders(bool) {}
  virtual void setShowScrollBottleneckRects(bool) {}

 private:
  // widget_ and window_ will outlive us.
  blink::WebWidget* widget_;
  mus::Window* window_;
  scoped_ptr<cc::LayerTreeHost> layer_tree_host_;
  scoped_ptr<cc::OutputSurface> output_surface_;
  scoped_refptr<base::SingleThreadTaskRunner>
      main_thread_compositor_task_runner_;
  base::WeakPtr<WebLayerTreeViewImpl> main_thread_bound_weak_ptr_;

  base::WeakPtrFactory<WebLayerTreeViewImpl> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(WebLayerTreeViewImpl);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_WEB_LAYER_TREE_VIEW_IMPL_H_
