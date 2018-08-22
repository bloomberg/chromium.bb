// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/feed_journal_bridge.h"

#include <jni.h>

#include <list>
#include <memory>
#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/android/feed/feed_host_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/feed/core/feed_host_service.h"
#include "components/feed/core/feed_journal_database.h"
#include "jni/FeedJournalBridge_jni.h"

namespace feed {

using base::android::JavaRef;
using base::android::JavaParamRef;

static jlong JNI_FeedJournalBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_this,
    const JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  FeedHostService* host_service =
      FeedHostServiceFactory::GetForBrowserContext(profile);
  DCHECK(host_service);
  FeedJournalDatabase* feed_journal_database =
      host_service->GetJournalDatabase();
  DCHECK(feed_journal_database);
  FeedJournalBridge* native_journal_bridge =
      new FeedJournalBridge(feed_journal_database);
  return reinterpret_cast<intptr_t>(native_journal_bridge);
}

FeedJournalBridge::FeedJournalBridge(FeedJournalDatabase* feed_journal_database)
    : feed_journal_database_(feed_journal_database), weak_ptr_factory_(this) {
  DCHECK(feed_journal_database_);
}

FeedJournalBridge::~FeedJournalBridge() = default;

void FeedJournalBridge::Destroy(JNIEnv* env, const JavaRef<jobject>& j_this) {
  delete this;
}

void FeedJournalBridge::LoadJournal(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jstring>& j_journal_name,
    const base::android::JavaRef<jobject>& j_callback) {}

void FeedJournalBridge::CommitJournalMutation(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jobject>& j_callback) {}

void FeedJournalBridge::DoesJournalExist(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jstring>& j_journal_name,
    const base::android::JavaRef<jobject>& j_callback) {}

void FeedJournalBridge::LoadAllJournalKeys(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jobject>& j_callback) {}

void FeedJournalBridge::DeleteAllJournals(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jobject>& j_callback) {}

void FeedJournalBridge::CreateJournalMutation(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jstring>& j_journal_name) {}

void FeedJournalBridge::DeleteJournalMutation(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this) {}

void FeedJournalBridge::AddAppendOperation(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jstring>& j_value) {}

void FeedJournalBridge::AddCopyOperation(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jstring>& j_to_journal_name) {}

void FeedJournalBridge::AddDeleteOperation(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this) {}

}  // namespace feed
