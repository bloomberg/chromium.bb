// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_POPUP_ZOOMER_H_
#define CONTENT_BROWSER_ANDROID_POPUP_ZOOMER_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "content/browser/android/render_widget_host_connector.h"
#include "ui/gfx/geometry/rect_f.h"

namespace content {

class RenderWidgetHostViewAndroid;

class PopupZoomer : public RenderWidgetHostConnector {
 public:
  PopupZoomer(JNIEnv* env,
              const base::android::JavaParamRef<jobject>& obj,
              WebContents* web_contents);

  // RendetWidgetHostConnector implementation.
  void UpdateRenderProcessConnection(
      RenderWidgetHostViewAndroid* old_rwhva,
      RenderWidgetHostViewAndroid* new_rhwva) override;

  // Shows the disambiguation popup
  // |rect_pixels|   --> window coordinates which |zoomed_bitmap| represents
  // |zoomed_bitmap| --> magnified image of potential touch targets
  void ResolveTapDisambiguation(JNIEnv* env,
                                const base::android::JavaParamRef<jobject>& obj,
                                jlong time_ms,
                                jfloat x,
                                jfloat y,
                                jboolean is_long_press);

  // Called from native -> java

  // Shows the disambiguation popup
  // |rect_pixels|   --> window coordinates which |zoomed_bitmap| represents
  // |zoomed_bitmap| --> magnified image of potential touch targets
  void ShowPopup(const gfx::Rect& rect_pixels, const SkBitmap& zoomed_bitmap);
  void HidePopup();

 private:
  ~PopupZoomer() override;

  // Current RenderWidgetHostView connected to this instance. Can be null.
  RenderWidgetHostViewAndroid* rwhva_;
  JavaObjectWeakGlobalRef java_obj_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_POPUP_ZOOMER_H_
