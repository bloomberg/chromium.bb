// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_METRICS_LAUNCH_METRICS_H_
#define CHROME_BROWSER_ANDROID_METRICS_LAUNCH_METRICS_H_

#include "base/android/jni_android.h"

namespace metrics {
bool RegisterLaunchMetrics(JNIEnv* env);
}  // namespace metrics

#endif  // CHROME_BROWSER_ANDROID_METRICS_LAUNCH_METRICS_H_
