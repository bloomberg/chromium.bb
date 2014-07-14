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
// TODO(boliu): Rename this class to JavaRasterHelper.
class JavaBrowserViewRendererHelper : public BrowserViewRendererJavaHelper {
 public:
  JavaBrowserViewRendererHelper();
  virtual ~JavaBrowserViewRendererHelper();

  static void SetAwDrawSWFunctionTable(AwDrawSWFunctionTable* table);
  static JavaBrowserViewRendererHelper* GetInstance();

  // BrowserViewRendererJavaHelper implementation.
  virtual bool RenderViaAuxilaryBitmapIfNeeded(
      jobject java_canvas,
      const gfx::Vector2d& scroll_correction,
      const gfx::Rect& auxiliary_bitmap_rect,
      RenderMethod render_source) OVERRIDE;

 private:
  bool RenderViaAuxilaryBitmap(JNIEnv* env,
                               jobject java_canvas,
                               const gfx::Vector2d& scroll_correction,
                               const gfx::Rect& auxiliary_bitmap_rect,
                               const RenderMethod& render_source);
  bool RasterizeIntoBitmap(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jbitmap,
      int scroll_x,
      int scroll_y,
      const JavaBrowserViewRendererHelper::RenderMethod& renderer);
};

bool RegisterJavaBrowserViewRendererHelper(JNIEnv* env);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_JAVA_BROWSER_VIEW_RENDERER_HELPER_H_
