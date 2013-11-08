// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/android/mojo_viewport.h"

#include <android/input.h>
#include <android/native_window_jni.h>

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "jni/MojoViewport_jni.h"

namespace mojo {
namespace services {

ui::EventType MotionEventActionToEventType(jint action) {
  switch (action) {
    case AMOTION_EVENT_ACTION_DOWN:
      return ui::ET_TOUCH_PRESSED;
    case AMOTION_EVENT_ACTION_MOVE:
      return ui::ET_TOUCH_MOVED;
    case AMOTION_EVENT_ACTION_UP:
      return ui::ET_TOUCH_RELEASED;
    default:
      NOTREACHED();
  }
  return ui::ET_UNKNOWN;
}

MojoViewportInit::MojoViewportInit() {
}

MojoViewportInit::~MojoViewportInit() {
}

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
  Java_MojoViewport_createForActivity(
      env, activity, reinterpret_cast<jint>(init));
}

void MojoViewport::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void MojoViewport::SurfaceCreated(JNIEnv* env, jobject obj, jobject jsurface) {
  base::android::ScopedJavaLocalRef<jobject> protector(env, jsurface);
  ANativeWindow* window = ANativeWindow_fromSurface(env, jsurface);

  ui_runner_->PostTask(FROM_HERE, base::Bind(
      &NativeViewportAndroid::OnNativeWindowCreated,
      native_viewport_,
      window));
}

void MojoViewport::SurfaceDestroyed(JNIEnv* env, jobject obj) {
  ui_runner_->PostTask(FROM_HERE, base::Bind(
      &NativeViewportAndroid::OnNativeWindowDestroyed, native_viewport_));
}

void MojoViewport::SurfaceSetSize(
    JNIEnv* env, jobject obj, jint width, jint height) {
  ui_runner_->PostTask(FROM_HERE, base::Bind(
      &NativeViewportAndroid::OnResized,
      native_viewport_,
      gfx::Size(width, height)));
}

bool MojoViewport::TouchEvent(JNIEnv* env, jobject obj,
                              jint pointer_id,
                              jint action,
                              jfloat x, jfloat y,
                              jlong time_ms) {
  ui_runner_->PostTask(FROM_HERE, base::Bind(
      &NativeViewportAndroid::OnTouchEvent,
      native_viewport_,
      pointer_id,
      MotionEventActionToEventType(action),
      x, y,
      time_ms));
  // TODO(beng): This type needs to live on the main thread so we can respond to
  //             this question truthfully.
  return true;
}

bool MojoViewport::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace services
}  // namespace mojo
