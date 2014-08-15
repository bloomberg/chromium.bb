// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/platform_viewport_android.h"

#include <android/input.h>
#include <android/native_window_jni.h>

#include "base/android/jni_android.h"
#include "jni/PlatformViewportAndroid_jni.h"
#include "ui/events/event.h"
#include "ui/gfx/point.h"

namespace mojo {

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

////////////////////////////////////////////////////////////////////////////////
// PlatformViewportAndroid, public:

// static
bool PlatformViewportAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

PlatformViewportAndroid::PlatformViewportAndroid(Delegate* delegate)
    : delegate_(delegate),
      window_(NULL),
      id_generator_(0),
      weak_factory_(this) {
}

PlatformViewportAndroid::~PlatformViewportAndroid() {
  if (window_)
    ReleaseWindow();
}

void PlatformViewportAndroid::Destroy(JNIEnv* env, jobject obj) {
  delegate_->OnDestroyed();
}

void PlatformViewportAndroid::SurfaceCreated(JNIEnv* env,
                                             jobject obj,
                                             jobject jsurface) {
  base::android::ScopedJavaLocalRef<jobject> protector(env, jsurface);
  // Note: This ensures that any local references used by
  // ANativeWindow_fromSurface are released immediately. This is needed as a
  // workaround for https://code.google.com/p/android/issues/detail?id=68174
  {
    base::android::ScopedJavaLocalFrame scoped_local_reference_frame(env);
    window_ = ANativeWindow_fromSurface(env, jsurface);
  }
  delegate_->OnAcceleratedWidgetAvailable(window_);
}

void PlatformViewportAndroid::SurfaceDestroyed(JNIEnv* env, jobject obj) {
  DCHECK(window_);
  ReleaseWindow();
}

void PlatformViewportAndroid::SurfaceSetSize(JNIEnv* env, jobject obj,
                                             jint width, jint height) {
  bounds_ = gfx::Rect(width, height);
  delegate_->OnBoundsChanged(bounds_);
}

bool PlatformViewportAndroid::TouchEvent(JNIEnv* env, jobject obj,
                                         jint pointer_id,
                                         jint action,
                                         jfloat x, jfloat y,
                                         jlong time_ms) {
  gfx::Point location(static_cast<int>(x), static_cast<int>(y));
  ui::TouchEvent event(MotionEventActionToEventType(action), location,
                       id_generator_.GetGeneratedID(pointer_id),
                       base::TimeDelta::FromMilliseconds(time_ms));
  // TODO(beng): handle multiple touch-points.
  delegate_->OnEvent(&event);
  if (action == ui::ET_TOUCH_RELEASED)
    id_generator_.ReleaseNumber(pointer_id);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// PlatformViewportAndroid, PlatformViewport implementation:

void PlatformViewportAndroid::Init(const gfx::Rect& bounds) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PlatformViewportAndroid_createForActivity(
      env,
      base::android::GetApplicationContext(),
      reinterpret_cast<jlong>(this));
}

void PlatformViewportAndroid::Show() {
  // Nothing to do. View is created visible.
}

void PlatformViewportAndroid::Hide() {
  // Nothing to do. View is always visible.
}

void PlatformViewportAndroid::Close() {
  // TODO(beng): close activity containing MojoView?

  // TODO(beng): perform this in response to view destruction.
  delegate_->OnDestroyed();
}

gfx::Size PlatformViewportAndroid::GetSize() {
  return bounds_.size();
}

void PlatformViewportAndroid::SetBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

void PlatformViewportAndroid::SetCapture() {
  NOTIMPLEMENTED();
}

void PlatformViewportAndroid::ReleaseCapture() {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// PlatformViewportAndroid, private:

void PlatformViewportAndroid::ReleaseWindow() {
  ANativeWindow_release(window_);
  window_ = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// PlatformViewport, public:

// static
scoped_ptr<PlatformViewport> PlatformViewport::Create(Delegate* delegate) {
  return scoped_ptr<PlatformViewport>(
      new PlatformViewportAndroid(delegate)).Pass();
}

}  // namespace mojo
