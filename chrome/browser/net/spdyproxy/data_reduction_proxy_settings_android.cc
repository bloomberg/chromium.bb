// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_settings_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_configurator.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_usage_stats.h"
#include "jni/DataReductionProxySettings_jni.h"


using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using data_reduction_proxy::DataReductionProxyParams;
using data_reduction_proxy::DataReductionProxySettings;

DataReductionProxySettingsAndroid::DataReductionProxySettingsAndroid() {
}

DataReductionProxySettingsAndroid::~DataReductionProxySettingsAndroid() {
}

jboolean DataReductionProxySettingsAndroid::IsDataReductionProxyAllowed(
    JNIEnv* env, jobject obj) {
  return Settings()->params()->allowed();
}

jboolean DataReductionProxySettingsAndroid::IsDataReductionProxyPromoAllowed(
    JNIEnv* env, jobject obj) {
  return Settings()->params()->promo_allowed();
}

jboolean DataReductionProxySettingsAndroid::IsIncludedInAltFieldTrial(
    JNIEnv* env, jobject obj) {
  return DataReductionProxyParams::IsIncludedInAlternativeFieldTrial();
}

ScopedJavaLocalRef<jstring>
DataReductionProxySettingsAndroid::GetDataReductionProxyOrigin(
    JNIEnv* env, jobject obj) {
  return ConvertUTF8ToJavaString(env, Settings()->params()->origin().spec());
}

jboolean DataReductionProxySettingsAndroid::IsDataReductionProxyEnabled(
    JNIEnv* env, jobject obj) {
  return Settings()->IsDataReductionProxyEnabled();
}

jboolean DataReductionProxySettingsAndroid::IsDataReductionProxyManaged(
    JNIEnv* env, jobject obj) {
  return Settings()->IsDataReductionProxyManaged();
}

void DataReductionProxySettingsAndroid::SetDataReductionProxyEnabled(
    JNIEnv* env,
    jobject obj,
    jboolean enabled) {
  Settings()->SetDataReductionProxyEnabled(enabled);
}

jlong DataReductionProxySettingsAndroid::GetDataReductionLastUpdateTime(
    JNIEnv* env, jobject obj) {
  return Settings()->GetDataReductionLastUpdateTime();
}

base::android::ScopedJavaLocalRef<jobject>
DataReductionProxySettingsAndroid::GetContentLengths(JNIEnv* env,
                                                     jobject obj) {
  int64 original_content_length;
  int64 received_content_length;
  int64 last_update_internal;
  Settings()->GetContentLengths(
      data_reduction_proxy::kNumDaysInHistorySummary,
      &original_content_length,
      &received_content_length,
      &last_update_internal);

  return Java_ContentLengths_create(env,
                                    original_content_length,
                                    received_content_length);
}

ScopedJavaLocalRef<jlongArray>
DataReductionProxySettingsAndroid::GetDailyOriginalContentLengths(
    JNIEnv* env, jobject obj) {
  return GetDailyContentLengths(
      env, data_reduction_proxy::prefs::kDailyHttpOriginalContentLength);
}

ScopedJavaLocalRef<jlongArray>
DataReductionProxySettingsAndroid::GetDailyReceivedContentLengths(
    JNIEnv* env, jobject obj) {
  return GetDailyContentLengths(
      env, data_reduction_proxy::prefs::kDailyHttpReceivedContentLength);
}

jboolean DataReductionProxySettingsAndroid::IsDataReductionProxyUnreachable(
    JNIEnv* env, jobject obj) {
  return Settings()->IsDataReductionProxyUnreachable();
}

// static
bool DataReductionProxySettingsAndroid::Register(JNIEnv* env) {
  bool register_natives_impl_result = RegisterNativesImpl(env);
  return register_natives_impl_result;
}

ScopedJavaLocalRef<jlongArray>
DataReductionProxySettingsAndroid::GetDailyContentLengths(
    JNIEnv* env,  const char* pref_name) {
  jlongArray result = env->NewLongArray(
      data_reduction_proxy::kNumDaysInHistory);

  DataReductionProxySettings::ContentLengthList lengths  =
      Settings()->GetDailyContentLengths(pref_name);

  if (!lengths.empty()) {
    DCHECK_EQ(lengths.size(), data_reduction_proxy::kNumDaysInHistory);
    env->SetLongArrayRegion(result, 0, lengths.size(), &lengths[0]);
    return ScopedJavaLocalRef<jlongArray>(env, result);
  }

  return ScopedJavaLocalRef<jlongArray>(env, result);
}

DataReductionProxySettings* DataReductionProxySettingsAndroid::Settings() {
  DataReductionProxySettings* settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          ProfileManager::GetActiveUserProfile());
  DCHECK(settings);
  return settings;
}


// Used by generated jni code.
static jlong Init(JNIEnv* env, jobject obj) {
  return reinterpret_cast<intptr_t>(new DataReductionProxySettingsAndroid());
}
