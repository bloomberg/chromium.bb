// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/android/mojo_viewport.h"

#include <android/native_window_jni.h>
#include "base/android/jni_android.h"
#include "jni/MojoViewport_jni.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/scoped_make_current.h"

namespace mojo {
namespace services {

static jint Init(JNIEnv* env, jclass obj, jint jinit) {
  MojoViewportInit* init = reinterpret_cast<MojoViewportInit*>(jinit);
  MojoViewport* viewport = new MojoViewport(init);
  return reinterpret_cast<jint>(viewport);
}

MojoViewport::MojoViewport(MojoViewportInit* init)
    : ui_runner_(init->ui_runner),
      native_viewport_(init->native_viewport) {
  delete init;
}

MojoViewport::~MojoViewport() {
}

void MojoViewport::CreateForActivity(
    jobject activity,
    MojoViewportInit* init) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MojoViewport_CreateForActivity(
      env, activity, reinterpret_cast<jint>(init));
}

void MojoViewport::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void MojoViewport::SurfaceCreated(JNIEnv* env, jobject obj, jobject jsurface) {
  base::android::ScopedJavaLocalRef<jobject> protector(env, jsurface);
  DCHECK(jsurface);
  window_ = ANativeWindow_fromSurface(env, jsurface);
  DCHECK(window_);

  surface_ = new gfx::NativeViewGLSurfaceEGL(window_);
  CHECK(surface_->Initialize());

  gl_context_ = gfx::GLContext::CreateGLContext(
      0, surface_.get(), gfx::PreferDiscreteGpu);
  ui::ScopedMakeCurrent make_current(gl_context_.get(), surface_.get());
  glClearColor(0, 1, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);
  surface_->SwapBuffers();
}

void MojoViewport::SurfaceDestroyed(JNIEnv* env, jobject obj) {
  DCHECK(window_);
  ANativeWindow_release(window_);
  window_ = NULL;
}

void MojoViewport::SurfaceSetSize(
    JNIEnv* env, jobject obj, jint width, jint height) {
}

bool MojoViewport::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace services
}  // namespace mojo
