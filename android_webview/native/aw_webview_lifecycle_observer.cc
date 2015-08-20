// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_webview_lifecycle_observer.h"

#include "jni/AwWebViewLifecycleObserver_jni.h"

using base::android::AttachCurrentThread;
using base::android::GetApplicationContext;

namespace android_webview {

bool RegisterAwWebViewLifecycleObserver(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
void AwWebViewLifecycleObserver::OnFirstWebViewCreated() {
  Java_AwWebViewLifecycleObserver_onFirstWebViewCreated(
      AttachCurrentThread(), GetApplicationContext());
}

// static
void AwWebViewLifecycleObserver::OnLastWebViewDestroyed() {
  Java_AwWebViewLifecycleObserver_onLastWebViewDestroyed(
      AttachCurrentThread(), GetApplicationContext());
}

}  // namespace android_webview
