// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_RENDER_WIDGET_COMPOSITOR_H_
#define CONTENT_RENDERER_GPU_RENDER_WIDGET_COMPOSITOR_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/debug/rendering_stats.h"
#include "cc/input/top_controls_state.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_settings.h"
#include "third_party/WebKit/public/platform/WebLayerTreeView.h"
#include "ui/gfx/rect.h"

namespace ui {
struct LatencyInfo;
}

namespace cc {
class InputHandler;
class LayerTreeHost;
}

namespace content {
class RenderWidget;

class RenderWidgetCompositor : public WebKit::WebLayerTreeView,
                               public cc::LayerTreeHostClient {
 public:
  // Attempt to construct and initialize a compositor instance for the widget
  // with the given settings. Returns NULL if initialization fails.
  static scoped_ptr<RenderWidgetCompositor> Create(RenderWidget* widget,
                                                   bool threaded);

  virtual ~RenderWidgetCompositor();

  const base::WeakPtr<cc::InputHandler>& GetInputHandler();
  void SetSuppressScheduleComposite(bool suppress);
  void Animate(base::TimeTicks time);
  void Composite(base::TimeTicks frame_begin_time);
  void SetNeedsDisplayOnAllLayers();
  void SetRasterizeOnlyVisibleContent();
  void GetRenderingStats(cc::RenderingStats* stats);
  void UpdateTopControlsState(cc::TopControlsState constraints,
                              cc::TopControlsState current,
                              bool animate);
  void SetOverdrawBottomHeight(float overdraw_bottom_height);
  void SetNeedsRedrawRect(gfx::Rect damage_rect);
  void SetLatencyInfo(const ui::LatencyInfo& latency_info);
  int GetLayerTreeId() const;
  void NotifyInputThrottledUntilCommit();

  // WebLayerTreeView implementation.
  virtual void setSurfaceReady();
  virtual void setRootLayer(const WebKit::WebLayer& layer);
  virtual void clearRootLayer();
  virtual void setViewportSize(
      const WebKit::WebSize& unused_deprecated,
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
  virtual void setShowScrollBottleneckRects(bool show);

  // cc::LayerTreeHostClient implementation.
  virtual void WillBeginFrame() OVERRIDE;
  virtual void DidBeginFrame() OVERRIDE;
  virtual void Animate(double frame_begin_time) OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void ApplyScrollAndScale(gfx::Vector2d scroll_delta,
                                   float page_scale) OVERRIDE;
  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface(bool fallback)
      OVERRIDE;
  virtual void DidInitializeOutputSurface(bool success) OVERRIDE;
  virtual void WillCommit() OVERRIDE;
  virtual void DidCommit() OVERRIDE;
  virtual void DidCommitAndDrawFrame() OVERRIDE;
  virtual void DidCompleteSwapBuffers() OVERRIDE;
  virtual void ScheduleComposite() OVERRIDE;
  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForMainThread() OVERRIDE;
  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForCompositorThread() OVERRIDE;

 private:
  RenderWidgetCompositor(RenderWidget* widget, bool threaded);

  bool initialize(cc::LayerTreeSettings settings);

  bool threaded_;
  bool suppress_schedule_composite_;
  RenderWidget* widget_;
  scoped_ptr<cc::LayerTreeHost> layer_tree_host_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_RENDER_WIDGET_COMPOSITOR_H_
