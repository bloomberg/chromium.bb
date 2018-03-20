// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CORE_H_
#define CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CORE_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/process/process.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/android/view_android.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/selection_bound.h"
#include "url/gurl.h"

namespace ui {
class WindowAndroid;
}

namespace content {

class RenderWidgetHostViewAndroid;

class ContentViewCore : public WebContentsObserver {
 public:
  ContentViewCore(JNIEnv* env,
                  const base::android::JavaRef<jobject>& obj,
                  WebContents* web_contents,
                  float dpi_scale);

  ~ContentViewCore() override;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();
  WebContents* GetWebContents() const;
  ui::WindowAndroid* GetWindowAndroid() const;

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------

  base::android::ScopedJavaLocalRef<jobject> GetJavaWindowAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void UpdateWindowAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jwindow_android);
  void OnJavaContentViewCoreDestroyed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // Returns the amount of the top controls height if controls are in the state
  // of shrinking Blink's view size, otherwise 0.
  int GetTopControlsShrinkBlinkHeightPixForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void SendOrientationChangeEvent(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint orientation);

  void ResetGestureDetection(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj);
  void SetDoubleTapSupportEnabled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean enabled);
  void SetMultiTouchZoomSupportEnabled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean enabled);

  void SetFocus(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj,
                jboolean focused);

  void SetDIPScale(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   jfloat dipScale);

  void SetTextTrackSettings(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean textTracksEnabled,
      const base::android::JavaParamRef<jstring>& textTrackBackgroundColor,
      const base::android::JavaParamRef<jstring>& textTrackFontFamily,
      const base::android::JavaParamRef<jstring>& textTrackFontStyle,
      const base::android::JavaParamRef<jstring>& textTrackFontVariant,
      const base::android::JavaParamRef<jstring>& textTrackTextColor,
      const base::android::JavaParamRef<jstring>& textTrackTextShadow,
      const base::android::JavaParamRef<jstring>& textTrackTextSize);

  jboolean UsingSynchronousCompositing(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // --------------------------------------------------------------------------
  // Methods called from native code
  // --------------------------------------------------------------------------

  void OnTouchDown(const base::android::ScopedJavaLocalRef<jobject>& event);

  ui::ViewAndroid* GetViewAndroid() const;

 private:
  class ContentViewUserData;

  friend class ContentViewUserData;

  // WebContentsObserver implementation.
  void RenderViewReady() override;
  void RenderViewHostChanged(RenderViewHost* old_host,
                             RenderViewHost* new_host) override;

  // --------------------------------------------------------------------------
  // Other private methods and data
  // --------------------------------------------------------------------------

  void InitWebContents();
  void SendScreenRectsAndResizeWidget();

  RenderWidgetHostViewAndroid* GetRenderWidgetHostViewAndroid() const;

  // Update focus state of the RenderWidgetHostView.
  void SetFocusInternal(bool focused);

  // Send device_orientation_ to renderer.
  void SendOrientationChangeEventInternal();

  float dpi_scale() const { return dpi_scale_; }

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
