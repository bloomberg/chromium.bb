// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/uma_utils.h"

#include "jni/UmaUtils_jni.h"

namespace chrome {
namespace android {

base::Time GetMainEntryPointTime() {
  JNIEnv* env = base::android::AttachCurrentThread();
  int64 startTimeUnixMs = Java_UmaUtils_getMainEntryPointTime(env);
  return base::Time::UnixEpoch() +
         base::TimeDelta::FromMilliseconds(startTimeUnixMs);
}

bool RegisterStartupMetricUtils(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace chrome
