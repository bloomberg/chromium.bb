// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/android/jni_registrar.h"

#include "chromecast/browser/android/cast_content_window_android.h"
#include "chromecast/browser/android/cast_metrics_helper_android.h"
#include "chromecast/browser/android/cast_web_contents_activity.h"

namespace chromecast {
namespace shell {

bool RegisterJni(JNIEnv* env) {
  if (!CastContentWindowAndroid::RegisterJni(env))
    return false;
  if (!CastMetricsHelperAndroid::RegisterJni(env))
    return false;
  if (!CastWebContentsActivity::RegisterJni(env))
    return false;

  return true;
}

}  // namespace shell
}  // namespace chromecast
