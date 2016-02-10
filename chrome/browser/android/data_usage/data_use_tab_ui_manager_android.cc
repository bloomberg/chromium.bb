// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/data_use_tab_ui_manager_android.h"

#include <stdint.h>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/data_usage/data_use_ui_tab_model.h"
#include "chrome/browser/android/data_usage/data_use_ui_tab_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/grit/generated_resources.h"
#include "components/sessions/core/session_id.h"
#include "jni/DataUseTabUIManager_jni.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Represents the IDs for string messages used by data use snackbar and
// interstitial dialog. A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.datausage
enum DataUseUIMessage {
  DATA_USE_TRACKING_STARTED_SNACKBAR_MESSAGE,
  DATA_USE_TRACKING_SNACKBAR_ACTION,
  DATA_USE_TRACKING_ENDED_SNACKBAR_MESSAGE,
  DATA_USE_TRACKING_ENDED_TITLE,
  DATA_USE_TRACKING_ENDED_MESSAGE,
  DATA_USE_TRACKING_ENDED_CHECKBOX_MESSAGE,
  DATA_USE_TRACKING_ENDED_CONTINUE,
  DATA_USE_LEARN_MORE_TITLE,
  DATA_USE_LEARN_MORE_LINK_URL,
  DATA_USE_UI_MESSAGE_MAX
};

// Represents the mapping between DataUseUIMessage ID and UI message IDs in
// generated_resources.grd
uint32_t data_use_ui_message_id_map[DATA_USE_UI_MESSAGE_MAX] = {
        [DATA_USE_TRACKING_STARTED_SNACKBAR_MESSAGE] =
            IDS_DATA_USE_TRACKING_STARTED_SNACKBAR_MESSAGE,
        [DATA_USE_TRACKING_SNACKBAR_ACTION] =
            IDS_DATA_USE_TRACKING_SNACKBAR_ACTION,
        [DATA_USE_TRACKING_ENDED_SNACKBAR_MESSAGE] =
            IDS_DATA_USE_TRACKING_ENDED_SNACKBAR_MESSAGE,
        [DATA_USE_TRACKING_ENDED_TITLE] = IDS_DATA_USE_TRACKING_ENDED_TITLE,
        [DATA_USE_TRACKING_ENDED_MESSAGE] = IDS_DATA_USE_TRACKING_ENDED_MESSAGE,
        [DATA_USE_TRACKING_ENDED_CHECKBOX_MESSAGE] =
            IDS_DATA_USE_TRACKING_ENDED_CHECKBOX_MESSAGE,
        [DATA_USE_TRACKING_ENDED_CONTINUE] =
            IDS_DATA_USE_TRACKING_ENDED_CONTINUE,
        [DATA_USE_LEARN_MORE_TITLE] = IDS_DATA_USE_LEARN_MORE_TITLE,
        [DATA_USE_LEARN_MORE_LINK_URL] = IDS_DATA_USE_LEARN_MORE_LINK_URL,
};

}  // namespace

// static
jboolean CheckAndResetDataUseTrackingStarted(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    jint tab_id,
    const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  chrome::android::DataUseUITabModel* data_use_ui_tab_model =
      chrome::android::DataUseUITabModelFactory::GetForBrowserContext(profile);
  DCHECK_LE(0, static_cast<SessionID::id_type>(tab_id));
  if (data_use_ui_tab_model) {
    return data_use_ui_tab_model->CheckAndResetDataUseTrackingStarted(
        static_cast<SessionID::id_type>(tab_id));
  }
  return false;
}

// static
jboolean CheckAndResetDataUseTrackingEnded(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    jint tab_id,
    const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  chrome::android::DataUseUITabModel* data_use_ui_tab_model =
      chrome::android::DataUseUITabModelFactory::GetForBrowserContext(profile);
  DCHECK_LE(0, static_cast<SessionID::id_type>(tab_id));
  if (data_use_ui_tab_model) {
    return data_use_ui_tab_model->CheckAndResetDataUseTrackingEnded(
        static_cast<SessionID::id_type>(tab_id));
  }
  return false;
}

// static
void UserClickedContinueOnDialogBox(JNIEnv* env,
                                    const JavaParamRef<jclass>& clazz,
                                    jint tab_id,
                                    const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  chrome::android::DataUseUITabModel* data_use_ui_tab_model =
      chrome::android::DataUseUITabModelFactory::GetForBrowserContext(profile);
  DCHECK_LE(0, static_cast<SessionID::id_type>(tab_id));
  if (data_use_ui_tab_model) {
    data_use_ui_tab_model->UserClickedContinueOnDialogBox(
        static_cast<SessionID::id_type>(tab_id));
  }
}

// static
jboolean WouldDataUseTrackingEnd(JNIEnv* env,
                                 const JavaParamRef<jclass>& clazz,
                                 jint tab_id,
                                 const JavaParamRef<jstring>& url,
                                 jint transition_type,
                                 const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  chrome::android::DataUseUITabModel* data_use_ui_tab_model =
      chrome::android::DataUseUITabModelFactory::GetForBrowserContext(profile);
  DCHECK_LE(0, static_cast<SessionID::id_type>(tab_id));
  if (data_use_ui_tab_model) {
    return data_use_ui_tab_model->WouldDataUseTrackingEnd(
        ConvertJavaStringToUTF8(env, url), transition_type,
        static_cast<SessionID::id_type>(tab_id));
  }
  return false;
}

// static
void OnCustomTabInitialNavigation(JNIEnv* env,
                                  const JavaParamRef<jclass>& clazz,
                                  jint tab_id,
                                  const JavaParamRef<jstring>& package_name,
                                  const JavaParamRef<jstring>& url,
                                  const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  chrome::android::DataUseUITabModel* data_use_ui_tab_model =
      chrome::android::DataUseUITabModelFactory::GetForBrowserContext(profile);
  DCHECK_LE(0, static_cast<SessionID::id_type>(tab_id));
  if (data_use_ui_tab_model) {
    data_use_ui_tab_model->ReportCustomTabInitialNavigation(
        static_cast<SessionID::id_type>(tab_id),
        ConvertJavaStringToUTF8(env, package_name),
        ConvertJavaStringToUTF8(env, url));
  }
}

// static
ScopedJavaLocalRef<jstring> GetDataUseUIString(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    int message_id) {
  DCHECK(message_id >= 0 && message_id < DATA_USE_UI_MESSAGE_MAX);
  return base::android::ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(data_use_ui_message_id_map[message_id]));
}

bool RegisterDataUseTabUIManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
