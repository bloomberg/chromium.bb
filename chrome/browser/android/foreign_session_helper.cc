// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/foreign_session_helper.h"

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sync/open_tabs_ui_delegate.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "jni/ForeignSessionHelper_jni.h"

using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertJavaStringToUTF8;
using browser_sync::OpenTabsUIDelegate;
using browser_sync::SyncedSession;

namespace {

OpenTabsUIDelegate* GetOpenTabsUIDelegate(Profile* profile) {
  ProfileSyncService* service = ProfileSyncServiceFactory::GetInstance()->
      GetForProfile(profile);

  // Only return the delegate if it exists and it is done syncing sessions.
  if (!service || !service->ShouldPushChanges())
    return NULL;

  return service->GetOpenTabsUIDelegate();
}

bool ShouldSkipTab(const SessionTab& session_tab) {
    if (session_tab.navigations.empty())
      return true;

    int selected_index = session_tab.normalized_navigation_index();
    const ::sessions::SerializedNavigationEntry& current_navigation =
        session_tab.navigations.at(selected_index);

    if (current_navigation.virtual_url().is_empty())
      return true;

    return false;
}

bool ShouldSkipWindow(const SessionWindow& window) {
  for (std::vector<SessionTab*>::const_iterator tab_it = window.tabs.begin();
      tab_it != window.tabs.end(); ++tab_it) {
    const SessionTab &session_tab = **tab_it;
    if (!ShouldSkipTab(session_tab))
      return false;
  }
  return true;
}

bool ShouldSkipSession(const browser_sync::SyncedSession& session) {
  for (SyncedSession::SyncedWindowMap::const_iterator it =
      session.windows.begin(); it != session.windows.end(); ++it) {
    const SessionWindow  &window = *(it->second);
    if (!ShouldSkipWindow(window))
      return false;
  }
  return true;
}

void CopyTabsToJava(
    JNIEnv* env,
    const SessionWindow& window,
    ScopedJavaLocalRef<jobject>& j_window) {
  for (std::vector<SessionTab*>::const_iterator tab_it = window.tabs.begin();
      tab_it != window.tabs.end(); ++tab_it) {
    const SessionTab &session_tab = **tab_it;

    if (ShouldSkipTab(session_tab))
      continue;

    int selected_index = session_tab.normalized_navigation_index();
    DCHECK(selected_index >= 0);
    DCHECK(selected_index < static_cast<int>(session_tab.navigations.size()));

    const ::sessions::SerializedNavigationEntry& current_navigation =
        session_tab.navigations.at(selected_index);

    GURL tab_url = current_navigation.virtual_url();

    Java_ForeignSessionHelper_pushTab(
        env, j_window.obj(),
        ConvertUTF8ToJavaString(env, tab_url.spec()).obj(),
        ConvertUTF16ToJavaString(env, current_navigation.title()).obj(),
        session_tab.timestamp.ToJavaTime(),
        session_tab.tab_id.id());
  }
}

void CopyWindowsToJava(
    JNIEnv* env,
    const SyncedSession& session,
    ScopedJavaLocalRef<jobject>& j_session) {
  for (SyncedSession::SyncedWindowMap::const_iterator it =
      session.windows.begin(); it != session.windows.end(); ++it) {
    const SessionWindow &window = *(it->second);

    if (ShouldSkipWindow(window))
      continue;

    ScopedJavaLocalRef<jobject> last_pushed_window;
    last_pushed_window.Reset(
        Java_ForeignSessionHelper_pushWindow(
            env, j_session.obj(),
            window.timestamp.ToJavaTime(),
            window.window_id.id()));

    CopyTabsToJava(env, window, last_pushed_window);
  }
}

}  // namespace

static jlong Init(JNIEnv* env, jclass clazz, jobject profile) {
  ForeignSessionHelper* foreign_session_helper = new ForeignSessionHelper(
      ProfileAndroid::FromProfileAndroid(profile));
  return reinterpret_cast<intptr_t>(foreign_session_helper);
}

ForeignSessionHelper::ForeignSessionHelper(Profile* profile)
    : profile_(profile) {
  ProfileSyncService* service = ProfileSyncServiceFactory::GetInstance()->
      GetForProfile(profile);
  registrar_.Add(this, chrome::NOTIFICATION_SYNC_CONFIGURE_DONE,
                 content::Source<ProfileSyncService>(service));
  registrar_.Add(this, chrome::NOTIFICATION_FOREIGN_SESSION_UPDATED,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_FOREIGN_SESSION_DISABLED,
                 content::Source<Profile>(profile));
}

ForeignSessionHelper::~ForeignSessionHelper() {
}

void ForeignSessionHelper::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

