// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/signin/account_management_screen_helper.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "jni/AccountManagementScreenHelper_jni.h"

// static
void AccountManagementScreenHelper::OpenAccountManagementScreen(
    Profile* profile) {
  DCHECK(profile);
  DCHECK(ProfileAndroid::FromProfile(profile));

  Java_AccountManagementScreenHelper_openAccountManagementScreen(
      base::android::AttachCurrentThread(),
      base::android::GetApplicationContext(),
      ProfileAndroid::FromProfile(profile)->GetJavaObject().obj());
}

// static
bool AccountManagementScreenHelper::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
