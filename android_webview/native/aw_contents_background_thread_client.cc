// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_contents_background_thread_client.h"

#include "jni/AwContentsBackgroundThreadClient_jni.h"

namespace android_webview {

// static
base::android::ScopedJavaLocalRef<jobject>
AwContentsBackgroundThreadClient::shouldInterceptRequest(
    JNIEnv* env,
    jobject obj,
    jstring url,
    jboolean isMainFrame,
    jboolean hasUserGesture,
    jstring method,
    jobjectArray requestHeaderNames,
    jobjectArray requestHeaderValues) {
  return Java_AwContentsBackgroundThreadClient_shouldInterceptRequestFromNative(
      env, obj, url, isMainFrame, hasUserGesture, method, requestHeaderNames,
      requestHeaderValues);
}

bool RegisterAwContentsBackgroundThreadClient(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android_webview
