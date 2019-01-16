// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/usage_stats/usage_stats_bridge.h"

#include <utility>

#include "chrome/browser/android/usage_stats/usage_stats_database.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "jni/UsageStatsBridge_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace usage_stats {

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
                                 const JavaRef<jobjectArray>& j_events,
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
                                         const JavaRef<jobject>& j_callback) {}

void UsageStatsBridge::SetSuspensions(JNIEnv* j_env,
                                      const JavaRef<jobject>& j_this,
                                      const JavaRef<jobjectArray>& j_domains,
                                      const JavaRef<jobject>& j_callback) {}

void UsageStatsBridge::GetAllTokenMappings(JNIEnv* j_env,
                                           const JavaRef<jobject>& j_this,
                                           const JavaRef<jobject>& j_callback) {
}

void UsageStatsBridge::SetTokenMappings(JNIEnv* j_env,
                                        const JavaRef<jobject>& j_this,
                                        const JavaRef<jobject>& j_mappings,
                                        const JavaRef<jobject>& j_callback) {}

// static
void UsageStatsBridge::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kUsageStatsEnabled, false);
}

}  // namespace usage_stats
