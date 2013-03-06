// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/scoped_java_surface.h"

#include "base/logging.h"
#include "content/common/android/surface_texture_bridge.h"
#include "jni/Surface_jni.h"

namespace {

bool g_jni_initialized = false;

void RegisterNativesIfNeeded(JNIEnv* env) {
  if (!g_jni_initialized) {
    JNI_Surface::RegisterNativesImpl(env);
    g_jni_initialized = true;
  }
}

}  // anonymous namespace

namespace content {

ScopedJavaSurface::ScopedJavaSurface() {
}

ScopedJavaSurface::ScopedJavaSurface(
    const base::android::JavaRef<jobject>& surface) {
  JNIEnv* env = base::android::AttachCurrentThread();
  RegisterNativesIfNeeded(env);
  DCHECK(env->IsInstanceOf(surface.obj(), g_Surface_clazz));
  j_surface_.Reset(surface);
}

ScopedJavaSurface::ScopedJavaSurface(
    const SurfaceTextureBridge* surface_texture) {
  JNIEnv* env = base::android::AttachCurrentThread();
  RegisterNativesIfNeeded(env);
  ScopedJavaLocalRef<jobject> tmp(JNI_Surface::Java_Surface_Constructor(
      env, surface_texture->j_surface_texture().obj()));
  DCHECK(!tmp.is_null());
  j_surface_.Reset(tmp);
}

ScopedJavaSurface::~ScopedJavaSurface() {
  if (!j_surface_.is_null()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    JNI_Surface::Java_Surface_release(env, j_surface_.obj());
  }
}

}  // namespace content
