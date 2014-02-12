// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_BROWSER_VIEW_RENDERER_H_
#define ANDROID_WEBVIEW_BROWSER_BROWSER_VIEW_RENDERER_H_

#include "base/android/scoped_java_ref.h"
#include "skia/ext/refptr.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/vector2d_f.h"

class SkPicture;
struct AwDrawGLInfo;
struct AwDrawSWFunctionTable;

namespace content {
class ContentViewCore;
}

namespace android_webview {

// Interface for all the WebView-specific content rendering operations.
// Provides software and hardware rendering and the Capture Picture API.
class BrowserViewRenderer {
 public:
  class Client {
   public:
    // Request DrawGL be called. Passing null canvas implies the request
    // will be of AwDrawGLInfo::kModeProcess type. The callback
    // may never be made, and the mode may be promoted to kModeDraw.
    virtual bool RequestDrawGL(jobject canvas) = 0;

    // Called when a new Picture is available. Needs to be enabled
    // via the EnableOnNewPicture method.
    virtual void OnNewPicture() = 0;

    // Called to trigger view invalidations.
    virtual void PostInvalidate() = 0;

    // Synchronously call back to SetGlobalVisibleRect with current value.
    virtual void UpdateGlobalVisibleRect() = 0;

    // Called to get view's absolute location on the screen.
    virtual gfx::Point GetLocationOnScreen() = 0;

    // Try to set the view's scroll offset to |new_value|.
    virtual void ScrollContainerViewTo(gfx::Vector2d new_value) = 0;

    // Set the view's scroll offset cap to |new_value|.
    virtual void SetMaxContainerViewScrollOffset(gfx::Vector2d new_value) = 0;

    // Is a WebView-managed fling in progress?
    virtual bool IsFlingActive() const = 0;

    // Set the current page scale to |page_scale_factor| and page scale limits
    // to |min_page_scale_factor|..|max_page_scale_factor|.
    virtual void SetPageScaleFactorAndLimits(float page_scale_factor,
                                             float min_page_scale_factor,
                                             float max_page_scale_factor) = 0;

    // Set the current contents_size to |contents_size_dip|.
    virtual void SetContentsSize(gfx::SizeF contents_size_dip) = 0;

    // Handle overscroll.
    virtual void DidOverscroll(gfx::Vector2d overscroll_delta) = 0;

   protected:
    virtual ~Client() {}
  };

  // Delegate to perform rendering actions involving Java objects.
  class JavaHelper {
   public:
    // Creates a RGBA_8888 Java Bitmap object of the requested size.
    virtual base::android::ScopedJavaLocalRef<jobject> CreateBitmap(
        JNIEnv* env,
        int width,
        int height,
        const base::android::JavaRef<jobject>& jcanvas,
        void* owner_key) = 0;

    // Draws the provided Java Bitmap into the provided Java Canvas.
    virtual void DrawBitmapIntoCanvas(
        JNIEnv* env,
        const base::android::JavaRef<jobject>& jbitmap,
        const base::android::JavaRef<jobject>& jcanvas,
        int x,
        int y) = 0;

    // Creates a Java Picture object that records drawing the provided Bitmap.
    virtual base::android::ScopedJavaLocalRef<jobject> RecordBitmapIntoPicture(
        JNIEnv* env,
        const base::android::JavaRef<jobject>& jbitmap) = 0;

   protected:
    virtual ~JavaHelper() {}
  };

  // Global hookup methods.
  static void SetAwDrawSWFunctionTable(AwDrawSWFunctionTable* table);
  static AwDrawSWFunctionTable* GetAwDrawSWFunctionTable();

  // Rendering methods.

  // Main handler for view drawing: performs a SW draw immediately, or sets up
  // a subsequent GL Draw (via Client::RequestDrawGL) and returns true. A false
  // return value indicates nothing was or will be drawn.
  // |java_canvas| is the target of the draw. |is_hardware_canvas| indicates
  // a GL Draw maybe possible on this canvas. |scroll| if the view's current
  // scroll offset. |clip| is the canvas's clip bounds. |visible_rect| is the
  // intersection of the view size and the window in window coordinates.
  virtual bool OnDraw(jobject java_canvas,
                      bool is_hardware_canvas,
                      const gfx::Vector2d& scroll,
                      const gfx::Rect& clip) = 0;

  // Called in response to a prior Client::RequestDrawGL() call. See
  // AwDrawGLInfo documentation for more details of the contract.
  virtual void DrawGL(AwDrawGLInfo* draw_info) = 0;

  // The global visible rect changed and this is the new value.
  virtual void SetGlobalVisibleRect(const gfx::Rect& visible_rect) = 0;

  // CapturePicture API methods.
  virtual skia::RefPtr<SkPicture> CapturePicture(int width, int height) = 0;
  virtual void EnableOnNewPicture(bool enabled) = 0;

  virtual void ClearView() = 0;

  // View update notifications.
  virtual void SetIsPaused(bool paused) = 0;
  virtual void SetViewVisibility(bool visible) = 0;
  virtual void SetWindowVisibility(bool visible) = 0;
  virtual void OnSizeChanged(int width, int height) = 0;
  virtual void OnAttachedToWindow(int width, int height) = 0;
  virtual void OnDetachedFromWindow() = 0;

  // Sets the scale for logical<->physical pixel conversions.
  virtual void SetDipScale(float dip_scale) = 0;

  // Set the root layer scroll offset to |new_value|.
  virtual void ScrollTo(gfx::Vector2d new_value) = 0;

  // Android views hierarchy gluing.
  virtual bool IsAttachedToWindow() = 0;
  virtual bool IsVisible() = 0;
  virtual gfx::Rect GetScreenRect() = 0;

  // ComponentCallbacks2.onTrimMemory callback.
  virtual void TrimMemory(int level) = 0;

  virtual ~BrowserViewRenderer() {}
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_BROWSER_VIEW_RENDERER_H_
