// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/surface_texture_bridge.h"

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "content/common/android/surface_texture_listener.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::GetClass;
using base::android::GetMethodID;
using base::android::ScopedJavaLocalRef;

namespace content {

SurfaceTextureBridge::SurfaceTextureBridge(int texture_id)
    : texture_id_(texture_id) {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  j_class_.Reset(GetClass(env, "android/graphics/SurfaceTexture"));
  jmethodID constructor = GetMethodID(env, j_class_, "<init>", "(I)V");
  ScopedJavaLocalRef<jobject> tmp(env,
      env->NewObject(j_class_.obj(), constructor, texture_id));
  DCHECK(!tmp.is_null());
  j_surface_texture_.Reset(tmp);
}

SurfaceTextureBridge::~SurfaceTextureBridge() {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  // Release the listener.
  const char* method_signature =
      "(Landroid/graphics/SurfaceTexture$OnFrameAvailableListener;)V";
  jmethodID method = GetMethodID(env, j_class_, "setOnFrameAvailableListener",
                                 method_signature);
  env->CallVoidMethod(j_surface_texture_.obj(), method, NULL);
  CheckException(env);

  // Release graphics memory.
  jmethodID release = GetMethodID(env, j_class_, "release", "()V");
  env->CallVoidMethod(j_surface_texture_.obj(), release);
  CheckException(env);
}

void SurfaceTextureBridge::SetFrameAvailableCallback(
    const base::Closure& callback) {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  // Since the listener is owned by the Java SurfaceTexture object, setting
  // a new listener here will release an existing one at the same time.
  ScopedJavaLocalRef<jobject> j_listener(env,
      SurfaceTextureListener::CreateSurfaceTextureListener(env, callback));
  DCHECK(!j_listener.is_null());

  // Set it as the onFrameAvailableListener for our SurfaceTexture instance.
  const char* method_signature =
      "(Landroid/graphics/SurfaceTexture$OnFrameAvailableListener;)V";
  jmethodID method = GetMethodID(env, j_class_, "setOnFrameAvailableListener",
                                 method_signature);
  env->CallVoidMethod(j_surface_texture_.obj(), method, j_listener.obj());
  CheckException(env);
}

void SurfaceTextureBridge::UpdateTexImage() {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  jmethodID method = GetMethodID(env, j_class_, "updateTexImage", "()V");
  env->CallVoidMethod(j_surface_texture_.obj(), method);
  CheckException(env);
}

void SurfaceTextureBridge::GetTransformMatrix(float mtx[16]) {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  ScopedJavaLocalRef<jfloatArray> jmatrix(env, env->NewFloatArray(16));
  jmethodID method = GetMethodID(env, j_class_, "getTransformMatrix", "([F)V");
  env->CallVoidMethod(j_surface_texture_.obj(), method, jmatrix.obj());
  CheckException(env);

  jboolean is_copy;
  jfloat* elements = env->GetFloatArrayElements(jmatrix.obj(), &is_copy);
  for (int i = 0; i < 16; ++i) {
    mtx[i] = static_cast<float>(elements[i]);
  }
  env->ReleaseFloatArrayElements(jmatrix.obj(), elements, JNI_ABORT);
}

void SurfaceTextureBridge::SetDefaultBufferSize(int width, int height) {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  jmethodID method = GetMethodID(env, j_class_,
                                 "setDefaultBufferSize", "(II)V");
  env->CallVoidMethod(j_surface_texture_.obj(), method,
                      static_cast<jint>(width), static_cast<jint>(height));
  CheckException(env);
}

}  // namespace content
