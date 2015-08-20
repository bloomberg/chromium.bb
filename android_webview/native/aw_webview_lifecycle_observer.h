// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_WEBVIEW_LIFECYCLE_OBSERVER_H_
#define ANDROID_WEBVIEW_NATIVE_AW_WEBVIEW_LIFECYCLE_OBSERVER_H_

#include "base/android/jni_android.h"

namespace android_webview {

class AwWebViewLifecycleObserver {
 public:
  static void OnFirstWebViewCreated();
  static void OnLastWebViewDestroyed();

 private:
  DISALLOW_COPY_AND_ASSIGN(AwWebViewLifecycleObserver);
};

bool RegisterAwWebViewLifecycleObserver(JNIEnv* env);

}  // namespace android_webview

#endif
