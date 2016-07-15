// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/android/blimp_client_context_impl_android.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "blimp/client/core/blimp_contents_impl.h"
#include "jni/BlimpClientContextImpl_jni.h"

namespace blimp {
namespace client {

// static
bool BlimpClientContextImplAndroid::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
BlimpClientContextImplAndroid* BlimpClientContextImplAndroid::FromJavaObject(
    JNIEnv* env,
    jobject jobj) {
  return reinterpret_cast<BlimpClientContextImplAndroid*>(
      Java_BlimpClientContextImpl_getNativePtr(env, jobj));
}

base::android::ScopedJavaLocalRef<jobject>
BlimpClientContextImplAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

BlimpClientContextImplAndroid::BlimpClientContextImplAndroid(
    BlimpClientContextImpl* blimp_client_context_impl)
    : blimp_client_context_impl_(blimp_client_context_impl) {
  JNIEnv* env = base::android::AttachCurrentThread();

  java_obj_.Reset(env, Java_BlimpClientContextImpl_create(
                           env, reinterpret_cast<intptr_t>(this))
                           .obj());
}

BlimpClientContextImplAndroid::~BlimpClientContextImplAndroid() {
  Java_BlimpClientContextImpl_clearNativePtr(
      base::android::AttachCurrentThread(), java_obj_.obj());
}

base::android::ScopedJavaLocalRef<jobject>
BlimpClientContextImplAndroid::CreateBlimpContents(JNIEnv* env, jobject jobj) {
  std::unique_ptr<BlimpContents> blimp_contents =
      blimp_client_context_impl_->CreateBlimpContents();
  // This intentionally releases the ownership and gives it to Java.
  BlimpContentsImpl* blimp_contents_impl =
      static_cast<BlimpContentsImpl*>(blimp_contents.release());
  return blimp_contents_impl->GetJavaBlimpContentsImpl();
}

}  // namespace client
}  // namespace blimp
