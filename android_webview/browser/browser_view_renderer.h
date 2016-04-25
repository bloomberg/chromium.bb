// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_BROWSER_VIEW_RENDERER_H_
#define ANDROID_WEBVIEW_BROWSER_BROWSER_VIEW_RENDERER_H_

#include <stddef.h>

#include <map>

#include "android_webview/browser/compositor_frame_producer.h"
#include "android_webview/browser/parent_compositor_draw_constraints.h"
#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/browser/android/synchronous_compositor_client.h"
#include "skia/ext/refptr.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

class SkCanvas;
class SkPicture;

namespace content {
class WebContents;
}

namespace android_webview {

class BrowserViewRendererClient;
class ChildFrame;
class CompositorFrameConsumer;

// Interface for all the WebView-specific content rendering operations.
// Provides software and hardware rendering and the Capture Picture API.
class BrowserViewRenderer : public content::SynchronousCompositorClient,
                            public CompositorFrameProducer {
 public:
  static void CalculateTileMemoryPolicy();
  static BrowserViewRenderer* FromWebContents(
      content::WebContents* web_contents);

  BrowserViewRenderer(
      BrowserViewRendererClient* client,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner);

  ~BrowserViewRenderer() override;

  void RegisterWithWebContents(content::WebContents* web_contents);

  // The BrowserViewRenderer client is responsible for ensuring that the
  // CompositorFrameConsumer has been set correctly via this method.
  void SetCompositorFrameConsumer(
      CompositorFrameConsumer* compositor_frame_consumer);

  // Called before either OnDrawHardware or OnDrawSoftware to set the view
  // state of this frame. |scroll| is the view's current scroll offset.
  // |global_visible_rect| is the intersection of the view size and the window
  // in window coordinates.
  void PrepareToDraw(const gfx::Vector2d& scroll,
                     const gfx::Rect& global_visible_rect);

  // Main handlers for view drawing. A false return value indicates no new
  // frame is produced.
  bool OnDrawHardware();
  bool OnDrawSoftware(SkCanvas* canvas);

  // CapturePicture API methods.
  skia::RefPtr<SkPicture> CapturePicture(int width, int height);
  void EnableOnNewPicture(bool enabled);

  void ClearView();

  void SetOffscreenPreRaster(bool enabled);

  // View update notifications.
  void SetIsPaused(bool paused);
  void SetViewVisibility(bool visible);
  void SetWindowVisibility(bool visible);
  void OnSizeChanged(int width, int height);
  void OnAttachedToWindow(int width, int height);
  void OnDetachedFromWindow();
  void ZoomBy(float delta);
  void OnComputeScroll(base::TimeTicks animation_time);

  // Sets the scale for logical<->physical pixel conversions.
  void SetDipScale(float dip_scale);
  float dip_scale() const { return dip_scale_; }
  float page_scale_factor() const { return page_scale_factor_; }

  // Set the root layer scroll offset to |new_value|.
  void ScrollTo(const gfx::Vector2d& new_value);

  // Android views hierarchy gluing.
  bool IsVisible() const;
  gfx::Rect GetScreenRect() const;
  bool attached_to_window() const { return attached_to_window_; }
  bool hardware_enabled() const { return hardware_enabled_; }
  gfx::Size size() const { return size_; }

  bool IsClientVisible() const;
  void TrimMemory();

  // SynchronousCompositorClient overrides.
  void DidInitializeCompositor(
      content::SynchronousCompositor* compositor) override;
  void DidDestroyCompositor(
      content::SynchronousCompositor* compositor) override;
  void DidBecomeCurrent(content::SynchronousCompositor* compositor) override;
  void PostInvalidate() override;
  void DidUpdateContent() override;
  void UpdateRootLayerState(const gfx::Vector2dF& total_scroll_offset_dip,
                            const gfx::Vector2dF& max_scroll_offset_dip,
                            const gfx::SizeF& scrollable_size_dip,
                            float page_scale_factor,
                            float min_page_scale_factor,
                            float max_page_scale_factor) override;
  void DidOverscroll(const gfx::Vector2dF& accumulated_overscroll,
                     const gfx::Vector2dF& latest_overscroll_delta,
                     const gfx::Vector2dF& current_fling_velocity) override;

  // CompositorFrameProducer overrides
  void OnParentDrawConstraintsUpdated() override;
  void OnCompositorFrameConsumerWillDestroy() override;

 private:
  void SetTotalRootLayerScrollOffset(const gfx::Vector2dF& new_value_dip);
  bool CanOnDraw();
  void UpdateCompositorIsActive();
  bool CompositeSW(SkCanvas* canvas);
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
  RootLayerStateAsValue(const gfx::Vector2dF& total_scroll_offset_dip,
                        const gfx::SizeF& scrollable_size_dip);

  void ReturnUnusedResource(std::unique_ptr<ChildFrame> frame);
  void ReturnResourceFromParent(
      CompositorFrameConsumer* compositor_frame_consumer);
  void ReleaseHardware();

  gfx::Vector2d max_scroll_offset() const;

  void UpdateMemoryPolicy();

  uint32_t GetCompositorID(content::SynchronousCompositor* compositor);
  // For debug tracing or logging. Return the string representation of this
  // view renderer's state.
  std::string ToString() const;

  BrowserViewRendererClient* const client_;
  const scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  CompositorFrameConsumer* compositor_frame_consumer_;

  // The current compositor that's owned by the current RVH.
  content::SynchronousCompositor* compositor_;
  // A map from compositor's per-WebView unique ID to the compositor's raw
  // pointer. A raw pointer here is fine because the entry will be erased when
  // a compositor is destroyed.
  std::map<size_t, content::SynchronousCompositor*> compositor_map_;

  bool is_paused_;
  bool view_visible_;
  bool window_visible_;  // Only applicable if |attached_to_window_| is true.
  bool attached_to_window_;
  bool hardware_enabled_;
  float dip_scale_;
  float page_scale_factor_;
  float min_page_scale_factor_;
  float max_page_scale_factor_;
  bool on_new_picture_enable_;
  bool clear_view_;

  bool offscreen_pre_raster_;

  gfx::Vector2d last_on_draw_scroll_offset_;
  gfx::Rect last_on_draw_global_visible_rect_;

  gfx::Size size_;

  gfx::SizeF scrollable_size_dip_;

  // Current scroll offset in CSS pixels.
  // TODO(miletus): Make scroll_offset_dip_ a gfx::ScrollOffset.
  gfx::Vector2dF scroll_offset_dip_;

  // Max scroll offset in CSS pixels.
  // TODO(miletus): Make max_scroll_offset_dip_ a gfx::ScrollOffset.
  gfx::Vector2dF max_scroll_offset_dip_;

  // Used to prevent rounding errors from accumulating enough to generate
  // visible skew (especially noticeable when scrolling up and down in the same
  // spot over a period of time).
  // TODO(miletus): Make overscroll_rounding_error_ a gfx::ScrollOffset.
  gfx::Vector2dF overscroll_rounding_error_;

  uint32_t next_compositor_id_;

  ParentCompositorDrawConstraints external_draw_constraints_;

  DISALLOW_COPY_AND_ASSIGN(BrowserViewRenderer);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_BROWSER_VIEW_RENDERER_H_
