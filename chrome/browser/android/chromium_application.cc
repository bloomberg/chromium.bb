// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chromium_application.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_content_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "jni/ChromiumApplication_jni.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using base::android::ConvertUTF8ToJavaString;

namespace {
void FlushCookiesOnIOThread(
    scoped_refptr<net::URLRequestContextGetter> getter) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  getter->GetURLRequestContext()
      ->cookie_store()
      ->GetCookieMonster()
      ->FlushStore(base::Closure());
}

void CommitPendingWritesForProfile(Profile* profile) {
  // Both of these calls are asynchronous. They may not finish (and may not
  // even start!) before the Android OS kills our process. But we can't wait
  // for them to finish because blocking the UI thread is illegal.
  profile->GetPrefs()->CommitPendingWrite();
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&FlushCookiesOnIOThread,
                 make_scoped_refptr(profile->GetRequestContext())));
}
}  // namespace

static jstring GetBrowserUserAgent(JNIEnv* env, jclass clazz) {
  return ConvertUTF8ToJavaString(env, GetUserAgent()).Release();
}

static void FlushPersistentData(JNIEnv* env, jclass obj) {
  // Commit the prending writes for all the loaded profiles.
  std::vector<Profile*> loaded_profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  std::for_each(loaded_profiles.begin(), loaded_profiles.end(),
                CommitPendingWritesForProfile);

  if (g_browser_process->local_state())
    g_browser_process->local_state()->CommitPendingWrite();
}

namespace chrome {
namespace android {

// static
bool ChromiumApplication::RegisterBindings(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void ChromiumApplication::OpenProtectedContentSettings() {
  Java_ChromiumApplication_openProtectedContentSettings(
      base::android::AttachCurrentThread(),
      base::android::GetApplicationContext());
}

void ChromiumApplication::ShowAutofillSettings() {
  Java_ChromiumApplication_showAutofillSettings(
      base::android::AttachCurrentThread(),
      base::android::GetApplicationContext());
}

void ChromiumApplication::ShowPasswordSettings() {
  Java_ChromiumApplication_showPasswordSettings(
      base::android::AttachCurrentThread(),
      base::android::GetApplicationContext());
}

void ChromiumApplication::OpenClearBrowsingData(
    content::WebContents* web_contents) {
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  DCHECK(tab);
  Java_ChromiumApplication_openClearBrowsingData(
      base::android::AttachCurrentThread(),
      base::android::GetApplicationContext(),
      tab->GetJavaObject().obj());
}

bool ChromiumApplication::AreParentalControlsEnabled() {
  return Java_ChromiumApplication_areParentalControlsEnabled(
      base::android::AttachCurrentThread(),
      base::android::GetApplicationContext());
}

}  // namespace android
}  // namespace chrome
