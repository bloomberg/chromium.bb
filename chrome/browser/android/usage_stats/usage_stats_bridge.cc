// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/usage_stats/usage_stats_bridge.h"

#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/usage_stats/usage_stats_database.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "jni/UsageStatsBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;

namespace usage_stats {

namespace {

bool isSuccess(UsageStatsDatabase::Error error) {
  return error == UsageStatsDatabase::Error::kNoError;
}

}  // namespace

static jlong JNI_UsageStatsBridge_Init(JNIEnv* env,
                                       const JavaParamRef<jobject>& j_this,
                                       const JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);

  std::unique_ptr<UsageStatsDatabase> usage_stats_database =
      std::make_unique<UsageStatsDatabase>(profile);

  UsageStatsBridge* native_usage_stats_bridge =
      new UsageStatsBridge(std::move(usage_stats_database));

  return reinterpret_cast<intptr_t>(native_usage_stats_bridge);
}

UsageStatsBridge::UsageStatsBridge(
    std::unique_ptr<UsageStatsDatabase> usage_stats_database)
    : usage_stats_database_(std::move(usage_stats_database)),
      weak_ptr_factory_(this) {}

UsageStatsBridge::~UsageStatsBridge() = default;

void UsageStatsBridge::Destroy(JNIEnv* env, const JavaRef<jobject>& j_this) {
  delete this;
}

void UsageStatsBridge::GetAllEvents(JNIEnv* j_env,
                                    const JavaRef<jobject>& j_this,
                                    const JavaRef<jobject>& j_callback) {}

void UsageStatsBridge::QueryEventsInRange(JNIEnv* j_env,
                                          const JavaRef<jobject>& j_this,
                                          const jlong j_start,
                                          const jlong j_end,
                                          const JavaRef<jobject>& j_callback) {}

void UsageStatsBridge::AddEvents(JNIEnv* j_env,
                                 const JavaRef<jobject>& j_this,
                                 const JavaRef<jobject>& j_events,
                                 const JavaRef<jobject>& j_callback) {}

void UsageStatsBridge::DeleteAllEvents(JNIEnv* j_env,
                                       const JavaRef<jobject>& j_this,
                                       const JavaRef<jobject>& j_callback) {}

void UsageStatsBridge::DeleteEventsInRange(JNIEnv* j_env,
                                           const JavaRef<jobject>& j_this,
                                           const jlong j_start,
                                           const jlong j_end,
                                           const JavaRef<jobject>& j_callback) {
}

void UsageStatsBridge::DeleteEventsWithMatchingDomains(
    JNIEnv* j_env,
    const JavaRef<jobject>& j_this,
    const JavaRef<jobjectArray>& j_domains,
    const JavaRef<jobject>& j_callback) {}

void UsageStatsBridge::GetAllSuspensions(JNIEnv* j_env,
                                         const JavaRef<jobject>& j_this,
                                         const JavaRef<jobject>& j_callback) {
  ScopedJavaGlobalRef<jobject> callback(j_callback);

  usage_stats_database_->GetAllSuspensions(
      base::BindOnce(&UsageStatsBridge::OnGetAllSuspensionsDone,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void UsageStatsBridge::SetSuspensions(JNIEnv* j_env,
                                      const JavaRef<jobject>& j_this,
                                      const JavaRef<jobjectArray>& j_domains,
                                      const JavaRef<jobject>& j_callback) {
  std::vector<std::string> domains;
  AppendJavaStringArrayToStringVector(j_env, j_domains, &domains);

  ScopedJavaGlobalRef<jobject> callback(j_callback);

  usage_stats_database_->SetSuspensions(
      domains, base::BindOnce(&UsageStatsBridge::OnUpdateDone,
                              weak_ptr_factory_.GetWeakPtr(), callback));
}

void UsageStatsBridge::GetAllTokenMappings(JNIEnv* j_env,
                                           const JavaRef<jobject>& j_this,
                                           const JavaRef<jobject>& j_callback) {
  ScopedJavaGlobalRef<jobject> callback(j_callback);

  usage_stats_database_->GetAllTokenMappings(
      base::BindOnce(&UsageStatsBridge::OnGetAllTokenMappingsDone,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void UsageStatsBridge::SetTokenMappings(JNIEnv* j_env,
                                        const JavaRef<jobject>& j_this,
                                        const JavaRef<jobjectArray>& j_tokens,
                                        const JavaRef<jobjectArray>& j_fqdns,
                                        const JavaRef<jobject>& j_callback) {
  ScopedJavaGlobalRef<jobject> callback(j_callback);

  std::vector<std::string> tokens, fqdns;
  AppendJavaStringArrayToStringVector(j_env, j_tokens, &tokens);
  AppendJavaStringArrayToStringVector(j_env, j_fqdns, &fqdns);

  DCHECK(tokens.size() == fqdns.size());

  // Zip tokens (keys) and FQDNs (values) into a map.
  UsageStatsDatabase::TokenMap mappings;
  for (size_t i = 0; i < tokens.size(); i++) {
    mappings.emplace(tokens[i], fqdns[i]);
  }

  usage_stats_database_->SetTokenMappings(
      mappings, base::BindOnce(&UsageStatsBridge::OnUpdateDone,
                               weak_ptr_factory_.GetWeakPtr(), callback));
}

void UsageStatsBridge::OnGetAllSuspensionsDone(
    ScopedJavaGlobalRef<jobject> callback,
    UsageStatsDatabase::Error error,
    std::vector<std::string> suspensions) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobjectArray> j_suspensions =
      isSuccess(error) ? ToJavaArrayOfStrings(env, suspensions)
                       : ToJavaArrayOfStrings(env, std::vector<std::string>());

  RunObjectCallbackAndroid(callback, j_suspensions);
}

void UsageStatsBridge::OnGetAllTokenMappingsDone(
    ScopedJavaGlobalRef<jobject> callback,
    UsageStatsDatabase::Error error,
    UsageStatsDatabase::TokenMap mappings) {
  JNIEnv* env = AttachCurrentThread();

  if (!isSuccess(error)) {
    RunObjectCallbackAndroid(
        callback, ToJavaArrayOfStrings(env, std::vector<std::string>()));
    return;
  }

  // Create separate vectors of keys and values from map for passing over
  // JNI bridge as String arrays.
  std::vector<std::string> keys, values;
  keys.reserve(mappings.size());
  values.reserve(mappings.size());

  for (auto mapping : mappings) {
    keys.emplace_back(std::move(mapping.first));
    values.emplace_back(std::move(mapping.second));
  }

  ScopedJavaLocalRef<jobjectArray> j_keys = ToJavaArrayOfStrings(env, keys);
  ScopedJavaLocalRef<jobjectArray> j_values = ToJavaArrayOfStrings(env, values);

  // Over JNI, reconstruct map from keys and values, and run on given callback.
  Java_UsageStatsBridge_createMapAndRunCallback(env, j_keys, j_values,
                                                callback);
}

void UsageStatsBridge::OnUpdateDone(ScopedJavaGlobalRef<jobject> callback,
                                    UsageStatsDatabase::Error error) {
  RunBooleanCallbackAndroid(callback, isSuccess(error));
}

// static
void UsageStatsBridge::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kUsageStatsEnabled, false);
}

}  // namespace usage_stats
