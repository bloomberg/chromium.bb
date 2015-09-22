// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/child_account_service_android.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "jni/ChildAccountService_jni.h"

jboolean IsChildAccountDetectionEnabled(JNIEnv* env,
                                        const JavaParamRef<jclass>& jcaller) {
  return ChildAccountService::IsChildAccountDetectionEnabled();
}

jboolean IsChildAccount(JNIEnv* env, const JavaParamRef<jclass>& jcaller) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetLastUsedProfile()->IsChild();
}

bool RegisterChildAccountService(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
