// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/child_account_service_android.h"

#include "base/android/callback_android.h"
#include "base/bind.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "jni/ChildAccountService_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::RunCallbackAndroid;
using base::android::ScopedJavaGlobalRef;

jboolean IsChildAccount(JNIEnv* env, const JavaParamRef<jclass>& jcaller) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetLastUsedProfile()->IsChild();
}

bool RegisterChildAccountService(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void ListenForChildStatusReceived(JNIEnv* env,
                                  const JavaParamRef<jclass>& jcaller,
                                  const JavaParamRef<jobject>& callback) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ChildAccountService* service = ChildAccountServiceFactory::GetForProfile(
      profile_manager->GetLastUsedProfile());
  // Needed to disambiguate RunCallbackAndroid
  void (*runCallback)(const JavaRef<jobject>&, bool) = &RunCallbackAndroid;
  // TODO(https://crbug.com/692591) Should use base::BindOnce, but
  // AddChildStatusReceivedCallback won't yet accept a BindOnce.
  service->AddChildStatusReceivedCallback(
      base::Bind(runCallback, ScopedJavaGlobalRef<jobject>(callback), true));
}
