// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/new_tab_page_prefs.h"

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "jni/NewTabPagePrefs_jni.h"

using base::android::ConvertJavaStringToUTF8;

static jlong Init(JNIEnv* env, jclass clazz, jobject profile) {
  NewTabPagePrefs* new_tab_page_prefs =
      new NewTabPagePrefs(ProfileAndroid::FromProfileAndroid(profile));
  return reinterpret_cast<intptr_t>(new_tab_page_prefs);
}

NewTabPagePrefs::NewTabPagePrefs(Profile* profile)
  : profile_(profile) {
}

void NewTabPagePrefs::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

NewTabPagePrefs::~NewTabPagePrefs() {
}

jboolean NewTabPagePrefs::GetCurrentlyOpenTabsCollapsed(JNIEnv* env,
                                                       jobject obj) {
  PrefService* prefs = profile_->GetPrefs();
  return prefs->GetBoolean(prefs::kNtpCollapsedCurrentlyOpenTabs);
}

void NewTabPagePrefs::SetCurrentlyOpenTabsCollapsed(JNIEnv* env,
                                                   jobject obj,
                                                   jboolean is_collapsed) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(prefs::kNtpCollapsedCurrentlyOpenTabs, is_collapsed);
}

jboolean NewTabPagePrefs::GetSnapshotDocumentCollapsed(JNIEnv* env,
                                                       jobject obj) {
  return profile_->GetPrefs()->GetBoolean(prefs::kNtpCollapsedSnapshotDocument);
}

void NewTabPagePrefs::SetSnapshotDocumentCollapsed(JNIEnv* env,
                                                   jobject obj,
                                                   jboolean is_collapsed) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(prefs::kNtpCollapsedSnapshotDocument, is_collapsed);
}

jboolean NewTabPagePrefs::GetRecentlyClosedTabsCollapsed(JNIEnv* env,
                                                         jobject obj) {
  return profile_->GetPrefs()->GetBoolean(
      prefs::kNtpCollapsedRecentlyClosedTabs);
}

void NewTabPagePrefs::SetRecentlyClosedTabsCollapsed(JNIEnv* env,
                                                     jobject obj,
                                                     jboolean is_collapsed) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(prefs::kNtpCollapsedRecentlyClosedTabs, is_collapsed);
}

jboolean NewTabPagePrefs::GetSyncPromoCollapsed(JNIEnv* env,
                                                       jobject obj) {
  return profile_->GetPrefs()->GetBoolean(prefs::kNtpCollapsedSyncPromo);
}

void NewTabPagePrefs::SetSyncPromoCollapsed(JNIEnv* env,
                                            jobject obj,
                                            jboolean is_collapsed) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(prefs::kNtpCollapsedSyncPromo, is_collapsed);
}

jboolean NewTabPagePrefs::GetForeignSessionCollapsed(JNIEnv* env,
                                                     jobject obj,
                                                     jstring session_tag) {
  const base::DictionaryValue* dict = profile_->GetPrefs()->GetDictionary(
      prefs::kNtpCollapsedForeignSessions);
  return dict && dict->HasKey(ConvertJavaStringToUTF8(env, session_tag));
}

void NewTabPagePrefs::SetForeignSessionCollapsed(JNIEnv* env,
                                                 jobject obj,
                                                 jstring session_tag,
                                                 jboolean is_collapsed) {
  // Store session tags for collapsed sessions in a preference so that the
  // collapsed state persists.
  PrefService* prefs = profile_->GetPrefs();
  DictionaryPrefUpdate update(prefs, prefs::kNtpCollapsedForeignSessions);
  if (is_collapsed)
    update.Get()->SetBoolean(ConvertJavaStringToUTF8(env, session_tag), true);
  else
    update.Get()->Remove(ConvertJavaStringToUTF8(env, session_tag), NULL);
}

// static
void NewTabPagePrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kNtpCollapsedCurrentlyOpenTabs,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kNtpCollapsedSnapshotDocument,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kNtpCollapsedRecentlyClosedTabs,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kNtpCollapsedSyncPromo,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kNtpCollapsedForeignSessions,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kNtpMostVisitedURLsBlacklist,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

// static
bool NewTabPagePrefs::RegisterNewTabPagePrefs(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
