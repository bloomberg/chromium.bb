// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_BROWSER_VIEW_RENDERER_IMPL_H_
#define ANDROID_WEBVIEW_BROWSER_BROWSER_VIEW_RENDERER_IMPL_H_

#include "android_webview/browser/browser_view_renderer.h"
#include "android_webview/browser/renderer_host/view_renderer_host.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/web_contents_observer.h"
#include "skia/ext/refptr.h"
#include "ui/gfx/point.h"
#include "ui/gfx/size.h"

typedef void* EGLContext;
struct AwDrawSWFunctionTable;
struct AwDrawGLInfo;
class SkCanvas;
class SkPicture;

namespace content {
class WebContents;
}

namespace android_webview {

class BrowserViewRendererImpl
    : public BrowserViewRenderer,
      public ViewRendererHost::Client,
      public content::Compositor::Client {
 public:
  static BrowserViewRendererImpl* Create(BrowserViewRenderer::Client* client,
                                         JavaHelper* java_helper);
  virtual ~BrowserViewRendererImpl();

  // Platform methods.
  static void SetAwDrawSWFunctionTable(AwDrawSWFunctionTable* table);

  // BrowserViewRenderer implementation.
  virtual void SetContents(
      content::ContentViewCore* content_view_core) OVERRIDE;
  virtual void DrawGL(AwDrawGLInfo* draw_info) OVERRIDE;
  virtual void SetScrollForHWFrame(int x, int y) OVERRIDE;
  virtual bool DrawSW(jobject java_canvas, const gfx::Rect& clip) OVERRIDE;
  virtual base::android::ScopedJavaLocalRef<jobject> CapturePicture() OVERRIDE;
  virtual void EnableOnNewPicture(OnNewPictureMode mode) OVERRIDE;
  virtual void OnVisibilityChanged(
      bool view_visible, bool window_visible) OVERRIDE;
  virtual void OnSizeChanged(int width, int height) OVERRIDE;
  virtual void OnAttachedToWindow(int width, int height) OVERRIDE;
  virtual void OnDetachedFromWindow() OVERRIDE;

  // content::Compositor::Client implementation.
  virtual void ScheduleComposite() OVERRIDE;

  // ViewRendererHost::Client implementation.
  virtual void OnPictureUpdated(int process_id, int render_view_id) OVERRIDE;

 protected:
  BrowserViewRendererImpl(BrowserViewRenderer::Client* client,
                          JavaHelper* java_helper);

 private:
  // Returns the latest locally available picture if any.
  // If none is available will synchronously request the latest one
  // and block until the result is received.
  skia::RefPtr<SkPicture> GetLastCapturedPicture();
  void OnPictureUpdated();

  void SetCompositorVisibility(bool visible);
  void ResetCompositor();
  void Invalidate();
  bool RenderSW(SkCanvas* canvas);
  bool RenderPicture(SkCanvas* canvas);

  BrowserViewRenderer::Client* client_;
  BrowserViewRenderer::JavaHelper* java_helper_;

  scoped_ptr<ViewRendererHost> view_renderer_host_;
  scoped_ptr<content::Compositor> compositor_;

  // Ensures content keeps clipped within the view during transformations.
  scoped_refptr<cc::Layer> view_clip_layer_;

  // Applies the transformation matrix.
  scoped_refptr<cc::Layer> transform_layer_;

  // Ensures content is drawn within the scissor clip rect provided by the
  // Android framework.
  scoped_refptr<cc::Layer> scissor_clip_layer_;

  // Last View scroll before hardware rendering is triggered.
  gfx::Point hw_rendering_scroll_;

  bool view_visible_;
  bool compositor_visible_;
  bool is_composite_pending_;
  float dpi_scale_;
  gfx::Size view_size_;
  OnNewPictureMode on_new_picture_mode_;

  // Used only for detecting Android View System context changes.
  // Not to be used between draw calls.
  EGLContext last_frame_context_;

  // Set via SetContents. Used to recognize updates to the local WebView.
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(BrowserViewRendererImpl);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_BROWSER_VIEW_RENDERER_IMPL_H_
