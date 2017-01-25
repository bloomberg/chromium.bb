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

}  // anonymous namespace

// static
bool ChromeWebApkHost::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
bool ChromeWebApkHost::AreWebApkEnabled() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ChromeWebApkHost_areWebApkEnabled(env);
}

// static
jboolean CanUseGooglePlayToInstallWebApk(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& clazz) {
  return variations::GetVariationParamValueByFeature(
             chrome::android::kImprovedA2HS, kPlayInstall) == "true";
}
