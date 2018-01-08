// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/metrics/uma_utils.h"

#include <stdint.h>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "components/metrics/metrics_reporting_default_state.h"
#include "jni/UmaUtils_jni.h"

using base::android::JavaParamRef;

class PrefService;

namespace chrome {
namespace android {

base::Time GetMainEntryPointTimeWallClock() {
  JNIEnv* env = base::android::AttachCurrentThread();
  int64_t startTimeUnixMs = Java_UmaUtils_getMainEntryPointWallTime(env);
  return base::Time::UnixEpoch() +
         base::TimeDelta::FromMilliseconds(startTimeUnixMs);
}

base::TimeTicks GetMainEntryPointTimeTicks() {
  JNIEnv* env = base::android::AttachCurrentThread();
  // Generally the use of base::TimeTicks::FromInternalValue() is discouraged.
  //
  // The implementation of the SystemClock.uptimeMillis() in AOSP uses the same
  // clock as base::TimeTicks::Now(): clock_gettime(CLOCK_MONOTONIC), see in
  // platform/system/code:
  // 1. libutils/SystemClock.cpp
  // 2. libutils/Timers.cpp
  //
  // We are not aware of any motivations for Android OEMs to modify the AOSP
  // implementation of either uptimeMillis() or clock_gettime(CLOCK_MONOTONIC),
  // so we assume that there are no such customizations.
  //
  // Under these assumptions the conversion is as safe as copying the value of
  // base::TimeTicks::Now() with a loss of sub-millisecond precision.
  return base::TimeTicks::FromInternalValue(
      Java_UmaUtils_getMainEntryPointTicks(env) * 1000);
}

static jboolean JNI_UmaUtils_IsClientInMetricsReportingSample(
    JNIEnv* env,
    const JavaParamRef<jclass>& obj) {
  return ChromeMetricsServicesManagerClient::IsClientInSample();
}

static void JNI_UmaUtils_RecordMetricsReportingDefaultOptIn(
    JNIEnv* env,
    const JavaParamRef<jclass>& obj,
    jboolean opt_in) {
  DCHECK(g_browser_process);
  PrefService* local_state = g_browser_process->local_state();
  metrics::RecordMetricsReportingDefaultState(
      local_state, opt_in ? metrics::EnableMetricsDefault::OPT_IN
                          : metrics::EnableMetricsDefault::OPT_OUT);
}

}  // namespace android
}  // namespace chrome
