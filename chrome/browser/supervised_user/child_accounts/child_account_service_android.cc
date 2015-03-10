// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/child_account_service_android.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "jni/ChildAccountService_jni.h"

jboolean IsChildAccountDetectionEnabled(JNIEnv* env, jobject obj) {
  return ChildAccountService::IsChildAccountDetectionEnabled();
}

void OnChildAccountSigninComplete(JNIEnv* env, jobject obj) {
  VLOG(1) << "OnChildAccountSigninComplete";

  Profile* profile = ProfileManager::GetLastUsedProfile();
  ChildAccountServiceFactory::GetForProfile(profile)->SetIsChildAccount(true);
}

bool RegisterChildAccountService(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
