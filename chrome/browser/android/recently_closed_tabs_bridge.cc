// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/recently_closed_tabs_bridge.h"

#include "base/android/jni_string.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "components/sessions/core/tab_restore_service.h"
#include "content/public/browser/web_contents.h"
#include "jni/RecentlyClosedBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace {

void AddTabToList(JNIEnv* env,
                  sessions::TabRestoreService::Entry* entry,
                  jobject jtabs_list) {
  const sessions::TabRestoreService::Tab* tab =
      static_cast<sessions::TabRestoreService::Tab*>(entry);
  const sessions::SerializedNavigationEntry& current_navigation =
      tab->navigations.at(tab->current_navigation_index);
  Java_RecentlyClosedBridge_pushTab(
      env, jtabs_list, entry->id,
      ConvertUTF16ToJavaString(env, current_navigation.title()).obj(),
      ConvertUTF8ToJavaString(env, current_navigation.virtual_url().spec())
          .obj());
}

void AddTabsToList(JNIEnv* env,
                   const sessions::TabRestoreService::Entries& entries,
                   jobject jtabs_list,
                   int max_tab_count) {
  int added_count = 0;
  for (sessions::TabRestoreService::Entries::const_iterator it =
           entries.begin();
       it != entries.end() && added_count < max_tab_count; ++it) {
    sessions::TabRestoreService::Entry* entry = *it;
    DCHECK_EQ(entry->type, sessions::TabRestoreService::TAB);
    if (entry->type == sessions::TabRestoreService::TAB) {
      AddTabToList(env, entry, jtabs_list);
      ++added_count;
    }
  }
}

}  // namespace

RecentlyClosedTabsBridge::RecentlyClosedTabsBridge(Profile* profile)
    : profile_(profile),
      tab_restore_service_(NULL) {
}

RecentlyClosedTabsBridge::~RecentlyClosedTabsBridge() {
  if (tab_restore_service_)
    tab_restore_service_->RemoveObserver(this);
}

void RecentlyClosedTabsBridge::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void RecentlyClosedTabsBridge::SetRecentlyClosedCallback(JNIEnv* env,
                                                         jobject obj,
                                                         jobject jcallback) {
  callback_.Reset(env, jcallback);
}

jboolean RecentlyClosedTabsBridge::GetRecentlyClosedTabs(JNIEnv* env,
                                                         jobject obj,
                                                         jobject jtabs_list,
                                                         jint max_tab_count) {
  EnsureTabRestoreService();
  if (!tab_restore_service_)
    return false;

  AddTabsToList(env, tab_restore_service_->entries(), jtabs_list,
                max_tab_count);
  return true;
}

jboolean RecentlyClosedTabsBridge::OpenRecentlyClosedTab(JNIEnv* env,
                                                         jobject obj,
                                                         jobject jtab,
                                                         jint recent_tab_id,
                                                         jint j_disposition) {
  if (!tab_restore_service_)
    return false;

  // Find and remove the corresponding tab entry from TabRestoreService.
  // We take ownership of the returned tab.
  scoped_ptr<sessions::TabRestoreService::Tab> tab_entry(
      tab_restore_service_->RemoveTabEntryById(recent_tab_id));
  if (!tab_entry)
    return false;

  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, jtab);
  if (!tab_android)
    return false;
  content::WebContents* web_contents = tab_android->web_contents();
  if (!web_contents)
    return false;

  // RestoreForeignSessionTab needs a SessionTab.
  sessions::SessionTab session_tab;
  session_tab.current_navigation_index = tab_entry->current_navigation_index;
  session_tab.navigations = tab_entry->navigations;

  WindowOpenDisposition disposition =
      static_cast<WindowOpenDisposition>(j_disposition);
  SessionRestore::RestoreForeignSessionTab(web_contents,
                                           session_tab,
                                           disposition);
  return true;
}

void RecentlyClosedTabsBridge::ClearRecentlyClosedTabs(JNIEnv* env,
                                                       jobject obj) {
  EnsureTabRestoreService();
  if (tab_restore_service_)
    tab_restore_service_->ClearEntries();
}

void RecentlyClosedTabsBridge::TabRestoreServiceChanged(
    sessions::TabRestoreService* service) {
  if (callback_.is_null())
    return;
  JNIEnv* env = AttachCurrentThread();
  Java_RecentlyClosedCallback_onUpdated(env, callback_.obj());
}

void RecentlyClosedTabsBridge::TabRestoreServiceDestroyed(
    sessions::TabRestoreService* service) {
  tab_restore_service_ = NULL;
}

void RecentlyClosedTabsBridge::EnsureTabRestoreService() {
  if (tab_restore_service_)
    return;

  tab_restore_service_ = TabRestoreServiceFactory::GetForProfile(profile_);

  // TabRestoreServiceFactory::GetForProfile() can return NULL (e.g. in
  // incognito mode).
  if (tab_restore_service_) {
    // This does nothing if the tabs have already been loaded or they
    // shouldn't be loaded.
    tab_restore_service_->LoadTabsFromLastSession();
    tab_restore_service_->AddObserver(this);
  }
}

static jlong Init(JNIEnv* env,
                  const JavaParamRef<jobject>& obj,
                  const JavaParamRef<jobject>& jprofile) {
  RecentlyClosedTabsBridge* bridge = new RecentlyClosedTabsBridge(
      ProfileAndroid::FromProfileAndroid(jprofile));
  return reinterpret_cast<intptr_t>(bridge);
}

// static
bool RecentlyClosedTabsBridge::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
