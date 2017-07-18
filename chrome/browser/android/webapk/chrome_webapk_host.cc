// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/chrome_webapk_host.h"

#include "base/feature_list.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "components/variations/variations_associated_data.h"
#include "jni/ChromeWebApkHost_jni.h"

namespace {

// Variations flag to enable launching Chrome renderer in WebAPK process.
const char* kLaunchRendererInWebApkProcess =
    "launch_renderer_in_webapk_process";

}  // anonymous namespace

// static
bool ChromeWebApkHost::CanInstallWebApk() {
  return base::FeatureList::IsEnabled(chrome::android::kImprovedA2HS);
}

// static
jboolean CanLaunchRendererInWebApkProcess(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& clazz) {
  return variations::GetVariationParamValueByFeature(
             chrome::android::kImprovedA2HS, kLaunchRendererInWebApkProcess) ==
         "true";
}
