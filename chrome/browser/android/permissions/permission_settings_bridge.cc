// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/scoped_java_ref.h"
#include "chrome/android/chrome_jni_headers/PermissionSettingsBridge_jni.h"
#include "chrome/browser/permissions/adaptive_notification_permission_ui_selector.h"
#include "chrome/browser/profiles/profile_android.h"

using base::android::JavaParamRef;

static jboolean JNI_PermissionSettingsBridge_ShouldShowNotificationsPromo(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile) {
  return AdaptiveNotificationPermissionUiSelector::GetForProfile(
             ProfileAndroid::FromProfileAndroid(jprofile))
      ->ShouldShowPromo();
}

static void JNI_PermissionSettingsBridge_DidShowNotificationsPromo(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile) {
  AdaptiveNotificationPermissionUiSelector::GetForProfile(
      ProfileAndroid::FromProfileAndroid(jprofile))
      ->PromoWasShown();
}
