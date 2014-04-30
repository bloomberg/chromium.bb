// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_MANAGER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_MANAGER_ANDROID_H_

#include <jni.h>

#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "google_apis/gaia/merge_session_helper.h"

class GoogleServiceAuthError;
class Profile;

namespace policy {
class CloudPolicyClient;
}

// Android wrapper of the SigninManager which provides access from the Java
// layer. Note that on Android, there's only a single profile, and therefore
// a single instance of this wrapper. The name of the Java class is
// SigninManager.
// This class should only be accessed from the UI thread.
//
// This class implements parts of the sign-in flow, to make sure that policy
// is available before sign-in completes.
class SigninManagerAndroid : public MergeSessionHelper::Observer {
 public:
  SigninManagerAndroid(JNIEnv* env, jobject obj);

  // Registers the SigninManagerAndroid's native methods through JNI.
  static bool Register(JNIEnv* env);

  void CheckPolicyBeforeSignIn(JNIEnv* env, jobject obj, jstring username);

  void FetchPolicyBeforeSignIn(JNIEnv* env, jobject obj);

  void OnSignInCompleted(JNIEnv* env, jobject obj, jstring username);

  void SignOut(JNIEnv* env, jobject obj);

  base::android::ScopedJavaLocalRef<jstring> GetManagementDomain(JNIEnv* env,
                                                                 jobject obj);

  void WipeProfileData(JNIEnv* env, jobject obj);

  void LogInSignedInUser(JNIEnv* env, jobject obj);

  void ClearLastSignedInUser(JNIEnv* env, jobject obj);

 private:
  virtual ~SigninManagerAndroid();

#if defined(ENABLE_CONFIGURATION_POLICY)
  void OnPolicyRegisterDone(const std::string& dm_token,
                            const std::string& client_id);
  void OnPolicyFetchDone(bool success);
#endif

  void OnBrowsingDataRemoverDone();

  void ClearLastSignedInUser();

  // MergeSessionHelper::Observer implementation.
  virtual void MergeSessionCompleted(
      const std::string& account_id,
      const GoogleServiceAuthError& error) OVERRIDE;

  Profile* profile_;

  // Java-side SigninManager object.
  base::android::ScopedJavaGlobalRef<jobject> java_signin_manager_;

#if defined(ENABLE_CONFIGURATION_POLICY)
  // CloudPolicy credentials stored during a pending sign-in, awaiting user
  // confirmation before starting to fetch policies.
  std::string dm_token_;
  std::string client_id_;

  // Username that is pending sign-in. This is used to extract the domain name
  // for the policy dialog, when |username_| corresponds to a managed account.
  std::string username_;
#endif

  // Helper to merge the signed into account into the cookie jar session.
  scoped_ptr<MergeSessionHelper> merge_session_helper_;

  base::WeakPtrFactory<SigninManagerAndroid> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SigninManagerAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_MANAGER_ANDROID_H_
