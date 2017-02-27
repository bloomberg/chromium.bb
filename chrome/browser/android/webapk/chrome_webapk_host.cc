// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/chrome_webapk_host.h"

#include "chrome/browser/android/chrome_feature_list.h"
#include "components/variations/variations_associated_data.h"
#include "jni/ChromeWebApkHost_jni.h"

namespace {

// Variations flag to enable installing WebAPKs using Google Play.
const char* kPlayInstall = "play_install";

// Variations flag to enable launching Chrome renderer in WebAPK process.
const char* kLaunchRendererInWebApkProcess =
    "launch_renderer_in_webapk_process";

}  // anonymous namespace

// static
bool ChromeWebApkHost::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
bool ChromeWebApkHost::CanInstallWebApk() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ChromeWebApkHost_canInstallWebApk(env);
}

// static
GooglePlayInstallState ChromeWebApkHost::GetGooglePlayInstallState() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return static_cast<GooglePlayInstallState>(
      Java_ChromeWebApkHost_getGooglePlayInstallState(env));
}

// static
jboolean CanUseGooglePlayToInstallWebApk(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& clazz) {
  return variations::GetVariationParamValueByFeature(
             chrome::android::kImprovedA2HS, kPlayInstall) == "true";
}

// static
jboolean CanLaunchRendererInWebApkProcess(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& clazz) {
  return variations::GetVariationParamValueByFeature(
             chrome::android::kImprovedA2HS, kLaunchRendererInWebApkProcess) ==
         "true";
}

// static
jboolean CanInstallFromUnknownSources(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& clazz) {
  return base::FeatureList::GetInstance()->IsFeatureOverriddenFromCommandLine(
             chrome::android::kImprovedA2HS.name,
             base::FeatureList::OVERRIDE_ENABLE_FEATURE);
}
