// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_UPDATE_MANAGER_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_UPDATE_MANAGER_H_

#include "base/android/jni_android.h"
#include "base/macros.h"

enum class WebApkInstallResult;

// WebApkUpdateManager is the C++ counterpart of org.chromium.chrome.browser's
// WebApkUpdateManager in Java. It calls WebApkInstaller to send an update
// request to WebAPK Server.
class WebApkUpdateManager {
 public:
  // Registers JNI hooks.
  static bool Register(JNIEnv* env);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(WebApkUpdateManager);
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_UPDATE_MANAGER_H_
