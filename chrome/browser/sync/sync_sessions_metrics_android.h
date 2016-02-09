// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_SESSIONS_METRICS_ANDROID_H_
#define CHROME_BROWSER_SYNC_SYNC_SESSIONS_METRICS_ANDROID_H_

#include <jni.h>

// Android JNI bridge, java class name is SyncSessionsMetrics.
class SyncSessionsMetricsAndroid {
 public:
  static bool Register(JNIEnv* env);
};

#endif  // CHROME_BROWSER_SYNC_SYNC_SESSIONS_METRICS_ANDROID_H_
