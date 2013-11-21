// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_CONTENT_VIEW_RENDER_VIEW_H_
#define CONTENT_BROWSER_ANDROID_CONTENT_VIEW_RENDER_VIEW_H_

#include "base/android/jni_helper.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/android/compositor_client.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class Compositor;

class ContentViewRenderView : public CompositorClient {
 public:
  // Registers the JNI methods for ContentViewRender.
  static bool RegisterContentViewRenderView(JNIEnv* env);

  ContentViewRenderView(JNIEnv* env,
                        jobject obj,
                        gfx::NativeWindow root_window);

  // Methods called from Java via JNI -----------------------------------------
  void Destroy(JNIEnv* env, jobject obj);
  void SetCurrentContentView(JNIEnv* env, jobject obj, int native_content_view);
  void SurfaceCreated(JNIEnv* env, jobject obj, jobject jsurface);
  void SurfaceDestroyed(JNIEnv* env, jobject obj);
  void SurfaceSetSize(JNIEnv* env, jobject obj, jint width, jint height);
  jboolean Composite(JNIEnv* env, jobject obj);
  jboolean CompositeToBitmap(JNIEnv* env, jobject obj, jobject java_bitmap);

  // CompositorClient ---------------------------------------------------------
  virtual void ScheduleComposite() OVERRIDE;
  virtual void OnSwapBuffersPosted() OVERRIDE;
  virtual void OnSwapBuffersCompleted() OVERRIDE;

 private:
  virtual ~ContentViewRenderView();

  void InitCompositor();

  bool buffers_swapped_during_composite_;

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  scoped_ptr<content::Compositor> compositor_;

  gfx::NativeWindow root_window_;

  DISALLOW_COPY_AND_ASSIGN(ContentViewRenderView);
};



}

#endif  // CONTENT_BROWSER_ANDROID_CONTENT_VIEW_RENDER_VIEW_H_
