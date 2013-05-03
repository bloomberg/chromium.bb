// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_CONTENT_VIEW_CORE_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_CONTENT_VIEW_CORE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_controller.h"

namespace cc {
class Layer;
}

namespace gfx {
class Rect;
class Size;
class SizeF;
class Vector2dF;
}

namespace ui {
class ViewAndroid;
class WindowAndroid;
}

namespace WebKit {
class WebCompositorInputHandler;
}

namespace content {
class WebContents;

// Native side of the ContentViewCore.java, which is the primary way of
// communicating with the native Chromium code on Android.  This is a
// public interface used by native code outside of the content module.
class CONTENT_EXPORT ContentViewCore {
 public:
  // Returns the existing ContentViewCore for |web_contents|, or NULL.
  static ContentViewCore* FromWebContents(WebContents* web_contents);
  static ContentViewCore* GetNativeContentViewCore(JNIEnv* env, jobject obj);

  virtual WebContents* GetWebContents() const = 0;
  virtual base::android::ScopedJavaLocalRef<jobject> GetJavaObject() = 0;
  virtual ui::ViewAndroid* GetViewAndroid() const = 0;
  virtual ui::WindowAndroid* GetWindowAndroid() const = 0;
  virtual scoped_refptr<cc::Layer> GetLayer() const = 0;
  virtual void LoadUrl(NavigationController::LoadURLParams& params) = 0;
  virtual jint GetCurrentRenderProcessId(JNIEnv* env, jobject obj) = 0;
  virtual void ShowPastePopup(int x, int y) = 0;
  virtual unsigned int GetScaledContentTexture(
      float scale,
      gfx::Size* out_size) = 0;
  virtual float GetDpiScale() const = 0;
  virtual void SetInputHandler(
      WebKit::WebCompositorInputHandler* input_handler) = 0;
  virtual void RequestContentClipping(const gfx::Rect& clipping,
                                      const gfx::Size& content_size) = 0;

  // Observer callback for frame metadata updates.
  typedef base::Callback<void(
      const gfx::SizeF& content_size,
      const gfx::Vector2dF& scroll_offset,
      float page_scale_factor)> UpdateFrameInfoCallback;

  virtual void AddFrameInfoCallback(
      const UpdateFrameInfoCallback& callback) = 0;
  virtual void RemoveFrameInfoCallback(
      const UpdateFrameInfoCallback& callback) = 0;

 protected:
  virtual ~ContentViewCore() {};
};

};  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_CONTENT_VIEW_CORE_H_
