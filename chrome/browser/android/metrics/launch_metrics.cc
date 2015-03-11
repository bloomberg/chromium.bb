// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/metrics/launch_metrics.h"

#include "base/android/jni_string.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/browser_process.h"
#include "components/rappor/rappor_utils.h"
#include "jni/LaunchMetrics_jni.h"
#include "url/gurl.h"

namespace metrics {

enum HomeScreenLaunch {
  HOME_SCREEN_LAUNCH_STANDALONE = 0,
  HOME_SCREEN_LAUNCH_SHORTCUT = 1,
  HOME_SCREEN_LAUNCH_COUNT = 2
};

bool RegisterLaunchMetrics(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static void RecordLaunch(JNIEnv* env, jclass caller, jboolean standalone,
                         jstring jurl) {
  int action = standalone ? HOME_SCREEN_LAUNCH_STANDALONE
                          : HOME_SCREEN_LAUNCH_SHORTCUT;
  std::string rappor_metric = standalone ? "Launch.HomeScreen.Standalone"
                                         : "Launch.HomeScreen.Shortcut";

  UMA_HISTOGRAM_ENUMERATION("Launch.HomeScreen", action,
                            HOME_SCREEN_LAUNCH_COUNT);

  std::string url = base::android::ConvertJavaStringToUTF8(env, jurl);
  rappor::SampleDomainAndRegistryFromGURL(g_browser_process->rappor_service(),
                                          rappor_metric, GURL(url));
}

};  // namespace metrics
