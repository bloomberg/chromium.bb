// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPPS_WEBAPP_METRICS_H_
#define CHROME_BROWSER_ANDROID_WEBAPPS_WEBAPP_METRICS_H_

#include "base/android/jni_android.h"

namespace webapps {

// Allows logging of WebappActivity-specific metrics.
class WebappMetrics {
 public:
  static bool Register(JNIEnv* env);
};

}  // namespace webapps

#endif  // CHROME_BROWSER_ANDROID_WEBAPPS_WEBAPP_METRICS_H_
