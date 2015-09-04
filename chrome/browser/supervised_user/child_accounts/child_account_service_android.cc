// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/child_account_service_android.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "jni/ChildAccountService_jni.h"

namespace {

bool g_is_child_account = false;
bool g_has_child_account_status = false;

}  // namespace

jboolean IsChildAccountDetectionEnabled(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  return ChildAccountService::IsChildAccountDetectionEnabled();
}

jboolean GetIsChildAccount(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetLastUsedProfile()->IsChild();
}

void SetIsChildAccount(JNIEnv* env,
                       const JavaParamRef<jobject>& obj,
                       jboolean is_child) {
  VLOG(1) << "OnChildAccountSigninComplete";

  // If the browser process has not been created yet, store the child account
  // status and return it later in GetJavaChildAccountStatus().
  if (!g_browser_process) {
    g_has_child_account_status = true;
    g_is_child_account = is_child;
    return;
  }

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetLastUsedProfile();
  ChildAccountServiceFactory::GetForProfile(profile)
      ->SetIsChildAccount(is_child);
}

bool GetJavaChildAccountStatus(bool* is_child_account) {
  if (!g_has_child_account_status)
    return false;

  *is_child_account = g_is_child_account;
  g_has_child_account_status = false;
  return true;
}

void ChildStatusInvalidationReceived() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ChildAccountService_onInvalidationReceived(env);
}

bool RegisterChildAccountService(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
