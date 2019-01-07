// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/usage_stats/usage_stats_bridge.h"

#include <utility>

#include "chrome/browser/android/usage_stats/usage_stats_database.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "jni/UsageStatsBridge_jni.h"

namespace usage_stats {

using base::android::JavaParamRef;
using base::android::JavaRef;

const char kUsageStatsFolder[] = "usage_stats";

static jlong JNI_UsageStatsBridge_Init(JNIEnv* env,
                                       const JavaParamRef<jobject>& j_this,
                                       const JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  base::FilePath usage_stats_dir(profile->GetPath().Append(kUsageStatsFolder));

  std::unique_ptr<UsageStatsDatabase> usage_stats_database =
      std::make_unique<UsageStatsDatabase>(usage_stats_dir);

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

void UsageStatsBridge::GetAllEvents(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jobject>& j_callback) {}

void UsageStatsBridge::QueryEventsInRange(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const jlong j_start,
    const jlong j_end,
    const base::android::JavaRef<jobject>& j_callback) {}

void UsageStatsBridge::AddEvents(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jobjectArray>& j_events,
    const base::android::JavaRef<jobject>& j_callback) {}

void UsageStatsBridge::DeleteAllEvents(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jobject>& j_callback) {}

void UsageStatsBridge::DeleteEventsInRange(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const jlong j_start,
    const jlong j_end,
    const base::android::JavaRef<jobject>& j_callback) {}

void UsageStatsBridge::DeleteEventsWithMatchingDomains(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jobjectArray>& j_domains,
    const base::android::JavaRef<jobject>& j_callback) {}

void UsageStatsBridge::GetAllSuspensions(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jobject>& j_callback) {}

void UsageStatsBridge::SetSuspensions(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jobjectArray>& j_domains,
    const base::android::JavaRef<jobject>& j_callback) {}

void UsageStatsBridge::GetAllTokenMappings(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jobject>& j_callback) {}

void UsageStatsBridge::SetTokenMappings(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jobject>& j_mappings,
    const base::android::JavaRef<jobject>& j_callback) {}

}  // namespace usage_stats
