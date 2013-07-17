// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/signin/signin_manager_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "jni/SigninManager_jni.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud/cloud_policy_client.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_android.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "google_apis/gaia/gaia_auth_util.h"
#endif

SigninManagerAndroid::SigninManagerAndroid(JNIEnv* env, jobject obj)
    : profile_(NULL) {
  java_signin_manager_.Reset(env, obj);
  DCHECK(g_browser_process);
  DCHECK(g_browser_process->profile_manager());
  profile_ = g_browser_process->profile_manager()->GetDefaultProfile();
  DCHECK(profile_);
}

SigninManagerAndroid::~SigninManagerAndroid() {}

void SigninManagerAndroid::CheckPolicyBeforeSignIn(JNIEnv* env,
                                                   jobject obj,
                                                   jstring username) {
#if defined(ENABLE_CONFIGURATION_POLICY)
  username_ = base::android::ConvertJavaStringToUTF8(env, username);
  policy::UserPolicySigninService* service =
      policy::UserPolicySigninServiceFactory::GetForProfile(profile_);
  service->RegisterPolicyClient(
      base::android::ConvertJavaStringToUTF8(env, username),
      base::Bind(&SigninManagerAndroid::OnPolicyRegisterDone,
                 base::Unretained(this)));
#else
  // This shouldn't be called when ShouldLoadPolicyForUser() is false.
  NOTREACHED();
  ScopedJavaLocalRef<jstring> domain;
  Java_SigninManager_onPolicyCheckedBeforeSignIn(env,
                                                 java_signin_manager_.obj(),
                                                 domain.obj());
#endif
}

void SigninManagerAndroid::FetchPolicyBeforeSignIn(JNIEnv* env, jobject obj) {
#if defined(ENABLE_CONFIGURATION_POLICY)
  if (cloud_policy_client_) {
    policy::UserPolicySigninService* service =
        policy::UserPolicySigninServiceFactory::GetForProfile(profile_);
    service->FetchPolicyForSignedInUser(
        cloud_policy_client_.Pass(),
        base::Bind(&SigninManagerAndroid::OnPolicyFetchDone,
                   base::Unretained(this)));
    return;
  }
#endif
  // This shouldn't be called when ShouldLoadPolicyForUser() is false, or when
  // CheckPolicyBeforeSignIn() failed.
  NOTREACHED();
  Java_SigninManager_onPolicyFetchedBeforeSignIn(env,
                                                 java_signin_manager_.obj());
}

void SigninManagerAndroid::OnSignInCompleted(JNIEnv* env,
                                             jobject obj,
                                             jstring username) {
  SigninManagerFactory::GetForProfile(profile_)->OnExternalSigninCompleted(
      base::android::ConvertJavaStringToUTF8(env, username));
}

void SigninManagerAndroid::SignOut(JNIEnv* env, jobject obj) {
  SigninManagerFactory::GetForProfile(profile_)->SignOut();
}

#if defined(ENABLE_CONFIGURATION_POLICY)

void SigninManagerAndroid::OnPolicyRegisterDone(
    scoped_ptr<policy::CloudPolicyClient> client) {
  cloud_policy_client_ = client.Pass();

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> domain;
  if (cloud_policy_client_) {
    DCHECK(!username_.empty());
    domain.Reset(
        base::android::ConvertUTF8ToJavaString(
            env, gaia::ExtractDomainName(username_)));
  } else {
    username_.clear();
  }

  Java_SigninManager_onPolicyCheckedBeforeSignIn(env,
                                                 java_signin_manager_.obj(),
                                                 domain.obj());
}

void SigninManagerAndroid::OnPolicyFetchDone(bool success) {
  Java_SigninManager_onPolicyFetchedBeforeSignIn(
      base::android::AttachCurrentThread(),
      java_signin_manager_.obj());
}

#endif

static int Init(JNIEnv* env, jobject obj) {
  SigninManagerAndroid* signin_manager_android =
      new SigninManagerAndroid(env, obj);
  return reinterpret_cast<jint>(signin_manager_android);
}

static jboolean ShouldLoadPolicyForUser(JNIEnv* env,
                                        jobject obj,
                                        jstring j_username) {
#if defined(ENABLE_CONFIGURATION_POLICY)
  std::string username =
      base::android::ConvertJavaStringToUTF8(env, j_username);
  return !policy::BrowserPolicyConnector::IsNonEnterpriseUser(username);
#else
  return false;
#endif
}

// static
bool SigninManagerAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
