// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPPS_WEBAPP_REGISTRY_H_
#define CHROME_BROWSER_ANDROID_WEBAPPS_WEBAPP_REGISTRY_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback_forward.h"

// WebappRegistry is the C++ counterpart of
// org.chromium.chrome.browser.webapp's WebappRegistry in Java.
class WebappRegistry {
 public:
  // Registers JNI hooks.
  static bool RegisterWebappRegistry(JNIEnv* env);

  // Cleans up data stored by web apps.
  static void UnregisterWebapps(const base::Closure& callback);

 private:
  ~WebappRegistry() = delete;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebappRegistry);
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPPS_WEBAPP_REGISTRY_H_
