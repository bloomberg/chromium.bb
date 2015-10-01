// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_BACKGROUND_THREAD_CLIENT_H_
#define ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_BACKGROUND_THREAD_CLIENT_H_

#include "base/android/scoped_java_ref.h"

namespace android_webview {

class AwContentsBackgroundThreadClient {
 public:
  static base::android::ScopedJavaLocalRef<jobject> shouldInterceptRequest(
      JNIEnv* env,
      jobject obj,
      jstring url,
      jboolean isMainFrame,
      jboolean hasUserGesture,
      jstring method,
      jobjectArray requestHeaderNames,
      jobjectArray requestHeaderValues);
};

// JNI registration method.
bool RegisterAwContentsBackgroundThreadClient(JNIEnv* env);

}

#endif  // ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_BACKGROUND_THREAD_CLIENT_H_
