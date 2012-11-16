// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_CONTENT_VIEW_RENDER_VIEW_H_
#define CONTENT_BROWSER_ANDROID_CONTENT_VIEW_RENDER_VIEW_H_

#include <jni.h>

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/android/compositor.h"

namespace content {

class ContentViewRenderView : public Compositor::Client {
 public:
  // Registers the JNI methods for ContentViewRender.
  static bool RegisterContentViewRenderView(JNIEnv* env);

  ContentViewRenderView();

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------
  static jint Init(JNIEnv* env, jclass clazz);
  void Destroy(JNIEnv* env, jobject obj);
  void SetCurrentContentView(JNIEnv* env, jobject obj, int native_content_view);
  void SurfaceCreated(JNIEnv* env, jobject obj, jobject jsurface);
  void SurfaceDestroyed(JNIEnv* env, jobject obj);
  void SurfaceSetSize(JNIEnv* env, jobject obj, jint width, jint height);

 private:
  friend class base::RefCounted<ContentViewRenderView>;
  virtual ~ContentViewRenderView();

  // Compositor::Client implementation.
  virtual void ScheduleComposite() OVERRIDE;

  void InitCompositor();
  void Composite();

  scoped_ptr<content::Compositor> compositor_;
  bool scheduled_composite_;

  base::WeakPtrFactory<ContentViewRenderView> weak_factory_;

  // Note that this class does not call back to Java and as a result does not
  // have a reference to its Java object.

  DISALLOW_COPY_AND_ASSIGN(ContentViewRenderView);
};



}

#endif  // CONTENT_BROWSER_ANDROID_CONTENT_VIEW_RENDER_VIEW_H_
