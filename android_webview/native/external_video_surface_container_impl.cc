// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/external_video_surface_container_impl.h"

#include "base/android/jni_android.h"
#include "content/public/browser/android/content_view_core.h"
#include "jni/ExternalVideoSurfaceContainer_jni.h"
#include "ui/gfx/rect_f.h"

using base::android::AttachCurrentThread;
using content::ContentViewCore;

namespace android_webview {

ExternalVideoSurfaceContainerImpl::ExternalVideoSurfaceContainerImpl(
    content::WebContents* web_contents) {
  ContentViewCore* cvc = ContentViewCore::FromWebContents(web_contents);
  if (cvc) {
    JNIEnv* env = AttachCurrentThread();
    jobject_.Reset(
        Java_ExternalVideoSurfaceContainer_create(
            env, reinterpret_cast<intptr_t>(this), cvc->GetJavaObject().obj()));
  }
}

ExternalVideoSurfaceContainerImpl::~ExternalVideoSurfaceContainerImpl() {
  JNIEnv* env = AttachCurrentThread();
  Java_ExternalVideoSurfaceContainer_destroy(env, jobject_.obj());
  jobject_.Reset();
}

void ExternalVideoSurfaceContainerImpl::RequestExternalVideoSurface(
    int player_id,
    const SurfaceCreatedCB& surface_created_cb,
    const SurfaceDestroyedCB& surface_destroyed_cb) {
  surface_created_cb_ = surface_created_cb;
  surface_destroyed_cb_ = surface_destroyed_cb;

  JNIEnv* env = AttachCurrentThread();
  Java_ExternalVideoSurfaceContainer_requestExternalVideoSurface(
      env, jobject_.obj(), static_cast<jint>(player_id));
}

void ExternalVideoSurfaceContainerImpl::ReleaseExternalVideoSurface(
    int player_id) {
  JNIEnv* env = AttachCurrentThread();
  Java_ExternalVideoSurfaceContainer_releaseExternalVideoSurface(
      env, jobject_.obj(), static_cast<jint>(player_id));

  surface_created_cb_.Reset();
  surface_destroyed_cb_.Reset();
}

void ExternalVideoSurfaceContainerImpl::OnFrameInfoUpdated() {
  JNIEnv* env = AttachCurrentThread();
  Java_ExternalVideoSurfaceContainer_onFrameInfoUpdated(env, jobject_.obj());
}

void ExternalVideoSurfaceContainerImpl::OnExternalVideoSurfacePositionChanged(
    int player_id, const gfx::RectF& rect) {
  JNIEnv* env = AttachCurrentThread();
  Java_ExternalVideoSurfaceContainer_onExternalVideoSurfacePositionChanged(
      env,
      jobject_.obj(),
      static_cast<jint>(player_id),
      static_cast<jfloat>(rect.x()),
      static_cast<jfloat>(rect.y()),
      static_cast<jfloat>(rect.x() + rect.width()),
      static_cast<jfloat>(rect.y() + rect.height()));
}

// Methods called from Java.
void ExternalVideoSurfaceContainerImpl::SurfaceCreated(
    JNIEnv* env, jobject obj, jint player_id, jobject jsurface) {
  if (!surface_created_cb_.is_null())
    surface_created_cb_.Run(static_cast<int>(player_id), jsurface);
}

void ExternalVideoSurfaceContainerImpl::SurfaceDestroyed(
    JNIEnv* env, jobject obj, jint player_id) {
  if (!surface_destroyed_cb_.is_null())
    surface_destroyed_cb_.Run(static_cast<int>(player_id));
}

bool RegisterExternalVideoSurfaceContainer(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android_webview
