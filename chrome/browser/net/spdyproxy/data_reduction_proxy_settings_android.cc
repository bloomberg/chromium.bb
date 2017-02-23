// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_settings_android.h"

#include <stdint.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/values.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "jni/DataReductionProxySettings_jni.h"
#include "net/proxy/proxy_server.h"
#include "url/gurl.h"


using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using data_reduction_proxy::DataReductionProxySettings;

DataReductionProxySettingsAndroid::DataReductionProxySettingsAndroid() {
}

DataReductionProxySettingsAndroid::~DataReductionProxySettingsAndroid() {
}

jboolean DataReductionProxySettingsAndroid::IsDataReductionProxyPromoAllowed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return Settings()->PromoAllowed();
}

jboolean DataReductionProxySettingsAndroid::IsDataReductionProxyEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return Settings()->IsDataReductionProxyEnabled();
}

jboolean DataReductionProxySettingsAndroid::IsDataReductionProxyManaged(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return Settings()->IsDataReductionProxyManaged();
}

void DataReductionProxySettingsAndroid::SetDataReductionProxyEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean enabled) {
  Settings()->SetDataReductionProxyEnabled(enabled);
}

jlong DataReductionProxySettingsAndroid::GetDataReductionLastUpdateTime(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return Settings()->GetDataReductionLastUpdateTime();
}

base::android::ScopedJavaLocalRef<jobject>
DataReductionProxySettingsAndroid::GetContentLengths(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  int64_t original_content_length;
  int64_t received_content_length;
  int64_t last_update_internal;
  Settings()->GetContentLengths(
      data_reduction_proxy::kNumDaysInHistorySummary,
      &original_content_length,
      &received_content_length,
      &last_update_internal);

  return Java_ContentLengths_create(env,
                                    original_content_length,
                                    received_content_length);
}

jlong DataReductionProxySettingsAndroid::GetTotalHttpContentLengthSaved(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return Settings()->GetTotalHttpContentLengthSaved();
}

ScopedJavaLocalRef<jlongArray>
DataReductionProxySettingsAndroid::GetDailyOriginalContentLengths(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return GetDailyContentLengths(
      env, data_reduction_proxy::prefs::kDailyHttpOriginalContentLength);
}

ScopedJavaLocalRef<jlongArray>
DataReductionProxySettingsAndroid::GetDailyReceivedContentLengths(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return GetDailyContentLengths(
      env, data_reduction_proxy::prefs::kDailyHttpReceivedContentLength);
}

jboolean DataReductionProxySettingsAndroid::IsDataReductionProxyUnreachable(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return Settings()->IsDataReductionProxyUnreachable();
}

jboolean DataReductionProxySettingsAndroid::AreLoFiPreviewsEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return data_reduction_proxy::params::IsIncludedInLitePageFieldTrial() ||
      (data_reduction_proxy::params::IsLoFiOnViaFlags() &&
          data_reduction_proxy::params::AreLitePagesEnabledViaFlags());
}

// static
bool DataReductionProxySettingsAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

ScopedJavaLocalRef<jlongArray>
DataReductionProxySettingsAndroid::GetDailyContentLengths(
    JNIEnv* env,  const char* pref_name) {
  jlongArray result = env->NewLongArray(
      data_reduction_proxy::kNumDaysInHistory);

 data_reduction_proxy::ContentLengthList lengths  =
      Settings()->GetDailyContentLengths(pref_name);

  if (!lengths.empty()) {
    DCHECK_EQ(lengths.size(), data_reduction_proxy::kNumDaysInHistory);
    env->SetLongArrayRegion(result, 0, lengths.size(), &lengths[0]);
    return ScopedJavaLocalRef<jlongArray>(env, result);
  }

  return ScopedJavaLocalRef<jlongArray>(env, result);
}

ScopedJavaLocalRef<jstring> DataReductionProxySettingsAndroid::GetHttpProxyList(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  data_reduction_proxy::DataReductionProxyEventStore* event_store =
      Settings()->GetEventStore();
  if (!event_store)
    return ConvertUTF8ToJavaString(env, std::string());

  return ConvertUTF8ToJavaString(env, event_store->GetHttpProxyList());
}

DataReductionProxySettings* DataReductionProxySettingsAndroid::Settings() {
  DataReductionProxySettings* settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          ProfileManager::GetActiveUserProfile());
  DCHECK(settings);
  return settings;
}

ScopedJavaLocalRef<jstring>
DataReductionProxySettingsAndroid::GetLastBypassEvent(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  data_reduction_proxy::DataReductionProxyEventStore* event_store =
      Settings()->GetEventStore();
  if (!event_store)
    return ConvertUTF8ToJavaString(env, std::string());

  return ConvertUTF8ToJavaString(env, event_store->SanitizedLastBypassEvent());
}

// Used by generated jni code.
static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new DataReductionProxySettingsAndroid());
}
