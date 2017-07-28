// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/android/jni_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "jni/ProfileManagerUtils_jni.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

void FlushCookiesOnIOThread(
    scoped_refptr<net::URLRequestContextGetter> getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  getter->GetURLRequestContext()->cookie_store()->FlushStore(base::Closure());
}

void FlushStoragePartition(content::StoragePartition* partition) {
  partition->Flush();
}

void CommitPendingWritesForProfile(Profile* profile) {
  // These calls are asynchronous. They may not finish (and may not even
  // start!) before the Android OS kills our process. But we can't wait for them
  // to finish because blocking the UI thread is illegal.
  profile->GetNetworkPredictor()->SaveStateForNextStartup();
  profile->GetPrefs()->CommitPendingWrite();
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&FlushCookiesOnIOThread,
                 make_scoped_refptr(profile->GetRequestContext())));
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

}  // namespace

static void FlushPersistentDataForAllProfiles(JNIEnv* env,
                                              const JavaParamRef<jclass>& obj) {
  std::vector<Profile*> loaded_profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  std::for_each(loaded_profiles.begin(), loaded_profiles.end(),
                CommitPendingWritesForProfile);

  if (g_browser_process->local_state())
    g_browser_process->local_state()->CommitPendingWrite();
}

static void RemoveSessionCookiesForAllProfiles(
    JNIEnv* env,
    const JavaParamRef<jclass>& obj) {
  std::vector<Profile*> loaded_profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  std::for_each(loaded_profiles.begin(), loaded_profiles.end(),
                RemoveSessionCookiesForProfile);
}