jboolean ForeignSessionHelper::IsTabSyncEnabled(JNIEnv* env, jobject obj) {
  ProfileSyncService* service = ProfileSyncServiceFactory::GetInstance()->
      GetForProfile(profile_);
  return service && service->GetActiveDataTypes().Has(syncer::PROXY_TABS);
}

void ForeignSessionHelper::SetOnForeignSessionCallback(JNIEnv* env,
                                                       jobject obj,
                                                       jobject callback) {
  callback_.Reset(env, callback);
}

void ForeignSessionHelper::Observe(
    int type, const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (callback_.is_null())
    return;

  JNIEnv* env = AttachCurrentThread();

  switch (type) {
    case chrome::NOTIFICATION_FOREIGN_SESSION_DISABLED:
      // Tab sync is disabled, so clean up data about collapsed sessions.
      profile_->GetPrefs()->ClearPref(
          prefs::kNtpCollapsedForeignSessions);
      // Purposeful fall through.
    case chrome::NOTIFICATION_SYNC_CONFIGURE_DONE:
    case chrome::NOTIFICATION_FOREIGN_SESSION_UPDATED:
      Java_ForeignSessionCallback_onUpdated(env, callback_.obj());
      break;
    default:
      NOTREACHED();
  }
}

jboolean ForeignSessionHelper::GetForeignSessions(JNIEnv* env,
                                                  jobject obj,
                                                  jobject result) {
  OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate(profile_);
  if (!open_tabs)
    return false;

  std::vector<const browser_sync::SyncedSession*> sessions;
  if (!open_tabs->GetAllForeignSessions(&sessions))
    return false;

  // Use a pref to keep track of sessions that were collapsed by the user.
  // To prevent the pref from accumulating stale sessions, clear it each time
  // and only add back sessions that are still current.
  DictionaryPrefUpdate pref_update(profile_->GetPrefs(),
                                   prefs::kNtpCollapsedForeignSessions);
  base::DictionaryValue* pref_collapsed_sessions = pref_update.Get();
  scoped_ptr<base::DictionaryValue> collapsed_sessions(
      pref_collapsed_sessions->DeepCopy());
  pref_collapsed_sessions->Clear();

  ScopedJavaLocalRef<jobject> last_pushed_session;
  ScopedJavaLocalRef<jobject> last_pushed_window;

  // Note: we don't own the SyncedSessions themselves.
  for (size_t i = 0; i < sessions.size(); ++i) {
    const browser_sync::SyncedSession &session = *(sessions[i]);
    if (ShouldSkipSession(session))
      continue;

    const bool is_collapsed = collapsed_sessions->HasKey(session.session_tag);

    if (is_collapsed)
      pref_collapsed_sessions->SetBoolean(session.session_tag, true);

    last_pushed_session.Reset(
        Java_ForeignSessionHelper_pushSession(
            env,
            result,
            ConvertUTF8ToJavaString(env, session.session_tag).obj(),
            ConvertUTF8ToJavaString(env, session.session_name).obj(),
            session.device_type,
            session.modified_time.ToJavaTime()));

    CopyWindowsToJava(env, session, last_pushed_session);
  }

  return true;
}

jboolean ForeignSessionHelper::OpenForeignSessionTab(JNIEnv* env,
                                                     jobject obj,
                                                     jobject j_tab,
                                                     jstring session_tag,
                                                     jint session_tab_id,
                                                     jint j_disposition) {
  OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate(profile_);
  if (!open_tabs) {
    LOG(ERROR) << "Null OpenTabsUIDelegate returned.";
    return false;
  }

  const SessionTab* session_tab;

  if (!open_tabs->GetForeignTab(ConvertJavaStringToUTF8(env, session_tag),
                                session_tab_id,
                                &session_tab)) {
    LOG(ERROR) << "Failed to load foreign tab.";
    return false;
  }

  if (session_tab->navigations.empty()) {
    LOG(ERROR) << "Foreign tab no longer has valid navigations.";
    return false;
  }

  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, j_tab);
  if (!tab_android)
    return false;
  content::WebContents* web_contents = tab_android->web_contents();
  if (!web_contents)
    return false;

  WindowOpenDisposition disposition =
      static_cast<WindowOpenDisposition>(j_disposition);
  SessionRestore::RestoreForeignSessionTab(web_contents,
                                           *session_tab,
                                           disposition);

  return true;
}

void ForeignSessionHelper::DeleteForeignSession(JNIEnv* env, jobject obj,
                                                jstring session_tag) {
  OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate(profile_);
  if (open_tabs)
    open_tabs->DeleteForeignSession(ConvertJavaStringToUTF8(env, session_tag));
}

// static
bool ForeignSessionHelper::RegisterForeignSessionHelper(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
