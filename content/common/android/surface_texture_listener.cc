// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/surface_texture_listener.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "content/common/android/surface_texture_bridge.h"
#include "jni/surface_texture_listener_jni.h"

namespace content {

// static
jobject SurfaceTextureListener::CreateSurfaceTextureListener(
    JNIEnv* env,
    const base::Closure& callback) {
  // The java listener object owns and releases the native instance.
  // This is necessary to avoid races with incoming notifications.
  ScopedJavaLocalRef<jobject> listener(Java_SurfaceTextureListener_create(env,
      reinterpret_cast<int>(new SurfaceTextureListener(callback))));

  DCHECK(!listener.is_null());
  return listener.Release();
}

SurfaceTextureListener::SurfaceTextureListener(const base::Closure& callback)
    : callback_(callback),
      browser_loop_(base::MessageLoopProxy::current()) {
}

SurfaceTextureListener::~SurfaceTextureListener() {
}

void SurfaceTextureListener::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void SurfaceTextureListener::FrameAvailable(JNIEnv* env, jobject obj) {
  // These notifications should be coming in on a thread private to Java.
  // Should this ever change, we can try to avoid reposting to the same thread.
  DCHECK(!browser_loop_->BelongsToCurrentThread());
  browser_loop_->PostTask(FROM_HERE, callback_);
}

// static
bool SurfaceTextureListener::RegisterSurfaceTextureListener(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
