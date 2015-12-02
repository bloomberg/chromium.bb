// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/data_use_tab_ui_manager_android.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/data_usage/data_use_ui_tab_model.h"
#include "chrome/browser/android/data_usage/data_use_ui_tab_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/sessions/core/session_id.h"
#include "jni/DataUseTabUIManager_jni.h"

// static
jboolean CheckDataUseTrackingStarted(JNIEnv* env,
                                     const JavaParamRef<jclass>& clazz,
                                     jint tab_id,
                                     const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  chrome::android::DataUseUITabModel* data_use_ui_tab_model =
      chrome::android::DataUseUITabModelFactory::GetForBrowserContext(profile);
  DCHECK_LE(0, static_cast<SessionID::id_type>(tab_id));
  if (data_use_ui_tab_model) {
    return data_use_ui_tab_model->HasDataUseTrackingStarted(
        static_cast<SessionID::id_type>(tab_id));
  }
  return false;
}

// static
jboolean CheckDataUseTrackingEnded(JNIEnv* env,
                                   const JavaParamRef<jclass>& clazz,
                                   jint tab_id,
                                   const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  chrome::android::DataUseUITabModel* data_use_ui_tab_model =
      chrome::android::DataUseUITabModelFactory::GetForBrowserContext(profile);
  DCHECK_LE(0, static_cast<SessionID::id_type>(tab_id));
  if (data_use_ui_tab_model) {
    return data_use_ui_tab_model->HasDataUseTrackingEnded(
        static_cast<SessionID::id_type>(tab_id));
  }
  return false;
}

// static
void OnCustomTabInitialNavigation(JNIEnv* env,
                                  const JavaParamRef<jclass>& clazz,
                                  jint tab_id,
                                  const JavaParamRef<jstring>& url,
                                  const JavaParamRef<jstring>& package_name,
                                  const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  chrome::android::DataUseUITabModel* data_use_ui_tab_model =
      chrome::android::DataUseUITabModelFactory::GetForBrowserContext(profile);
  DCHECK_LE(0, static_cast<SessionID::id_type>(tab_id));
  if (data_use_ui_tab_model) {
    data_use_ui_tab_model->ReportCustomTabInitialNavigation(
        static_cast<SessionID::id_type>(tab_id),
        ConvertJavaStringToUTF8(env, url),
        ConvertJavaStringToUTF8(env, package_name));
  }
}

bool RegisterDataUseTabUIManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
