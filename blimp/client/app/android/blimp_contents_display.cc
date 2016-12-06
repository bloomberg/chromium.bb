// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/app/android/blimp_contents_display.h"

#include <android/native_window_jni.h>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "blimp/client/app/android/blimp_environment.h"
#include "blimp/client/app/compositor/browser_compositor.h"
#include "blimp/client/public/compositor/compositor_dependencies.h"
#include "blimp/client/public/contents/blimp_contents.h"
#include "blimp/client/public/contents/blimp_contents_view.h"
#include "blimp/client/support/compositor/compositor_dependencies_impl.h"
#include "jni/BlimpContentsDisplay_jni.h"
#include "ui/android/view_android.h"
#include "ui/events/android/motion_event_android.h"
#include "ui/gfx/geometry/size.h"

namespace blimp {
namespace client {
namespace app {

static jlong Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj,
    const base::android::JavaParamRef<jobject>& jblimp_environment,
    const base::android::JavaParamRef<jobject>& jblimp_contents,
    jint real_width,
    jint real_height,
    jint width,
    jint height) {
  BlimpEnvironment* blimp_environment =
      BlimpEnvironment::FromJavaObject(env, jblimp_environment);
  BlimpContents* blimp_contents =
      BlimpContents::FromJavaObject(env, jblimp_contents);
  return reinterpret_cast<intptr_t>(new BlimpContentsDisplay(
      env, jobj, gfx::Size(real_width, real_height), gfx::Size(width, height),
      blimp_environment, blimp_contents));
}

// static
bool BlimpContentsDisplay::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

BlimpContentsDisplay::BlimpContentsDisplay(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj,
    const gfx::Size& real_size,
    const gfx::Size& size,
    BlimpEnvironment* blimp_environment,
    BlimpContents* blimp_contents)
    : blimp_contents_(blimp_contents),
      current_surface_format_(0),
      window_(gfx::kNullAcceleratedWidget),
      weak_ptr_factory_(this) {
  DCHECK(blimp_contents_);

  compositor_dependencies_ = blimp_environment->CreateCompositorDepencencies();

  compositor_ =
      base::MakeUnique<BrowserCompositor>(compositor_dependencies_.get());
  compositor_->set_did_complete_swap_buffers_callback(
      base::Bind(&BlimpContentsDisplay::OnSwapBuffersCompleted,
                 weak_ptr_factory_.GetWeakPtr()));

  compositor_->SetContentLayer(
      blimp_contents_->GetView()->GetNativeView()->GetLayer());

  java_obj_.Reset(env, jobj);
}

BlimpContentsDisplay::~BlimpContentsDisplay() {
  SetSurface(nullptr);

  // Destroy the BrowserCompositor before the BlimpCompositorDependencies.
  compositor_.reset();
  compositor_dependencies_.reset();
}

void BlimpContentsDisplay::Destroy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj) {
  delete this;
}

void BlimpContentsDisplay::OnContentAreaSizeChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj,
    jint width,
    jint height,
    jfloat dpToPx) {
  compositor_->SetSize(gfx::Size(width, height));
}

void BlimpContentsDisplay::OnSurfaceChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj,
    jint format,
    jint width,
    jint height,
    const base::android::JavaParamRef<jobject>& jsurface) {
  if (current_surface_format_ == format) {
    return;
  }

  current_surface_format_ = format;
  SetSurface(nullptr);

  if (jsurface) {
    SetSurface(jsurface);
  }
}

void BlimpContentsDisplay::OnSurfaceCreated(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj) {
  current_surface_format_ = 0 /** PixelFormat.UNKNOWN */;
}

void BlimpContentsDisplay::OnSurfaceDestroyed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj) {
  current_surface_format_ = 0 /** PixelFormat.UNKNOWN */;
  SetSurface(nullptr);
}

void BlimpContentsDisplay::SetSurface(jobject surface) {
  JNIEnv* env = base::android::AttachCurrentThread();
  // Release all references to the old surface.
  if (window_ != gfx::kNullAcceleratedWidget) {
    compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
    blimp_contents_->Hide();
    ANativeWindow_release(window_);
    window_ = gfx::kNullAcceleratedWidget;
  }

  if (surface) {
    base::android::ScopedJavaLocalFrame scoped_local_reference_frame(env);
    window_ = ANativeWindow_fromSurface(env, surface);
    compositor_->SetAcceleratedWidget(window_);
    blimp_contents_->Show();
  }
}

void BlimpContentsDisplay::OnSwapBuffersCompleted() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BlimpContentsDisplay_onSwapBuffersCompleted(env, java_obj_);
}

}  // namespace app
}  // namespace client
}  // namespace blimp
