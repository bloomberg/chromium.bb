// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/history/browsing_history_bridge.h"

#include <jni.h>

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/url_formatter/url_formatter.h"
#include "jni/BrowsingHistoryBridge_jni.h"

const int kMaxQueryCount = 150;

BrowsingHistoryBridge::BrowsingHistoryBridge(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jobject j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  browsing_history_service_.reset(new BrowsingHistoryService(profile, this));
  j_history_service_obj_.Reset(env, obj);
}

BrowsingHistoryBridge::~BrowsingHistoryBridge() {}

void BrowsingHistoryBridge::Destroy(JNIEnv*, const JavaParamRef<jobject>&) {
  delete this;
}

void BrowsingHistoryBridge::QueryHistory(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_result_obj,
    jstring j_query,
    int64_t j_query_end_time) {
  j_query_result_obj_.Reset(env, j_result_obj);

  history::QueryOptions options;
  options.max_count = kMaxQueryCount;
  if (j_query_end_time == 0) {
    options.end_time = base::Time();
  } else {
    options.end_time = base::Time::FromJavaTime(j_query_end_time);
  }
  options.duplicate_policy = history::QueryOptions::REMOVE_DUPLICATES_PER_DAY;

  browsing_history_service_->QueryHistory(
      base::android::ConvertJavaStringToUTF16(env, j_query), options);
}

// BrowsingHistoryServiceHandler implementation
void BrowsingHistoryBridge::OnQueryComplete(
    std::vector<BrowsingHistoryService::HistoryEntry>* results,
    BrowsingHistoryService::QueryResultsInfo* query_results_info) {

  JNIEnv* env = base::android::AttachCurrentThread();

  for (const BrowsingHistoryService::HistoryEntry& entry : *results) {
    // TODO(twellington): move the domain logic to BrowsingHistoryServce so it
    // can be shared with BrowsingHistoryHandler.
    base::string16 domain = url_formatter::IDNToUnicode(entry.url.host());
    // When the domain is empty, use the scheme instead. This allows for a
    // sensible treatment of e.g. file: URLs when group by domain is on.
    if (domain.empty())
      domain = base::UTF8ToUTF16(entry.url.scheme() + ":");

    // This relies on |all_timestamps| being a sorted data structure.
    int64_t most_recent_java_timestamp =
        base::Time::FromInternalValue(*entry.all_timestamps.rbegin())
            .ToJavaTime();
    std::vector<int64_t> native_timestamps(entry.all_timestamps.begin(),
                                           entry.all_timestamps.end());

    Java_BrowsingHistoryBridge_createHistoryItemAndAddToList(
        env, j_query_result_obj_.obj(),
        base::android::ConvertUTF8ToJavaString(env, entry.url.spec()),
        base::android::ConvertUTF16ToJavaString(env, domain),
        base::android::ConvertUTF16ToJavaString(env, entry.title),
        most_recent_java_timestamp,
        base::android::ToJavaLongArray(env, native_timestamps),
        entry.blocked_visit);
  }

  Java_BrowsingHistoryBridge_onQueryHistoryComplete(
      env,
      j_history_service_obj_.obj(),
      j_query_result_obj_.obj(),
      !(query_results_info->reached_beginning));
}

void BrowsingHistoryBridge::MarkItemForRemoval(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jstring j_url,
    const JavaParamRef<jlongArray>& j_native_timestamps) {
  std::unique_ptr<BrowsingHistoryService::HistoryEntry> entry(
      new BrowsingHistoryService::HistoryEntry());
  entry->url = GURL(base::android::ConvertJavaStringToUTF16(env, j_url));

  std::vector<int64_t> timestamps;
  base::android::JavaLongArrayToInt64Vector(env, j_native_timestamps.obj(),
                                            &timestamps);
  entry->all_timestamps.insert(timestamps.begin(), timestamps.end());

  items_to_remove_.push_back(std::move(entry));
}

void BrowsingHistoryBridge::RemoveItems(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  browsing_history_service_->RemoveVisits(&items_to_remove_);
  items_to_remove_.clear();
}

void BrowsingHistoryBridge::OnRemoveVisitsComplete() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BrowsingHistoryBridge_onRemoveComplete(env,
                                              j_history_service_obj_.obj());
}

void BrowsingHistoryBridge::OnRemoveVisitsFailed() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BrowsingHistoryBridge_onRemoveFailed(env, j_history_service_obj_.obj());
}

void BrowsingHistoryBridge::HistoryDeleted() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BrowsingHistoryBridge_onHistoryDeleted(env,
                                              j_history_service_obj_.obj());
}

void BrowsingHistoryBridge::HasOtherFormsOfBrowsingHistory(
    bool has_other_forms, bool has_synced_results) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BrowsingHistoryBridge_hasOtherFormsOfBrowsingData(
      env, j_history_service_obj_.obj(), has_other_forms, has_synced_results);
}

bool RegisterBrowsingHistoryBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static jlong Init(JNIEnv* env,
                  const JavaParamRef<jobject>& obj,
                  const JavaParamRef<jobject>& j_profile) {
  BrowsingHistoryBridge* bridge =
      new BrowsingHistoryBridge(env, obj, j_profile);
  return reinterpret_cast<intptr_t>(bridge);
}
