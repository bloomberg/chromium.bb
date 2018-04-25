// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CORE_H_
#define CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CORE_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"

namespace ui {
class ViewAndroid;
class WindowAndroid;
}

namespace content {

class RenderWidgetHostViewAndroid;
class WebContentsImpl;

class ContentViewCore : public WebContentsObserver {
 public:
  ContentViewCore(JNIEnv* env,
                  const base::android::JavaRef<jobject>& obj,
                  WebContents* web_contents,
                  float dpi_scale);

  ~ContentViewCore() override;

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------

  void UpdateWindowAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jwindow_android);
  void OnJavaContentViewCoreDestroyed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void SendOrientationChangeEvent(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint orientation);

  void SetFocus(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj,
                jboolean focused);

  void SetDIPScale(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   jfloat dipScale);

 private:

  // WebContentsObserver implementation.
  void RenderViewReady() override;
  void RenderViewHostChanged(RenderViewHost* old_host,
                             RenderViewHost* new_host) override;
  void WebContentsDestroyed() override;

  // --------------------------------------------------------------------------
  // Other private methods and data
  // --------------------------------------------------------------------------

  void SendScreenRectsAndResizeWidget();

  ui::ViewAndroid* GetViewAndroid() const;
  ui::WindowAndroid* GetWindowAndroid() const;

  RenderWidgetHostViewAndroid* GetRenderWidgetHostViewAndroid() const;

  // Update focus state of the RenderWidgetHostView.
  void SetFocusInternal(bool focused);

  // Send device_orientation_ to renderer.
  void SendOrientationChangeEventInternal();

  // A weak reference to the Java ContentViewCore object.
  JavaObjectWeakGlobalRef java_ref_;

  // Reference to the current WebContents used to determine how and what to
  // display in the ContentViewCore.
  WebContentsImpl* web_contents_;

  // Device scale factor.
  float dpi_scale_;

  // The cache of device's current orientation set from Java side, this value
  // will be sent to Renderer once it is ready.
  int device_orientation_;

  DISALLOW_COPY_AND_ASSIGN(ContentViewCore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CORE_H_
