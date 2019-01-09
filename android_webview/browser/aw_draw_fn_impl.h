// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_DRAW_FN_IMPL_H_
#define ANDROID_WEBVIEW_BROWSER_AW_DRAW_FN_IMPL_H_

#include "android_webview/browser/compositor_frame_consumer.h"
#include "android_webview/browser/render_thread_manager.h"
#include "android_webview/public/browser/draw_fn.h"
#include "base/android/jni_weak_ref.h"

namespace android_webview {

class AwDrawFnImpl {
 public:
  AwDrawFnImpl();
  ~AwDrawFnImpl();

  void ReleaseHandle(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);
  jint GetFunctorHandle(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);
  jlong GetCompositorFrameConsumer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  int functor_handle() { return functor_handle_; }
  void OnSync(AwDrawFn_OnSyncParams* params);
  void OnContextDestroyed();
  void DrawGL(AwDrawFn_DrawGLParams* params);
  void InitVk(AwDrawFn_InitVkParams* params);
  void DrawVk(AwDrawFn_DrawVkParams* params);
  void PostDrawVk(AwDrawFn_PostDrawVkParams* params);

 private:
  CompositorFrameConsumer* GetCompositorFrameConsumer() {
    return &render_thread_manager_;
  }

  int functor_handle_;
  RenderThreadManager render_thread_manager_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_DRAW_FN_IMPL_H_
