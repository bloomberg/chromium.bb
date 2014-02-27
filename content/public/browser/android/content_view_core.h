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

class SkBitmap;

namespace cc {
class Layer;
}

namespace gfx {
class Size;
class SizeF;
class Vector2dF;
}

namespace ui {
class ViewAndroid;
class WindowAndroid;
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

  // Request a scaled content readback.  The result is passed through the
  // callback.  The boolean parameter indicates whether the readback was a
  // success or not.  The content is passed through the SkBitmap parameter.
  // |out_size| is returned with the size of the content.
  virtual void GetScaledContentBitmap(
      float scale,
      jobject bitmap_config,
      const base::Callback<void(bool, const SkBitmap&)>& result_callback) = 0;
  virtual float GetDpiScale() const = 0;
  virtual void PauseVideo() = 0;
  virtual void PauseOrResumeGeolocation(bool should_pause) = 0;

  // Observer callback for frame metadata updates.
  typedef base::Callback<void(
      const gfx::SizeF& content_size,
      const gfx::Vector2dF& scroll_offset,
      float page_scale_factor)> UpdateFrameInfoCallback;

 protected:
  virtual ~ContentViewCore() {};
};

};  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_CONTENT_VIEW_CORE_H_
