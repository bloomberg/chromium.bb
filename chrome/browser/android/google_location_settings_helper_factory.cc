// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/google_location_settings_helper_factory.h"

#include "base/logging.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "jni/GoogleLocationSettingsHelperStub_jni.h"

GoogleLocationSettingsHelperFactory::GoogleLocationSettingsHelperFactory() {
}

GoogleLocationSettingsHelperFactory::~GoogleLocationSettingsHelperFactory() {
}

const base::android::ScopedJavaGlobalRef<jobject>&
    GoogleLocationSettingsHelperFactory::GetHelperInstance() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_google_location_settings_helper_.Reset(
      Java_GoogleLocationSettingsHelperStub_createInstance(env,
          base::android::GetApplicationContext()));
  return java_google_location_settings_helper_;
}

// Register native methods

bool GoogleLocationSettingsHelperFactory::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
