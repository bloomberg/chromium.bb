// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/android/blimp_client_context_impl_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "blimp/client/core/contents/blimp_contents_impl.h"
#include "blimp/client/public/blimp_client_context.h"
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

// This function is declared in //blimp/client/public/blimp_client_context.h,
// and either this function or the one in
// //blimp/client/core/android/dummy_client_context_android.cc should be linked
// in to any binary using BlimpClientContext::GetJavaObject.
// static
base::android::ScopedJavaLocalRef<jobject> BlimpClientContext::GetJavaObject(
    BlimpClientContext* blimp_client_context) {
  BlimpClientContextImplAndroid* blimp_client_context_impl_android =
      static_cast<BlimpClientContextImplAndroid*>(blimp_client_context);
  return blimp_client_context_impl_android->GetJavaObject();
}

BlimpClientContextImplAndroid::BlimpClientContextImplAndroid(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner)
    : BlimpClientContextImpl(io_thread_task_runner, file_thread_task_runner) {
  JNIEnv* env = base::android::AttachCurrentThread();

  java_obj_.Reset(env, Java_BlimpClientContextImpl_create(
                           env, reinterpret_cast<intptr_t>(this))
                           .obj());
}

BlimpClientContextImplAndroid::~BlimpClientContextImplAndroid() {
  Java_BlimpClientContextImpl_clearNativePtr(
      base::android::AttachCurrentThread(), java_obj_);
}

base::android::ScopedJavaLocalRef<jobject>
BlimpClientContextImplAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

base::android::ScopedJavaLocalRef<jobject>
BlimpClientContextImplAndroid::CreateBlimpContentsJava(JNIEnv* env,
                                                       jobject jobj) {
  std::unique_ptr<BlimpContents> blimp_contents =
      BlimpClientContextImpl::CreateBlimpContents();
  // This intentionally releases the ownership and gives it to Java.
  BlimpContentsImpl* blimp_contents_impl =
      static_cast<BlimpContentsImpl*>(blimp_contents.release());
  return blimp_contents_impl->GetJavaObject();
}

GURL BlimpClientContextImplAndroid::GetAssignerURL() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> jurl =
      Java_BlimpClientContextImpl_getAssignerUrl(env, java_obj_);
  GURL assigner_url = GURL(ConvertJavaStringToUTF8(env, jurl));
  DCHECK(assigner_url.is_valid());
  return assigner_url;
}

void BlimpClientContextImplAndroid::ConnectFromJava(JNIEnv* env, jobject jobj) {
  BlimpClientContextImpl::Connect();
}

}  // namespace client
}  // namespace blimp
