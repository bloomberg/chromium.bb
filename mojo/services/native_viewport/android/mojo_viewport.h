// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NATIVE_VIEWPORT_ANDROID_MOJO_VIEWPORT_H_
#define MOJO_SERVICES_NATIVE_VIEWPORT_ANDROID_MOJO_VIEWPORT_H_

#include "base/android/jni_helper.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "mojo/services/native_viewport/native_viewport_android.h"
#include "ui/gl/gl_surface_egl.h"

struct ANativeWindow;

namespace mojo {
namespace services {

struct MojoViewportInit {
  scoped_refptr<base::SingleThreadTaskRunner> ui_runner;
  base::WeakPtr<NativeViewportAndroid> native_viewport;
};

class MojoViewport {
 public:
  static bool Register(JNIEnv* env);

  static void CreateForActivity(
    jobject activity, MojoViewportInit* init);

  explicit MojoViewport(MojoViewportInit* init);

  void Destroy(JNIEnv* env, jobject obj);
  void SurfaceCreated(JNIEnv* env, jobject obj, jobject jsurface);
  void SurfaceDestroyed(JNIEnv* env, jobject obj);
  void SurfaceSetSize(JNIEnv* env, jobject obj, jint width, jint height);

 private:
  ~MojoViewport();

  scoped_refptr<base::SingleThreadTaskRunner> ui_runner_;
  base::WeakPtr<NativeViewportAndroid> native_viewport_;

  ANativeWindow* window_;
  scoped_refptr<gfx::GLSurface> surface_;
  scoped_refptr<gfx::GLContext> gl_context_;

  DISALLOW_COPY_AND_ASSIGN(MojoViewport);
};

}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_NATIVE_VIEWPORT_ANDROID_MOJO_VIEWPORT_H_
