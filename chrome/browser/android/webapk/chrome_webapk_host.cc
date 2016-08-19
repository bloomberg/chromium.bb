// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/chrome_webapk_host.h"

#include "jni/ChromeWebApkHost_jni.h"

// static
bool ChromeWebApkHost::AreWebApkEnabled() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ChromeWebApkHost_areWebApkEnabled(env);
}
