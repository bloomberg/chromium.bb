// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/android/mojo_view.h"

#include <android/native_window_jni.h>
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "jni/MojoView_jni.h"

namespace mojo {

static jint Init(JNIEnv* env, jclass obj) {
  MojoView* mojo_view = new MojoView(env, obj);
  return reinterpret_cast<jint>(mojo_view);
}

MojoView::MojoView(JNIEnv* env, jobject obj) {
}

MojoView::~MojoView() {
}

void MojoView::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void MojoView::SurfaceCreated(JNIEnv* env, jobject obj, jobject jsurface) {
  base::android::ScopedJavaLocalRef<jobject> protector(env, jsurface);
  DCHECK(jsurface);
  window_ = ANativeWindow_fromSurface(env, jsurface);
  DCHECK(window_);

  scoped_refptr<gfx::GLSurface> surface =
      new gfx::NativeViewGLSurfaceEGL(window_);
  CHECK(surface->Initialize());
}

void MojoView::SurfaceDestroyed(JNIEnv* env, jobject obj) {
  DCHECK(window_);
  ANativeWindow_release(window_);
  window_ = NULL;
}

void MojoView::SurfaceSetSize(
    JNIEnv* env, jobject obj, jint width, jint height) {
}

bool MojoView::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace mojo
