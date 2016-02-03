// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_WEB_LAYER_TREE_VIEW_IMPL_FOR_TESTING_H_
#define CONTENT_TEST_WEB_LAYER_TREE_VIEW_IMPL_FOR_TESTING_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "third_party/WebKit/public/platform/WebLayerTreeView.h"

namespace cc {
class LayerTreeHost;
}

namespace blink {
class WebCompositorAnimationTimeline;
class WebLayer;
}

namespace content {

// Dummy WeblayerTeeView that does not support any actual compositing.
class WebLayerTreeViewImplForTesting
    : public blink::WebLayerTreeView,
      public cc::LayerTreeHostClient,
      public cc::LayerTreeHostSingleThreadClient {
 public:
  WebLayerTreeViewImplForTesting();
  ~WebLayerTreeViewImplForTesting() override;

  void Initialize();

  // blink::WebLayerTreeView implementation.
  void setRootLayer(const blink::WebLayer& layer) override;
  void clearRootLayer() override;
  void attachCompositorAnimationTimeline(
      blink::WebCompositorAnimationTimeline*) override;
  void detachCompositorAnimationTimeline(
      blink::WebCompositorAnimationTimeline*) override;
  virtual void setViewportSize(const blink::WebSize& unused_deprecated,
                               const blink::WebSize& device_viewport_size);
  void setViewportSize(const blink::WebSize& device_viewport_size) override;
  void setDeviceScaleFactor(float scale_factor) override;
  void setBackgroundColor(blink::WebColor) override;
  void setHasTransparentBackground(bool transparent) override;
  void setVisible(bool visible) override;
  void setPageScaleFactorAndLimits(float page_scale_factor,
                                   float minimum,
                                   float maximum) override;
  void startPageScaleAnimation(const blink::WebPoint& destination,
                               bool use_anchor,
                               float new_page_scale,
                               double duration_sec) override;
  void setNeedsAnimate() override;
  void didStopFlinging() override;
  void setDeferCommits(bool defer_commits) override;
  void registerForAnimations(blink::WebLayer* layer) override;
  void registerViewportLayers(
      const blink::WebLayer* overscrollElasticityLayer,
      const blink::WebLayer* pageScaleLayerLayer,
      const blink::WebLayer* innerViewportScrollLayer,
      const blink::WebLayer* outerViewportScrollLayer) override;
  void clearViewportLayers() override;
  void registerSelection(const blink::WebSelection& selection) override;
  void clearSelection() override;
  void setHaveWheelEventHandlers(bool have_event_handlers) override;
  bool haveWheelEventHandlers() const override;
  void setHaveScrollEventHandlers(bool) override;
  bool haveScrollEventHandlers() const override;

  // cc::LayerTreeHostClient implementation.
  void WillBeginMainFrame() override {}
  void DidBeginMainFrame() override {}
  void BeginMainFrame(const cc::BeginFrameArgs& args) override {}
  void BeginMainFrameNotExpectedSoon() override {}
  void UpdateLayerTreeHost() override;
  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float page_scale,
                           float top_controls_delta) override;
  void RequestNewOutputSurface() override;
  void DidInitializeOutputSurface() override {}
  void DidFailToInitializeOutputSurface() override;
  void WillCommit() override {}
  void DidCommit() override {}
  void DidCommitAndDrawFrame() override {}
  void DidCompleteSwapBuffers() override {}
  void DidCompletePageScaleAnimation() override {}
  void RecordFrameTimingEvents(
      scoped_ptr<cc::FrameTimingTracker::CompositeTimingSet> composite_events,
      scoped_ptr<cc::FrameTimingTracker::MainFrameTimingSet> main_frame_events)
      override {}

  // cc::LayerTreeHostSingleThreadClient implementation.
  void DidPostSwapBuffers() override {}
  void DidAbortSwapBuffers() override {}

 private:
  cc::TestTaskGraphRunner task_graph_runner_;
  scoped_ptr<cc::LayerTreeHost> layer_tree_host_;

  DISALLOW_COPY_AND_ASSIGN(WebLayerTreeViewImplForTesting);
};

}  // namespace content

#endif  // CONTENT_TEST_WEB_LAYER_TREE_VIEW_IMPL_FOR_TESTING_H_
