// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_CHROME_WEBAPK_HOST_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_CHROME_WEBAPK_HOST_H_

#include <jni.h>

#include "base/macros.h"

// This enum is used to back a UMA histogram, and must be treated as
// append-only.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser
enum class GooglePlayInstallState {
  SUPPORTED = 0,
  DISABLED_OTHER = 1,
  NO_PLAY_SERVICES = 2,
  DISABLED_BY_VARIATIONS = 3,
  DISABLED_BY_PLAY = 4,
  MAX = 5
};

// ChromeWebApkHost is the C++ counterpart of org.chromium.chrome.browser's
// ChromeWebApkHost in Java.
class ChromeWebApkHost {
 public:
  // Registers JNI hooks.
  static bool Register(JNIEnv* env);

  // Returns whether installing WebApk is possible either from "unknown sources"
  // or Google Play.
  static bool CanInstallWebApk();

  // Returns the state of installing a WebAPK from Google Play.
  static GooglePlayInstallState GetGooglePlayInstallState();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ChromeWebApkHost);
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_CHROME_WEBAPK_HOST_H_
