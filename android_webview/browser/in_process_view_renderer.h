// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_IN_PROCESS_IN_PROCESS_VIEW_RENDERER_H_
#define ANDROID_WEBVIEW_BROWSER_IN_PROCESS_IN_PROCESS_VIEW_RENDERER_H_

#include <string>

#include "android_webview/browser/browser_view_renderer.h"
#include "base/cancelable_callback.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/android/synchronous_compositor_client.h"
#include "ui/gfx/vector2d_f.h"

namespace content {
class SynchronousCompositor;
class WebContents;
}

typedef void* EGLContext;
class SkCanvas;

namespace android_webview {

// Provides RenderViewHost wrapper functionality for sending WebView-specific
// IPC messages to the renderer and from there to WebKit.
class InProcessViewRenderer : public BrowserViewRenderer,
                              public content::SynchronousCompositorClient {
 public:
  InProcessViewRenderer(BrowserViewRenderer::Client* client,
                        JavaHelper* java_helper,
                        content::WebContents* web_contents);
  virtual ~InProcessViewRenderer();

  static InProcessViewRenderer* FromWebContents(
      content::WebContents* contents);

  // BrowserViewRenderer overrides
  virtual bool OnDraw(jobject java_canvas,
                      bool is_hardware_canvas,
                      const gfx::Vector2d& scroll_,
                      const gfx::Rect& clip,
                      const gfx::Rect& visible_rect) OVERRIDE;
  virtual void DrawGL(AwDrawGLInfo* draw_info) OVERRIDE;
  virtual base::android::ScopedJavaLocalRef<jobject> CapturePicture(
      int width,
      int height) OVERRIDE;
  virtual void EnableOnNewPicture(bool enabled) OVERRIDE;
  virtual void OnVisibilityChanged(bool visible) OVERRIDE;
  virtual void OnSizeChanged(int width, int height) OVERRIDE;
  virtual void ScrollTo(gfx::Vector2d new_value) OVERRIDE;
  virtual void SetPageScaleFactor(float page_scale_factor) OVERRIDE;
  virtual void OnAttachedToWindow(int width, int height) OVERRIDE;
  virtual void OnDetachedFromWindow() OVERRIDE;
  virtual void SetDipScale(float dip_scale) OVERRIDE;
  virtual bool IsAttachedToWindow() OVERRIDE;
  virtual bool IsViewVisible() OVERRIDE;
  virtual gfx::Rect GetScreenRect() OVERRIDE;

  // SynchronousCompositorClient overrides
  virtual void DidInitializeCompositor(
      content::SynchronousCompositor* compositor) OVERRIDE;
  virtual void DidDestroyCompositor(
      content::SynchronousCompositor* compositor) OVERRIDE;
  virtual void SetContinuousInvalidate(bool invalidate) OVERRIDE;
  virtual void SetTotalRootLayerScrollOffset(
      gfx::Vector2dF new_value_css) OVERRIDE;
  virtual void DidUpdateContent() OVERRIDE;
  virtual gfx::Vector2dF GetTotalRootLayerScrollOffset() OVERRIDE;
  virtual void DidOverscroll(gfx::Vector2dF latest_overscroll_delta,
                             gfx::Vector2dF current_fling_velocity) OVERRIDE;

  void WebContentsGone();

 private:
  void EnsureContinuousInvalidation(AwDrawGLInfo* draw_info);
  bool DrawSWInternal(jobject java_canvas,
                      const gfx::Rect& clip_bounds);
  bool CompositeSW(SkCanvas* canvas);

  // If we call up view invalidate and OnDraw is not called before a deadline,
  // then we keep ticking the SynchronousCompositor so it can make progress.
  void FallbackTickFired();

  // For debug tracing or logging. Return the string representation of this
  // view renderer's state and the |draw_info| if provided.
  std::string ToString(AwDrawGLInfo* draw_info) const;

  BrowserViewRenderer::Client* client_;
  BrowserViewRenderer::JavaHelper* java_helper_;
  content::WebContents* web_contents_;
  content::SynchronousCompositor* compositor_;

  bool visible_;
  float dip_scale_;
  float page_scale_factor_;
  bool on_new_picture_enable_;

  // When true, we should continuously invalidate and keep drawing, for example
  // to drive animation.
  bool continuous_invalidate_;
  // Used to block additional invalidates while one is already pending or before
  // compositor draw which may switch continuous_invalidate on and off in the
  // process.
  bool block_invalidates_;
  // Holds a callback to FallbackTickFired while it is pending.
  base::CancelableClosure fallback_tick_;

  int width_;
  int height_;

  bool attached_to_window_;
  bool hardware_initialized_;
  bool hardware_failed_;

  // Used only for detecting Android View System context changes.
  // Not to be used between draw calls.
  EGLContext last_egl_context_;

  gfx::Rect global_visible_rect_at_start_of_frame_;

  // Last View scroll when View.onDraw() was called.
  gfx::Vector2d scroll_at_start_of_frame_;

  // Current scroll offset in CSS pixels.
  gfx::Vector2dF scroll_offset_css_;

  // Used to prevent rounding errors from accumulating enough to generate
  // visible skew (especially noticeable when scrolling up and down in the same
  // spot over a period of time).
  gfx::Vector2dF overscroll_rounding_error_;

  DISALLOW_COPY_AND_ASSIGN(InProcessViewRenderer);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_IN_PROCESS_IN_PROCESS_VIEW_RENDERER_H_
