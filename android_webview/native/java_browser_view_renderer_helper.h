// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_JAVA_BROWSER_VIEW_RENDERER_HELPER_H_
#define ANDROID_WEBVIEW_NATIVE_JAVA_BROWSER_VIEW_RENDERER_HELPER_H_

#include "android_webview/browser/browser_view_renderer.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"

namespace android_webview {

// Native side of java-class of same name.
// Provides utility methods for rendering involving with Java objects.
class JavaBrowserViewRendererHelper : public BrowserViewRenderer::JavaHelper {
 public:
  JavaBrowserViewRendererHelper();
  virtual ~JavaBrowserViewRendererHelper();

  // BrowserViewRenderer::JavaHelper implementation.
  virtual base::android::ScopedJavaLocalRef<jobject> CreateBitmap(
      JNIEnv* env,
      int width,
      int height) OVERRIDE;
  virtual void DrawBitmapIntoCanvas(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jbitmap,
      const base::android::JavaRef<jobject>& jcanvas) OVERRIDE;
  virtual base::android::ScopedJavaLocalRef<jobject> RecordBitmapIntoPicture(
     JNIEnv* env,
     const base::android::JavaRef<jobject>& jbitmap) OVERRIDE;
};

bool RegisterJavaBrowserViewRendererHelper(JNIEnv* env);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_JAVA_BROWSER_VIEW_RENDERER_HELPER_H_
