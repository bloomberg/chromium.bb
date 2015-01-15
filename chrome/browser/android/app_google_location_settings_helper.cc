// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/app_google_location_settings_helper.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "jni/LocationSettings_jni.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ScopedJavaLocalRef;

AppGoogleLocationSettingsHelper::AppGoogleLocationSettingsHelper()
    : GoogleLocationSettingsHelper() {
}

AppGoogleLocationSettingsHelper::~AppGoogleLocationSettingsHelper() {
}

// TODO(finnur): Rename this function IsLocationEnabled.
bool AppGoogleLocationSettingsHelper::IsSystemLocationEnabled() {
  JNIEnv* env = AttachCurrentThread();
  return Java_LocationSettings_areAllLocationSettingsEnabled(env);
}

// Register native methods

bool AppGoogleLocationSettingsHelper::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
