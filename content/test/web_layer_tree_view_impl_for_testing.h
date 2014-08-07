// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_WEB_LAYER_TREE_VIEW_IMPL_FOR_TESTING_H_
#define CONTENT_TEST_WEB_LAYER_TREE_VIEW_IMPL_FOR_TESTING_H_

#include "base/memory/scoped_ptr.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "third_party/WebKit/public/platform/WebLayerTreeView.h"

namespace cc {
class LayerTreeHost;
}

namespace blink { class WebLayer; }

namespace content {

class WebLayerTreeViewImplForTesting
    : public blink::WebLayerTreeView,
      public cc::LayerTreeHostClient,
      public cc::LayerTreeHostSingleThreadClient {
 public:
  WebLayerTreeViewImplForTesting();
  virtual ~WebLayerTreeViewImplForTesting();

  void Initialize();

  // blink::WebLayerTreeView implementation.
  virtual void setSurfaceReady();
  virtual void setRootLayer(const blink::WebLayer& layer);
  virtual void clearRootLayer();
  virtual void setViewportSize(const blink::WebSize& unused_deprecated,
                               const blink::WebSize& device_viewport_size);
  virtual void setViewportSize(const blink::WebSize& device_viewport_size);
  virtual blink::WebSize layoutViewportSize() const;
  virtual blink::WebSize deviceViewportSize() const;
  virtual void setDeviceScaleFactor(float scale_factor);
  virtual float deviceScaleFactor() const;
  virtual void setBackgroundColor(blink::WebColor);
  virtual void setHasTransparentBackground(bool transparent);
  virtual void setVisible(bool visible);
  virtual void setPageScaleFactorAndLimits(float page_scale_factor,
                                           float minimum,
                                           float maximum);
  virtual void startPageScaleAnimation(const blink::WebPoint& destination,
                                       bool use_anchor,
                                       float new_page_scale,
                                       double duration_sec);
  virtual void setNeedsAnimate();
  virtual bool commitRequested() const;
  virtual void didStopFlinging();
  virtual void finishAllRendering();
  virtual void setDeferCommits(bool defer_commits);
  virtual void registerViewportLayers(
      const blink::WebLayer* pageScaleLayerLayer,
      const blink::WebLayer* innerViewportScrollLayer,
      const blink::WebLayer* outerViewportScrollLayer) OVERRIDE;
  virtual void clearViewportLayers() OVERRIDE;
  virtual void registerSelection(
      const blink::WebSelectionBound& start,
      const blink::WebSelectionBound& end) OVERRIDE;
  virtual void clearSelection() OVERRIDE;

  // cc::LayerTreeHostClient implementation.
  virtual void WillBeginMainFrame(int frame_id) OVERRIDE {}
  virtual void DidBeginMainFrame() OVERRIDE {}
  virtual void Animate(base::TimeTicks frame_begin_time) OVERRIDE {}
  virtual void Layout() OVERRIDE;
  virtual void ApplyScrollAndScale(const gfx::Vector2d& scroll_delta,
                                   float page_scale) OVERRIDE;
  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface(bool fallback)
      OVERRIDE;
  virtual void DidInitializeOutputSurface() OVERRIDE {}
  virtual void WillCommit() OVERRIDE {}
  virtual void DidCommit() OVERRIDE {}
  virtual void DidCommitAndDrawFrame() OVERRIDE {}
  virtual void DidCompleteSwapBuffers() OVERRIDE {}

  // cc::LayerTreeHostSingleThreadClient implementation.
  virtual void DidPostSwapBuffers() OVERRIDE {}
  virtual void DidAbortSwapBuffers() OVERRIDE {}

 private:
  scoped_ptr<cc::LayerTreeHost> layer_tree_host_;

  DISALLOW_COPY_AND_ASSIGN(WebLayerTreeViewImplForTesting);
};

}  // namespace content

#endif  // CONTENT_TEST_WEB_LAYER_TREE_VIEW_IMPL_FOR_TESTING_H_
