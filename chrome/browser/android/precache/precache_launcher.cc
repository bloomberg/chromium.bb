// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/precache/precache_launcher.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/precache/precache_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/precache/content/precache_manager.h"
#include "components/prefs/pref_service.h"
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

}  // namespace

PrecacheLauncher::PrecacheLauncher(JNIEnv* env, jobject obj)
    : weak_java_precache_launcher_(env, obj), weak_factory_(this) {}

PrecacheLauncher::~PrecacheLauncher() {}

void PrecacheLauncher::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

void PrecacheLauncher::Start(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  // TODO(bengr): Add integration tests for the whole feature.
  Profile* profile = GetProfile();

  PrecacheManager* precache_manager = GetPrecacheManager(profile);

  if (precache_manager == nullptr) {
    OnPrecacheCompleted(false);
    return;
  }

  precache_manager->StartPrecaching(base::Bind(
      &PrecacheLauncher::OnPrecacheCompleted, weak_factory_.GetWeakPtr()));
}

void PrecacheLauncher::Cancel(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  Profile* profile = GetProfile();
  PrecacheManager* precache_manager = GetPrecacheManager(profile);

  precache_manager->CancelPrecaching();
}

void PrecacheLauncher::OnPrecacheCompleted(bool try_again_soon) {
  JNIEnv* env = AttachCurrentThread();
  Java_PrecacheLauncher_onPrecacheCompletedCallback(
      env, weak_java_precache_launcher_.get(env).obj(),
      try_again_soon ? JNI_TRUE : JNI_FALSE);
}

static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new PrecacheLauncher(env, obj));
}

// Must be run on the UI thread.
static jboolean ShouldRun(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  Profile* profile = GetProfile();
  PrecacheManager* precache_manager = GetPrecacheManager(profile);
  return precache_manager && (precache_manager->IsInExperimentGroup() ||
                              precache_manager->IsInControlGroup());
}

bool RegisterPrecacheLauncher(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
