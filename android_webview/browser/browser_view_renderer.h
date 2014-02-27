// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_BROWSER_VIEW_RENDERER_H_
#define ANDROID_WEBVIEW_BROWSER_BROWSER_VIEW_RENDERER_H_

#include "android_webview/browser/shared_renderer_state.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "content/public/browser/android/synchronous_compositor_client.h"
#include "skia/ext/refptr.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/vector2d_f.h"

class SkCanvas;
class SkPicture;
struct AwDrawGLInfo;
struct AwDrawSWFunctionTable;

namespace content {
class ContentViewCore;
class SynchronousCompositor;
class WebContents;
}

namespace android_webview {

class BrowserViewRendererClient;
class HardwareRenderer;

// Delegate to perform rendering actions involving Java objects.
class BrowserViewRendererJavaHelper {
 public:
  static BrowserViewRendererJavaHelper* GetInstance();

  typedef base::Callback<bool(SkCanvas*)> RenderMethod;

  // Try obtaining the native SkCanvas from |java_canvas| and call
  // |render_source| with it. If that fails, allocate an auxilary bitmap
  // for |render_source| to render into, then copy the bitmap into
  // |java_canvas|.
  virtual bool RenderViaAuxilaryBitmapIfNeeded(
      jobject java_canvas,
      const gfx::Vector2d& scroll_correction,
      const gfx::Rect& clip,
      RenderMethod render_source) = 0;

 protected:
  virtual ~BrowserViewRendererJavaHelper() {}
};

// Interface for all the WebView-specific content rendering operations.
// Provides software and hardware rendering and the Capture Picture API.
class BrowserViewRenderer : public content::SynchronousCompositorClient {
 public:
  BrowserViewRenderer(BrowserViewRendererClient* client,
                      SharedRendererState* shared_renderer_state,
                      content::WebContents* web_contents);

  virtual ~BrowserViewRenderer();

  // Main handler for view drawing: performs a SW draw immediately, or sets up
  // a subsequent GL Draw (via BrowserViewRendererClient::RequestDrawGL) and
  // returns true. A false return value indicates nothing was or will be drawn.
  // |java_canvas| is the target of the draw. |is_hardware_canvas| indicates
  // a GL Draw maybe possible on this canvas. |scroll| if the view's current
  // scroll offset. |clip| is the canvas's clip bounds. |visible_rect| is the
  // intersection of the view size and the window in window coordinates.
  bool OnDraw(jobject java_canvas,
              bool is_hardware_canvas,
              const gfx::Vector2d& scroll,
              const gfx::Rect& clip);

  // Called in response to a prior BrowserViewRendererClient::RequestDrawGL()
  // call. See AwDrawGLInfo documentation for more details of the contract.
  void DrawGL(AwDrawGLInfo* draw_info);

  // The global visible rect changed and this is the new value.
  void SetGlobalVisibleRect(const gfx::Rect& visible_rect);

  // CapturePicture API methods.
  skia::RefPtr<SkPicture> CapturePicture(int width, int height);
  void EnableOnNewPicture(bool enabled);

  void ClearView();

  // View update notifications.
  void SetIsPaused(bool paused);
  void SetViewVisibility(bool visible);
  void SetWindowVisibility(bool visible);
  void OnSizeChanged(int width, int height);
  void OnAttachedToWindow(int width, int height);
  void OnDetachedFromWindow();

  // Sets the scale for logical<->physical pixel conversions.
  void SetDipScale(float dip_scale);

  // Set the root layer scroll offset to |new_value|.
  void ScrollTo(gfx::Vector2d new_value);

  // Android views hierarchy gluing.
  bool IsAttachedToWindow() const;
  bool IsVisible() const;
  gfx::Rect GetScreenRect() const;

  // ComponentCallbacks2.onTrimMemory callback.
  void TrimMemory(int level);

  // SynchronousCompositorClient overrides
  virtual void DidInitializeCompositor(
      content::SynchronousCompositor* compositor) OVERRIDE;
  virtual void DidDestroyCompositor(content::SynchronousCompositor* compositor)
      OVERRIDE;
  virtual void SetContinuousInvalidate(bool invalidate) OVERRIDE;
  virtual void SetMaxRootLayerScrollOffset(gfx::Vector2dF new_value) OVERRIDE;
  virtual void SetTotalRootLayerScrollOffset(gfx::Vector2dF new_value_css)
      OVERRIDE;
  virtual void DidUpdateContent() OVERRIDE;
  virtual gfx::Vector2dF GetTotalRootLayerScrollOffset() OVERRIDE;
  virtual bool IsExternalFlingActive() const OVERRIDE;
  virtual void SetRootLayerPageScaleFactorAndLimits(float page_scale_factor,
                                                    float min_page_scale_factor,
                                                    float max_page_scale_factor)
      OVERRIDE;
  virtual void SetRootLayerScrollableSize(gfx::SizeF scrollable_size) OVERRIDE;
  virtual void DidOverscroll(gfx::Vector2dF accumulated_overscroll,
                             gfx::Vector2dF latest_overscroll_delta,
                             gfx::Vector2dF current_fling_velocity) OVERRIDE;

 private:
  // Checks the continuous invalidate and block invalidate state, and schedule
  // invalidates appropriately. If |invalidate_ignore_compositor| is true,
  // then send a view invalidate regardless of compositor expectation.
  void EnsureContinuousInvalidation(bool invalidate_ignore_compositor);
  bool DrawSWInternal(jobject java_canvas, const gfx::Rect& clip_bounds);
  bool CompositeSW(SkCanvas* canvas);

  // If we call up view invalidate and OnDraw is not called before a deadline,
  // then we keep ticking the SynchronousCompositor so it can make progress.
  void FallbackTickFired();
  void ForceFakeCompositeSW();

  gfx::Vector2d max_scroll_offset() const;

  // For debug tracing or logging. Return the string representation of this
  // view renderer's state and the |draw_info| if provided.
  std::string ToString(AwDrawGLInfo* draw_info) const;

  BrowserViewRendererClient* client_;
  SharedRendererState* shared_renderer_state_;
  content::WebContents* web_contents_;
  bool has_compositor_;

  scoped_ptr<HardwareRenderer> hardware_renderer_;

  bool is_paused_;
  bool view_visible_;
  bool window_visible_;  // Only applicable if |attached_to_window_| is true.
  bool attached_to_window_;
  float dip_scale_;
  float page_scale_factor_;
  bool on_new_picture_enable_;
  bool clear_view_;

  // When true, we should continuously invalidate and keep drawing, for example
  // to drive animation. This value is set by the compositor and should always
  // reflect the expectation of the compositor and not be reused for other
  // states.
  bool compositor_needs_continuous_invalidate_;

  // Used to block additional invalidates while one is already pending or before
  // compositor draw which may switch continuous_invalidate on and off in the
  // process.
  bool block_invalidates_;

  // Holds a callback to FallbackTickFired while it is pending.
  base::CancelableClosure fallback_tick_;

  int width_;
  int height_;

  DrawGLInput draw_gl_input_;

  // Current scroll offset in CSS pixels.
  gfx::Vector2dF scroll_offset_dip_;

  // Max scroll offset in CSS pixels.
  gfx::Vector2dF max_scroll_offset_dip_;

  // Used to prevent rounding errors from accumulating enough to generate
  // visible skew (especially noticeable when scrolling up and down in the same
  // spot over a period of time).
  gfx::Vector2dF overscroll_rounding_error_;

  DISALLOW_COPY_AND_ASSIGN(BrowserViewRenderer);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_BROWSER_VIEW_RENDERER_H_
