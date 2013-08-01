// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/browser_startup_config.h"

#include "base/android/jni_android.h"
#include "jni/BrowserStartupConfig_jni.h"

namespace content {

bool BrowserMayStartAsynchronously() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_BrowserStartupConfig_browserMayStartAsynchonously(env);
}

void BrowserStartupComplete(int result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BrowserStartupConfig_browserStartupComplete(env, result);
}

bool RegisterBrowserStartupConfig(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
}  // namespace content
