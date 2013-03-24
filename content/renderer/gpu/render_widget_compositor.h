// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_RENDER_WIDGET_COMPOSITOR_H_
#define CONTENT_RENDERER_GPU_RENDER_WIDGET_COMPOSITOR_H_

#include "base/time.h"
#include "cc/debug/rendering_stats.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_settings.h"
#include "skia/ext/refptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerTreeView.h"

class SkPicture;

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
  static scoped_ptr<RenderWidgetCompositor> Create(RenderWidget* widget);

  virtual ~RenderWidgetCompositor();

  void SetSuppressScheduleComposite(bool suppress);
  void Animate(base::TimeTicks time);
  void Composite(base::TimeTicks frame_begin_time);
  void GetRenderingStats(cc::RenderingStats* stats);
  skia::RefPtr<SkPicture> CapturePicture();
  void EnableHidingTopControls(bool enable);
  void SetOverdrawBottomHeight(float overdraw_bottom_height);

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
  virtual void WillBeginFrame() OVERRIDE;
  virtual void DidBeginFrame() OVERRIDE;
  virtual void Animate(double frame_begin_time) OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void ApplyScrollAndScale(gfx::Vector2d scroll_delta,
                                   float page_scale) OVERRIDE;
  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface() OVERRIDE;
  virtual void DidRecreateOutputSurface(bool success) OVERRIDE;
  virtual scoped_ptr<cc::InputHandler> CreateInputHandler() OVERRIDE;
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
  explicit RenderWidgetCompositor(RenderWidget* widget);

  bool initialize(cc::LayerTreeSettings settings);

  bool threaded_;
  bool suppress_schedule_composite_;
  RenderWidget* widget_;
  scoped_ptr<cc::LayerTreeHost> layer_tree_host_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_RENDER_WIDGET_COMPOSITOR_H_

