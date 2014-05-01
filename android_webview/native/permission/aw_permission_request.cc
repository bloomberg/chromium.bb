// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/permission/aw_permission_request.h"

#include "android_webview/native/permission/aw_permission_request_delegate.h"
#include "base/android/jni_string.h"
#include "jni/AwPermissionRequest_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace android_webview {

AwPermissionRequest::AwPermissionRequest(
    scoped_ptr<AwPermissionRequestDelegate> delegate)
    : delegate_(delegate.Pass()),
      weak_factory_(this) {
  DCHECK(delegate_.get());
}

AwPermissionRequest::~AwPermissionRequest() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_object = java_ref_.get(env);
  if (j_object.is_null())
    return;
  Java_AwPermissionRequest_detachNativeInstance(env, j_object.obj());
}

void AwPermissionRequest::OnAccept(JNIEnv* env,
                                   jobject jcaller,
                                   jboolean accept) {
  delegate_->NotifyRequestResult(accept);
  delete this;
}

ScopedJavaLocalRef<jobject> AwPermissionRequest::CreateJavaPeer() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_object = Java_AwPermissionRequest_create(
      env, reinterpret_cast<jlong>(this),
      ConvertUTF8ToJavaString(env, GetOrigin().spec()).obj(), GetResources());
  java_ref_ = JavaObjectWeakGlobalRef(env, j_object.obj());
  return j_object;
}

ScopedJavaLocalRef<jobject> AwPermissionRequest::GetJavaObject() {
  return java_ref_.get(AttachCurrentThread());
}

const GURL& AwPermissionRequest::GetOrigin() {
  return delegate_->GetOrigin();
}

int64 AwPermissionRequest::GetResources() {
  return delegate_->GetResources();
}

bool RegisterAwPermissionRequest(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android_webivew
