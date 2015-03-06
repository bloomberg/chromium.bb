// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/webapp_metrics.h"

#include "base/android/jni_string.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/metrics/rappor/sampling.h"
#include "jni/WebappMetrics_jni.h"
#include "url/gurl.h"

namespace webapps {

bool WebappMetrics::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static void RecordActivityStart(JNIEnv* env, jclass caller,
                                jstring jurl) {
  std::string url = base::android::ConvertJavaStringToUTF8(env, jurl);
  base::RecordAction(base::UserMetricsAction("webapps.LaunchedFromHome"));
  rappor::SampleDomainAndRegistryFromGURL("webapps.LaunchURL", GURL(url));
}

};  // namespace webapps

