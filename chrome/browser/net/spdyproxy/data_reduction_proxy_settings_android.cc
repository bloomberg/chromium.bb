// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_settings_android.h"

#include <stdint.h>

#include <map>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/browser/data_usage_store.h"
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

namespace {

constexpr size_t kBucketsPerDay =
    24 * 60 / data_reduction_proxy::kDataUsageBucketLengthInMinutes;

struct DataUsageForHost {
  DataUsageForHost() : data_used(0), original_size(0) {}

  int64_t data_used;
  int64_t original_size;
};

}  // namespace

DataReductionProxySettingsAndroid::DataReductionProxySettingsAndroid()
    : weak_factory_(this) {}

DataReductionProxySettingsAndroid::DataReductionProxySettingsAndroid(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj)
    : weak_factory_(this) {
  j_settings_obj_ = JavaObjectWeakGlobalRef(env, obj);
}

DataReductionProxySettingsAndroid::~DataReductionProxySettingsAndroid() {
}

jboolean DataReductionProxySettingsAndroid::IsDataReductionProxyPromoAllowed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return data_reduction_proxy::params::IsIncludedInPromoFieldTrial();
}

jboolean DataReductionProxySettingsAndroid::IsDataReductionProxyFREPromoAllowed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return data_reduction_proxy::params::IsIncludedInFREPromoFieldTrial();
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

void DataReductionProxySettingsAndroid::ClearDataSavingStatistics(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  Settings()->ClearDataSavingStatistics();
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

void DataReductionProxySettingsAndroid::QueryDataUsage(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_result_obj,
    jint num_days) {
  DCHECK(num_days <= data_reduction_proxy::kDataUsageHistoryNumDays);
  j_query_result_obj_.Reset(env, j_result_obj);
  num_day_for_query_ = num_days;
  Settings()
      ->data_reduction_proxy_service()
      ->compression_stats()
      ->GetHistoricalDataUsage(base::Bind(
          &DataReductionProxySettingsAndroid::OnQueryDataUsageComplete,
          weak_factory_.GetWeakPtr()));
}

void DataReductionProxySettingsAndroid::OnQueryDataUsageComplete(
    std::unique_ptr<std::vector<data_reduction_proxy::DataUsageBucket>>
        data_usage) {
  if (j_query_result_obj_.is_null())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();

  std::map<base::StringPiece, DataUsageForHost> per_site_usage_map;

  // Data usage is sorted chronologically with the last entry corresponding to
  // |base::Time::Now()|.
  const size_t num_buckets_to_display = num_day_for_query_ * kBucketsPerDay;
  for (auto data_usage_it = data_usage->size() > num_buckets_to_display
                                ? data_usage->end() - num_buckets_to_display
                                : data_usage->begin();
       data_usage_it != data_usage->end(); ++data_usage_it) {
    for (const auto& connection_usage : data_usage_it->connection_usage()) {
      for (const auto& site_usage : connection_usage.site_usage()) {
        DataUsageForHost& usage = per_site_usage_map[site_usage.hostname()];
        usage.data_used += site_usage.data_used();
        usage.original_size += site_usage.original_size();
      }
    }
  }

  for (const auto& site_bucket : per_site_usage_map) {
    Java_DataReductionProxySettings_createDataUseItemAndAddToList(
        env, j_query_result_obj_.obj(),
        ConvertUTF8ToJavaString(env, site_bucket.first),
        site_bucket.second.data_used, site_bucket.second.original_size);
  }

  Java_DataReductionProxySettings_onQueryDataUsageComplete(
      env, j_settings_obj_.get(env), j_query_result_obj_.obj());

  j_query_result_obj_.Release();
}

// Used by generated jni code.
static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(
      new DataReductionProxySettingsAndroid(env, obj));
}
