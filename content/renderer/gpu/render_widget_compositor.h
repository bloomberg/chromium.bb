// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_RENDER_WIDGET_COMPOSITOR_H_
#define CONTENT_RENDERER_GPU_RENDER_WIDGET_COMPOSITOR_H_

#include "cc/layer_tree_host_client.h"
#include "cc/layer_tree_settings.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerTreeView.h"

namespace cc {
class LayerTreeHost;
}

namespace content {
class RenderWidget;

class RenderWidgetCompositor : public WebKit::WebLayerTreeView,
                               public cc::LayerTreeHostClient {
 public:
  // Attempt to construct and initialize a compositor instance for the widget
  // with the given settings. Returns NULL if initialization fails.
  static scoped_ptr<RenderWidgetCompositor> Create(
      RenderWidget* widget,
      WebKit::WebLayerTreeViewClient* client,
      WebKit::WebLayerTreeView::Settings settings);

  virtual ~RenderWidgetCompositor();

  cc::LayerTreeHost* layer_tree_host() const { return layer_tree_host_.get(); }

  // WebLayerTreeView implementation.
  virtual void setSurfaceReady();
  virtual void setRootLayer(const WebKit::WebLayer& layer);
  virtual void clearRootLayer();
  virtual void setViewportSize(
      const WebKit::WebSize& layout_viewport_size,
      const WebKit::WebSize& device_viewport_size);
  virtual WebKit::WebSize layoutViewportSize() const;
  virtual WebKit::WebSize deviceViewportSize() const;
  virtual WebKit::WebFloatPoint adjustEventPointForPinchZoom(
      const WebKit::WebFloatPoint& point) const;
  virtual void setDeviceScaleFactor(float device_scale);
  virtual float deviceScaleFactor() const;
  virtual void setBackgroundColor(WebKit::WebColor color);
  virtual void setHasTransparentBackground(bool transparent);
  virtual void setVisible(bool visible);
  virtual void setPageScaleFactorAndLimits(float page_scale_factor,
                                           float minimum,
                                           float maximum);
  virtual void startPageScaleAnimation(const WebKit::WebPoint& destination,
                                       bool use_anchor,
                                       float new_page_scale,
                                       double duration_sec);
  virtual void setNeedsAnimate();
  virtual void setNeedsRedraw();
  virtual bool commitRequested() const;
  virtual void composite();
  virtual void updateAnimations(double frame_begin_time);
  virtual void didStopFlinging();
  virtual bool compositeAndReadback(void *pixels, const WebKit::WebRect& rect);
  virtual void finishAllRendering();
  virtual void setDeferCommits(bool defer_commits);
  virtual void registerForAnimations(WebKit::WebLayer* layer);
  virtual void renderingStats(WebKit::WebRenderingStats& stats) const {}
  virtual void setShowFPSCounter(bool show);
  virtual void setShowPaintRects(bool show);
  virtual void setShowDebugBorders(bool show);
  virtual void setContinuousPaintingEnabled(bool enabled);

  // cc::LayerTreeHostClient implementation.
  virtual void willBeginFrame() OVERRIDE;
  virtual void didBeginFrame() OVERRIDE;
  virtual void animate(double monotonic_frame_begin_time) OVERRIDE;
  virtual void layout() OVERRIDE;
  virtual void applyScrollAndScale(gfx::Vector2d scroll_delta,
                                   float page_scale) OVERRIDE;
  virtual scoped_ptr<cc::OutputSurface> createOutputSurface() OVERRIDE;
  virtual void didRecreateOutputSurface(bool success) OVERRIDE;
  virtual scoped_ptr<cc::InputHandler> createInputHandler() OVERRIDE;
  virtual void willCommit() OVERRIDE;
  virtual void didCommit() OVERRIDE;
  virtual void didCommitAndDrawFrame() OVERRIDE;
  virtual void didCompleteSwapBuffers() OVERRIDE;
  virtual void scheduleComposite() OVERRIDE;
  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForMainThread() OVERRIDE;
  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForCompositorThread() OVERRIDE;

private:
  RenderWidgetCompositor(RenderWidget* widget,
                         WebKit::WebLayerTreeViewClient* client);

  bool initialize(cc::LayerTreeSettings settings);

  bool threaded_;
  RenderWidget* widget_;
  WebKit::WebLayerTreeViewClient* client_;
  scoped_ptr<cc::LayerTreeHost> layer_tree_host_;

  class MainThreadContextProvider;
  scoped_refptr<MainThreadContextProvider> contexts_main_thread_;
  class CompositorThreadContextProvider;
  scoped_refptr<CompositorThreadContextProvider> contexts_compositor_thread_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_RENDER_WIDGET_COMPOSITOR_H_

