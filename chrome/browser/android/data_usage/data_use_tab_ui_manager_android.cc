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
#include "jni/DataUseTabUIManager_jni.h"

// static
jboolean CheckDataUseTrackingStarted(JNIEnv* env,
                                     const JavaParamRef<jclass>& clazz,
                                     jint tab_id,
                                     const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  chrome::android::DataUseUITabModel* data_use_ui_tab_model =
      chrome::android::DataUseUITabModelFactory::GetForBrowserContext(profile);
  if (data_use_ui_tab_model)
    return data_use_ui_tab_model->HasDataUseTrackingStarted(tab_id);
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
  if (data_use_ui_tab_model)
    return data_use_ui_tab_model->HasDataUseTrackingEnded(tab_id);
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
  if (data_use_ui_tab_model) {
    data_use_ui_tab_model->ReportCustomTabInitialNavigation(
        tab_id, ConvertJavaStringToUTF8(env, url),
        ConvertJavaStringToUTF8(env, package_name));
  }
}

bool RegisterDataUseTabUIManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
