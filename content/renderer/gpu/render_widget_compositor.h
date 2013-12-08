// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_RENDER_WIDGET_COMPOSITOR_H_
#define CONTENT_RENDERER_GPU_RENDER_WIDGET_COMPOSITOR_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/base/swap_promise_monitor.h"
#include "cc/debug/rendering_stats.h"
#include "cc/input/top_controls_state.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "cc/trees/layer_tree_settings.h"
#include "third_party/WebKit/public/platform/WebLayerTreeView.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/rect.h"

namespace ui {
struct LatencyInfo;
}

namespace cc {
class InputHandler;
class Layer;
class LayerTreeHost;
}

namespace content {
class RenderWidget;

class RenderWidgetCompositor : public blink::WebLayerTreeView,
                               public cc::LayerTreeHostClient,
                               public cc::LayerTreeHostSingleThreadClient {
 public:
  // Attempt to construct and initialize a compositor instance for the widget
  // with the given settings. Returns NULL if initialization fails.
  static scoped_ptr<RenderWidgetCompositor> Create(RenderWidget* widget,
                                                   bool threaded);

  virtual ~RenderWidgetCompositor();

  const base::WeakPtr<cc::InputHandler>& GetInputHandler();
  void SetSuppressScheduleComposite(bool suppress);
  bool BeginMainFrameRequested() const;
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
  // Like setNeedsRedraw but forces the frame to be drawn, without early-outs.
  // Redraw will be forced after the next commit
  void SetNeedsForcedRedraw();
  // Calling CreateLatencyInfoSwapPromiseMonitor() to get a scoped
  // LatencyInfoSwapPromiseMonitor. During the life time of the
  // LatencyInfoSwapPromiseMonitor, if SetNeedsCommit() or SetNeedsUpdateLayer()
  // is called on LayerTreeHost, the original latency info will be turned
  // into a LatencyInfoSwapPromise.
  scoped_ptr<cc::SwapPromiseMonitor> CreateLatencyInfoSwapPromiseMonitor(
      ui::LatencyInfo* latency);
  int GetLayerTreeId() const;
  void NotifyInputThrottledUntilCommit();
  const cc::Layer* GetRootLayer() const;
  bool ScheduleMicroBenchmark(
      const std::string& name,
      scoped_ptr<base::Value> value,
      const base::Callback<void(scoped_ptr<base::Value>)>& callback);

  // WebLayerTreeView implementation.
  virtual void setSurfaceReady();
  virtual void setRootLayer(const blink::WebLayer& layer);
  virtual void clearRootLayer();
  virtual void setViewportSize(
      const blink::WebSize& unused_deprecated,
      const blink::WebSize& device_viewport_size);
  virtual blink::WebSize layoutViewportSize() const;
  virtual blink::WebSize deviceViewportSize() const;
  virtual blink::WebFloatPoint adjustEventPointForPinchZoom(
      const blink::WebFloatPoint& point) const;
  virtual void setDeviceScaleFactor(float device_scale);
  virtual float deviceScaleFactor() const;
  virtual void setBackgroundColor(blink::WebColor color);
  virtual void setHasTransparentBackground(bool transparent);
  virtual void setOverhangBitmap(const SkBitmap& bitmap);
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
  virtual bool compositeAndReadback(void *pixels, const blink::WebRect& rect);
  virtual void finishAllRendering();
  virtual void setDeferCommits(bool defer_commits);
  virtual void registerForAnimations(blink::WebLayer* layer);
  virtual void registerViewportLayers(
      const blink::WebLayer* pageScaleLayer,
      const blink::WebLayer* innerViewportScrollLayer,
      const blink::WebLayer* outerViewportScrollLayer) OVERRIDE;
  virtual void clearViewportLayers() OVERRIDE;
  virtual void renderingStats(blink::WebRenderingStats& stats) const {}
  virtual void setShowFPSCounter(bool show);
  virtual void setShowPaintRects(bool show);
  virtual void setShowDebugBorders(bool show);
  virtual void setContinuousPaintingEnabled(bool enabled);
  virtual void setShowScrollBottleneckRects(bool show);

  // cc::LayerTreeHostClient implementation.
  virtual void WillBeginMainFrame(int frame_id) OVERRIDE;
  virtual void DidBeginMainFrame() OVERRIDE;
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
  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProvider() OVERRIDE;
  virtual void RateLimitSharedMainThreadContext() OVERRIDE;

  // cc::LayerTreeHostSingleThreadClient implementation.
  virtual void ScheduleComposite() OVERRIDE;
  virtual void ScheduleAnimation() OVERRIDE;
  virtual void DidPostSwapBuffers() OVERRIDE;
  virtual void DidAbortSwapBuffers() OVERRIDE;

 private:
  RenderWidgetCompositor(RenderWidget* widget, bool threaded);

  bool Initialize(cc::LayerTreeSettings settings);

  bool threaded_;
  bool suppress_schedule_composite_;
  RenderWidget* widget_;
  scoped_ptr<cc::LayerTreeHost> layer_tree_host_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_RENDER_WIDGET_COMPOSITOR_H_
