// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_process.h"
#include "jni/AwBrowserProcess_jni.h"

namespace android_webview {

void TriggerMinidumpUploading() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AwBrowserProcess_triggerMinidumpUploading(env);
}

}  // namespace android_webview
