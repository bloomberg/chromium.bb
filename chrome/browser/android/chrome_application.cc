// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chrome_application.h"

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/protocol_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/chrome_content_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "jni/ChromeApplication_jni.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using base::android::ConvertUTF8ToJavaString;

namespace {

void FlushCookiesOnIOThread(
    scoped_refptr<net::URLRequestContextGetter> getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  getter->GetURLRequestContext()
      ->cookie_store()
      ->GetCookieMonster()
      ->FlushStore(base::Closure());
}

void FlushStoragePartition(content::StoragePartition* partition) {
  partition->Flush();
}

void CommitPendingWritesForProfile(Profile* profile) {
  // These calls are asynchronous. They may not finish (and may not even
  // start!) before the Android OS kills our process. But we can't wait for them
  // to finish because blocking the UI thread is illegal.
  profile->GetPrefs()->CommitPendingWrite();
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&FlushCookiesOnIOThread,
                 make_scoped_refptr(profile->GetRequestContext())));
  profile->GetNetworkPredictor()->SaveStateForNextStartupAndTrim();
  content::BrowserContext::ForEachStoragePartition(
      profile, base::Bind(FlushStoragePartition));
}

void RemoveSessionCookiesOnIOThread(
    scoped_refptr<net::URLRequestContextGetter> getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  getter->GetURLRequestContext()->cookie_store()->DeleteSessionCookiesAsync(
      net::CookieStore::DeleteCallback());
}

void RemoveSessionCookiesForProfile(Profile* profile) {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&RemoveSessionCookiesOnIOThread,
                 make_scoped_refptr(profile->GetRequestContext())));
}

void ChangeAppStatusOnIOThread(safe_browsing::SafeBrowsingService* sb_service,
                               jboolean foreground) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  safe_browsing::SafeBrowsingProtocolManager* proto_manager =
      sb_service->protocol_manager();
  if (proto_manager)
    proto_manager->SetAppInForeground(foreground);
}

}  // namespace

static ScopedJavaLocalRef<jstring> GetBrowserUserAgent(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz) {
  return ConvertUTF8ToJavaString(env, GetUserAgent());
}

static void FlushPersistentData(JNIEnv* env, const JavaParamRef<jclass>& obj) {
  // Commit the prending writes for all the loaded profiles.
  std::vector<Profile*> loaded_profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  std::for_each(loaded_profiles.begin(), loaded_profiles.end(),
                CommitPendingWritesForProfile);

  if (g_browser_process->local_state())
    g_browser_process->local_state()->CommitPendingWrite();
}

static void RemoveSessionCookies(JNIEnv* env, const JavaParamRef<jclass>& obj) {
  std::vector<Profile*> loaded_profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  std::for_each(loaded_profiles.begin(), loaded_profiles.end(),
                RemoveSessionCookiesForProfile);
}

static void ChangeAppStatus(JNIEnv* env,
                            const JavaParamRef<jclass>& obj,
                            jboolean foreground) {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&ChangeAppStatusOnIOThread,
                 base::Unretained(g_browser_process->safe_browsing_service()),
                 foreground));
}

namespace chrome {
namespace android {

// static
bool ChromeApplication::RegisterBindings(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void ChromeApplication::ShowAutofillSettings() {
  Java_ChromeApplication_showAutofillSettings(
      base::android::AttachCurrentThread(),
      base::android::GetApplicationContext());
}

void ChromeApplication::ShowPasswordSettings() {
  Java_ChromeApplication_showPasswordSettings(
      base::android::AttachCurrentThread(),
      base::android::GetApplicationContext());
}

void ChromeApplication::OpenClearBrowsingData(
    content::WebContents* web_contents) {
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  DCHECK(tab);
  Java_ChromeApplication_openClearBrowsingData(
      base::android::AttachCurrentThread(),
      base::android::GetApplicationContext(),
      tab->GetJavaObject().obj());
}

bool ChromeApplication::AreParentalControlsEnabled() {
  return Java_ChromeApplication_areParentalControlsEnabled(
      base::android::AttachCurrentThread(),
      base::android::GetApplicationContext());
}

}  // namespace android
}  // namespace chrome
