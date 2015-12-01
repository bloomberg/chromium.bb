// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/android/blimp_view.h"

#include <android/native_window_jni.h>

#include "blimp/client/compositor/blimp_compositor_android.h"
#include "jni/BlimpView_jni.h"
#include "ui/gfx/geometry/size.h"

namespace blimp {

// static
static jlong Init(JNIEnv* env,
                  const JavaParamRef<jobject>& jobj,
                  jint real_width,
                  jint real_height,
                  jint width,
                  jint height,
                  jfloat dp_to_px) {
  return reinterpret_cast<intptr_t>(
      new BlimpView(env, jobj, gfx::Size(real_width, real_height),
                    gfx::Size(width, height), dp_to_px));
}

// static
bool BlimpView::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

BlimpView::BlimpView(JNIEnv* env,
                     const JavaParamRef<jobject>& jobj,
                     const gfx::Size& real_size,
                     const gfx::Size& size,
                     float dp_to_px)
    : compositor_(BlimpCompositorAndroid::Create(real_size, size, dp_to_px)),
      current_surface_format_(0),
      window_(gfx::kNullAcceleratedWidget) {
  java_obj_.Reset(env, jobj);
}

BlimpView::~BlimpView() {
  ReleaseAcceleratedWidget();
}

void BlimpView::Destroy(JNIEnv* env, jobject jobj) {
  delete this;
}

void BlimpView::SetNeedsComposite(JNIEnv* env, jobject jobj) {}

void BlimpView::OnSurfaceChanged(JNIEnv* env,
                                 jobject jobj,
                                 jint format,
                                 jint width,
                                 jint height,
                                 jobject jsurface) {
  if (current_surface_format_ != format) {
    current_surface_format_ = format;
    ReleaseAcceleratedWidget();

    if (jsurface) {
      base::android::ScopedJavaLocalFrame scoped_local_reference_frame(env);
      window_ = ANativeWindow_fromSurface(env, jsurface);
      compositor_->SetAcceleratedWidget(window_);
      compositor_->SetVisible(true);
    }
  }

  compositor_->SetSize(gfx::Size(width, height));
}

void BlimpView::OnSurfaceCreated(JNIEnv* env, jobject jobj) {
  current_surface_format_ = 0 /** PixelFormat.UNKNOWN */;
}

void BlimpView::OnSurfaceDestroyed(JNIEnv* env, jobject jobj) {
  current_surface_format_ = 0 /** PixelFormat.UNKNOWN */;
  ReleaseAcceleratedWidget();
}

void BlimpView::SetVisibility(JNIEnv* env, jobject jobj, jboolean visible) {
  compositor_->SetVisible(visible);
}

void BlimpView::ReleaseAcceleratedWidget() {
  if (window_ == gfx::kNullAcceleratedWidget)
    return;

  compositor_->ReleaseAcceleratedWidget();
  ANativeWindow_release(window_);
  window_ = gfx::kNullAcceleratedWidget;
}

}  // namespace blimp
