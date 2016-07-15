// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/android/dummy_blimp_client_context_android.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "jni/DummyBlimpClientContext_jni.h"

namespace blimp {
namespace client {

// static
bool DummyBlimpClientContextAndroid::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
DummyBlimpClientContextAndroid* DummyBlimpClientContextAndroid::FromJavaObject(
    JNIEnv* env,
    jobject jobj) {
  return reinterpret_cast<DummyBlimpClientContextAndroid*>(
      Java_DummyBlimpClientContext_getNativePtr(env, jobj));
}

base::android::ScopedJavaLocalRef<jobject>
DummyBlimpClientContextAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

DummyBlimpClientContextAndroid::DummyBlimpClientContextAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();

  java_obj_.Reset(env, Java_DummyBlimpClientContext_create(
                           env, reinterpret_cast<intptr_t>(this))
                           .obj());
}

DummyBlimpClientContextAndroid::~DummyBlimpClientContextAndroid() {
  Java_DummyBlimpClientContext_clearNativePtr(
      base::android::AttachCurrentThread(), java_obj_.obj());
}

}  // namespace client
}  // namespace blimp
