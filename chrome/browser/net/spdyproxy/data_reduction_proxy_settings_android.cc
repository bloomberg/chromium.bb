// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_settings_android.h"

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
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "jni/DataReductionProxySettings_jni.h"
#include "net/proxy/proxy_server.h"
#include "url/gurl.h"


using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using data_reduction_proxy::DataReductionProxySettings;

DataReductionProxySettingsAndroid::DataReductionProxySettingsAndroid() {
}

DataReductionProxySettingsAndroid::~DataReductionProxySettingsAndroid() {
}

jboolean DataReductionProxySettingsAndroid::IsDataReductionProxyAllowed(
    JNIEnv* env, jobject obj) {
  return Settings()->Allowed();
}

jboolean DataReductionProxySettingsAndroid::IsDataReductionProxyPromoAllowed(
    JNIEnv* env, jobject obj) {
  return Settings()->PromoAllowed();
}

jboolean DataReductionProxySettingsAndroid::IsIncludedInAltFieldTrial(
    JNIEnv* env, jobject obj) {
  return false;
}

jboolean DataReductionProxySettingsAndroid::IsDataReductionProxyEnabled(
    JNIEnv* env, jobject obj) {
  return Settings()->IsDataReductionProxyEnabled();
}

jboolean DataReductionProxySettingsAndroid::CanUseDataReductionProxy(
    JNIEnv* env, jobject obj, jstring url) {
  return Settings()->CanUseDataReductionProxy(
      GURL(base::android::ConvertJavaStringToUTF16(env, url)));
}

jboolean DataReductionProxySettingsAndroid::WasLoFiModeActiveOnMainFrame(
    JNIEnv* env, jobject obj) {
  return Settings()->WasLoFiModeActiveOnMainFrame();
}

jboolean DataReductionProxySettingsAndroid::WasLoFiLoadImageRequestedBefore(
    JNIEnv* env, jobject obj) {
  return Settings()->WasLoFiLoadImageRequestedBefore();
}

void DataReductionProxySettingsAndroid::SetLoFiLoadImageRequested(
    JNIEnv* env, jobject obj) {
  Settings()->SetLoFiLoadImageRequested();
}

jboolean DataReductionProxySettingsAndroid::IsDataReductionProxyManaged(
    JNIEnv* env, jobject obj) {
  return Settings()->IsDataReductionProxyManaged();
}

void DataReductionProxySettingsAndroid::IncrementLoFiSnackbarShown(
    JNIEnv* env, jobject obj) {
  Settings()->IncrementLoFiSnackbarShown();
}

void DataReductionProxySettingsAndroid::IncrementLoFiUserRequestsForImages(
    JNIEnv* env, jobject obj) {
  Settings()->IncrementLoFiUserRequestsForImages();
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
    jobject obj) {
  data_reduction_proxy::DataReductionProxyEventStore* event_store =
      Settings()->GetEventStore();
  if (!event_store)
    return ConvertUTF8ToJavaString(env, std::string());

  return ConvertUTF8ToJavaString(env, event_store->GetHttpProxyList());
}

ScopedJavaLocalRef<jstring>
DataReductionProxySettingsAndroid::GetHttpsProxyList(JNIEnv* env, jobject obj) {
  data_reduction_proxy::DataReductionProxyEventStore* event_store =
      Settings()->GetEventStore();
  if (!event_store)
    return ConvertUTF8ToJavaString(env, std::string());

  return ConvertUTF8ToJavaString(env, event_store->GetHttpsProxyList());
}

DataReductionProxySettings* DataReductionProxySettingsAndroid::Settings() {
  DataReductionProxySettings* settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          ProfileManager::GetActiveUserProfile());
  DCHECK(settings);
  return settings;
}

ScopedJavaLocalRef<jstring>
DataReductionProxySettingsAndroid::GetLastBypassEvent(JNIEnv* env,
                                                      jobject obj) {
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
