// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_BROWSER_VIEW_RENDERER_H_
#define ANDROID_WEBVIEW_BROWSER_BROWSER_VIEW_RENDERER_H_

#include "base/android/scoped_java_ref.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

struct AwDrawGLInfo;

namespace content {
class ContentViewCore;
class WebContents;
}

namespace gfx {
class Rect;
}

namespace android_webview {

// Interface for all the WebView-specific content rendering operations.
// Provides software and hardware rendering and the Capture Picture API.
class BrowserViewRenderer {
 public:
  class Client {
   public:
     // Called to trigger view invalidations.
     virtual void Invalidate() = 0;

     // Called when a new Picture is available. Needs to be enabled
     // via the EnableOnNewPicture method.
     virtual void OnNewPicture(
         const base::android::JavaRef<jobject>& picture) = 0;

     // Called to get view's absolute location on the screen.
     virtual gfx::Point GetLocationOnScreen() = 0;

     // Called when the RenderView page scale changes.
     virtual void OnPageScaleFactorChanged(float page_scale_factor) = 0;

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
        int height) = 0;

    // Draws the provided Java Bitmap into the provided Java Canvas.
    virtual void DrawBitmapIntoCanvas(
        JNIEnv* env,
        const base::android::JavaRef<jobject>& jbitmap,
        const base::android::JavaRef<jobject>& jcanvas) = 0;

    // Creates a Java Picture object that records drawing the provided Bitmap.
    virtual base::android::ScopedJavaLocalRef<jobject> RecordBitmapIntoPicture(
        JNIEnv* env,
        const base::android::JavaRef<jobject>& jbitmap) = 0;

   protected:
    virtual ~JavaHelper() {}
  };

  enum OnNewPictureMode {
    kOnNewPictureDisabled = 0,
    kOnNewPictureEnabled,
    kOnNewPictureInvalidationOnly,
  };

  // Content control methods.
  virtual void SetContents(content::ContentViewCore* content_view_core) = 0;

  // Hardware rendering methods.
  virtual void DrawGL(AwDrawGLInfo* draw_info) = 0;
  virtual void SetScrollForHWFrame(int x, int y) = 0;

  // Software rendering methods.
  virtual bool DrawSW(jobject java_canvas, const gfx::Rect& clip_bounds) = 0;

  // CapturePicture API methods.
  virtual base::android::ScopedJavaLocalRef<jobject> CapturePicture() = 0;
  virtual void EnableOnNewPicture(OnNewPictureMode mode) = 0;

  // View update notifications.
  virtual void OnVisibilityChanged(bool view_visible, bool window_visible) = 0;
  virtual void OnSizeChanged(int width, int height) = 0;
  virtual void OnAttachedToWindow(int width, int height) = 0;
  virtual void OnDetachedFromWindow() = 0;

  // Android views hierarchy gluing.
  virtual bool IsAttachedToWindow() = 0;
  virtual bool IsViewVisible() = 0;
  virtual gfx::Rect GetScreenRect() = 0;

  virtual ~BrowserViewRenderer() {}
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_BROWSER_VIEW_RENDERER_H_
