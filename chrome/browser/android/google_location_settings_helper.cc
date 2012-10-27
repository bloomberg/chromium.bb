// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/google_location_settings_helper.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "jni/GoogleLocationSettingsHelper_jni.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ScopedJavaLocalRef;

GoogleLocationSettingsHelperFactory*
    GoogleLocationSettingsHelper::helper_factory_ = NULL;

GoogleLocationSettingsHelper::GoogleLocationSettingsHelper() {
  JNIEnv* env = AttachCurrentThread();
  if (helper_factory_ == NULL) {
    SetGoogleLocationSettingsHelperFactory(
        new GoogleLocationSettingsHelperFactory());
  }
  java_google_location_settings_helper_.Reset(env,
      helper_factory_->GetHelperInstance().obj());
}

GoogleLocationSettingsHelper::~GoogleLocationSettingsHelper() {
}

void GoogleLocationSettingsHelper::SetGoogleLocationSettingsHelperFactory(
    GoogleLocationSettingsHelperFactory* factory) {
  helper_factory_ = factory;
}

std::string GoogleLocationSettingsHelper::GetAcceptButtonLabel() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> infobar_allow_text =
      Java_GoogleLocationSettingsHelper_getInfoBarAcceptLabel(
          env, java_google_location_settings_helper_.obj());
  return ConvertJavaStringToUTF8(infobar_allow_text);
}

void GoogleLocationSettingsHelper::ShowGoogleLocationSettings() {
  JNIEnv* env = AttachCurrentThread();
  Java_GoogleLocationSettingsHelper_startGoogleLocationSettingsActivity(
      env, java_google_location_settings_helper_.obj());
}

bool GoogleLocationSettingsHelper::IsGoogleAppsLocationSettingEnabled() {
  JNIEnv* env = AttachCurrentThread();
  return Java_GoogleLocationSettingsHelper_isGoogleAppsLocationSettingEnabled(
      env, java_google_location_settings_helper_.obj());
}

bool GoogleLocationSettingsHelper::IsMasterLocationSettingEnabled() {
  JNIEnv* env = AttachCurrentThread();
  return Java_GoogleLocationSettingsHelper_isMasterLocationSettingEnabled(
      env, java_google_location_settings_helper_.obj());
}

// Register native methods

bool GoogleLocationSettingsHelper::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
