// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/foreign_session_helper.h"

#include <jni.h>

#include "base/android/jni_string.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
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
using browser_sync::SessionModelAssociator;
using browser_sync::SyncedSession;

namespace {

SessionModelAssociator* GetSessionModelAssociator(Profile* profile) {
  ProfileSyncService* service = ProfileSyncServiceFactory::GetInstance()->
      GetForProfile(profile);

  // Only return the associator if it exists and it is done syncing sessions.
  if (!service || !service->ShouldPushChanges())
    return NULL;

  return service->GetSessionModelAssociator();
}

void CopyTabsToJava(
    JNIEnv* env,
    const SessionWindow* window,
    ScopedJavaLocalRef<jobject>& j_window) {
  for (std::vector<SessionTab*>::const_iterator tab_it = window->tabs.begin();
      tab_it != window->tabs.end(); ++tab_it) {
    const SessionTab &tab = **tab_it;

    if (tab.navigations.empty())
      continue;

    const ::sessions::SerializedNavigationEntry& current_navigation =
        tab.navigations.at(tab.current_navigation_index);

    GURL tab_url = current_navigation.virtual_url();
    if (tab_url.SchemeIs(chrome::kChromeNativeScheme) ||
        (tab_url.SchemeIs(chrome::kChromeUIScheme) &&
            tab_url.host() == chrome::kChromeUINewTabHost))
      continue;

    Java_ForeignSessionHelper_pushTab(
        env, j_window.obj(),
        ConvertUTF8ToJavaString(env, tab_url.spec()).Release(),
        ConvertUTF16ToJavaString(env, current_navigation.title()).Release(),
        tab.timestamp.ToInternalValue(), tab.tab_id.id());
  }
}

void CopyWindowsToJava(
    JNIEnv* env,
    const SyncedSession* session,
    ScopedJavaLocalRef<jobject>& j_session) {
  for (SyncedSession::SyncedWindowMap::const_iterator it =
      session->windows.begin(); it != session->windows.end(); ++it) {
    const SessionWindow* window = it->second;

    ScopedJavaLocalRef<jobject> last_pushed_window;
    last_pushed_window.Reset(
        Java_ForeignSessionHelper_pushWindow(
            env, j_session.obj(), window->timestamp.ToInternalValue(),
            window->window_id.id()));

    CopyTabsToJava(env, window, last_pushed_window);
  }
}

}  // namespace

static jint Init(JNIEnv* env, jclass clazz, jobject profile) {
  ForeignSessionHelper* foreign_session_helper = new ForeignSessionHelper(
      ProfileAndroid::FromProfileAndroid(profile));
  return reinterpret_cast<jint>(foreign_session_helper);
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
  SessionModelAssociator* associator = GetSessionModelAssociator(profile_);
  if (!associator)
    return false;

  std::vector<const browser_sync::SyncedSession*> sessions;
  if (!associator->GetAllForeignSessions(&sessions))
    return false;

  // Use a pref to keep track of sessions that were collapsed by the user.
  // To prevent the pref from accumulating stale sessions, clear it each time
  // and only add back sessions that are still current.
  DictionaryPrefUpdate pref_update(profile_->GetPrefs(),
                                   prefs::kNtpCollapsedForeignSessions);
  DictionaryValue* pref_collapsed_sessions = pref_update.Get();
  scoped_ptr<DictionaryValue> collapsed_sessions(
      pref_collapsed_sessions->DeepCopy());
  pref_collapsed_sessions->Clear();

  ScopedJavaLocalRef<jobject> last_pushed_session;
  ScopedJavaLocalRef<jobject> last_pushed_window;

  // Note: we don't own the SyncedSessions themselves.
  for (size_t i = 0; i < sessions.size(); ++i) {
    const browser_sync::SyncedSession* session = sessions[i];

    const bool is_collapsed = collapsed_sessions->HasKey(session->session_tag);

    if (is_collapsed)
      pref_collapsed_sessions->SetBoolean(session->session_tag, true);

    last_pushed_session.Reset(
        Java_ForeignSessionHelper_pushSession(
            env,
            result,
            ConvertUTF8ToJavaString(env, session->session_tag).Release(),
            ConvertUTF8ToJavaString(env, session->session_name).Release(),
            ConvertUTF8ToJavaString(env,
                                    session->DeviceTypeAsString()).Release(),
            session->modified_time.ToInternalValue()));

    CopyWindowsToJava(env, session, last_pushed_session);
  }

  return true;
}

jboolean ForeignSessionHelper::OpenForeignSessionTab(JNIEnv* env,
                                                     jobject obj,
                                                     jstring session_tag,
                                                     jint tab_id) {
  SessionModelAssociator* associator = GetSessionModelAssociator(profile_);
  if (!associator) {
    LOG(ERROR) << "Null SessionModelAssociator returned.";
    return false;
  }

  const SessionTab* tab;

  if (!associator->GetForeignTab(ConvertJavaStringToUTF8(env, session_tag),
                                 tab_id, &tab)) {
    LOG(ERROR) << "Failed to load foreign tab.";
    return false;
  }

  if (tab->navigations.empty()) {
    LOG(ERROR) << "Foreign tab no longer has valid navigations.";
    return false;
  }

  TabModel* tab_model = TabModelList::GetTabModelWithProfile(profile_);
  DCHECK(tab_model);
  if (!tab_model)
    return false;

  std::vector<content::NavigationEntry*> entries =
      sessions::SerializedNavigationEntry::ToNavigationEntries(
          tab->navigations, profile_);
  content::WebContents* new_web_contents = content::WebContents::Create(
      content::WebContents::CreateParams(profile_));
  int selected_index = tab->normalized_navigation_index();
  new_web_contents->GetController().Restore(
      selected_index,
      content::NavigationController::RESTORE_LAST_SESSION_EXITED_CLEANLY,
      &entries);
  tab_model->CreateTab(new_web_contents);

  return true;
}

void ForeignSessionHelper::SetForeignSessionCollapsed(JNIEnv* env, jobject obj,
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

void ForeignSessionHelper::DeleteForeignSession(JNIEnv* env, jobject obj,
                                                jstring session_tag) {
  SessionModelAssociator* associator = GetSessionModelAssociator(profile_);
  if (associator)
    associator->DeleteForeignSession(ConvertJavaStringToUTF8(env, session_tag));
}

// static
bool ForeignSessionHelper::RegisterForeignSessionHelper(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
