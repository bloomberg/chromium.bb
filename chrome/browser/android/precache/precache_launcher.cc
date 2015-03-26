// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/precache/precache_launcher.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/precache/most_visited_urls_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/history/core/browser/top_sites.h"
#include "components/precache/content/precache_manager.h"
#include "components/precache/content/precache_manager_factory.h"
#include "jni/PrecacheLauncher_jni.h"

using base::android::AttachCurrentThread;
using precache::PrecacheManager;

namespace {

// Get the profile that should be used for precaching.
Profile* GetProfile() {
  Profile* profile = g_browser_process->profile_manager()->GetLastUsedProfile()
      ->GetOriginalProfile();
  DCHECK(profile);
  DCHECK(g_browser_process->profile_manager()->IsValidProfile(profile));
  return profile;
}

// Get the PrecacheManager for the given |profile|.
PrecacheManager* GetPrecacheManager(Profile* profile) {
  PrecacheManager* precache_manager =
      precache::PrecacheManagerFactory::GetForBrowserContext(profile);
  DCHECK(precache_manager);
  return precache_manager;
}

bool IsDataReductionProxyEnabled(Profile* profile) {
  // TODO(bengr): Use DataReductionProxySettings instead once it is safe to
  // instantiate from here.
  PrefService* prefs = profile->GetPrefs();
  return prefs && prefs->GetBoolean(
      data_reduction_proxy::prefs::kDataReductionProxyEnabled);
}

}  // namespace

PrecacheLauncher::PrecacheLauncher(JNIEnv* env, jobject obj)
    : weak_java_precache_launcher_(env, obj), weak_factory_(this) {}

PrecacheLauncher::~PrecacheLauncher() {}

void PrecacheLauncher::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void PrecacheLauncher::Start(JNIEnv* env, jobject obj) {
  // TODO(bengr): Add integration tests for the whole feature.
  Profile* profile = GetProfile();
  PrecacheManager* precache_manager = GetPrecacheManager(profile);
  scoped_refptr<history::TopSites> ts = TopSitesFactory::GetForProfile(profile);
  precache::MostVisitedURLsProvider url_list_provider(ts.get());

  if (!IsDataReductionProxyEnabled(profile)) {
    Java_PrecacheLauncher_onPrecacheCompletedCallback(
        env, weak_java_precache_launcher_.get(env).obj());
    return;
  }

  precache_manager->StartPrecaching(
      base::Bind(&PrecacheLauncher::OnPrecacheCompleted,
                 weak_factory_.GetWeakPtr()),
      &url_list_provider);
}

void PrecacheLauncher::Cancel(JNIEnv* env, jobject obj) {
  Profile* profile = GetProfile();
  PrecacheManager* precache_manager = GetPrecacheManager(profile);

  precache_manager->CancelPrecaching();
}

void PrecacheLauncher::OnPrecacheCompleted() {
  JNIEnv* env = AttachCurrentThread();
  Java_PrecacheLauncher_onPrecacheCompletedCallback(
      env, weak_java_precache_launcher_.get(env).obj());
}

static jlong Init(JNIEnv* env, jobject obj) {
  return reinterpret_cast<intptr_t>(new PrecacheLauncher(env, obj));
}

static jboolean IsPrecachingEnabled(JNIEnv* env, jclass clazz) {
  return PrecacheManager::IsPrecachingEnabled();
}

bool RegisterPrecacheLauncher(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
