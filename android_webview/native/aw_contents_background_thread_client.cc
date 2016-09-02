// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_contents_background_thread_client.h"

#include "jni/AwContentsBackgroundThreadClient_jni.h"

using base::android::JavaRef;

namespace android_webview {

// static
base::android::ScopedJavaLocalRef<jobject>
AwContentsBackgroundThreadClient::shouldInterceptRequest(
    JNIEnv* env,
    const JavaRef<jobject>& obj,
    const JavaRef<jstring>& url,
    jboolean isMainFrame,
    jboolean hasUserGesture,
    const JavaRef<jstring>& method,
    const JavaRef<jobjectArray>& requestHeaderNames,
    const JavaRef<jobjectArray>& requestHeaderValues) {
  return Java_AwContentsBackgroundThreadClient_shouldInterceptRequestFromNative(
      env, obj, url, isMainFrame, hasUserGesture, method, requestHeaderNames,
      requestHeaderValues);
}

}  // namespace android_webview
