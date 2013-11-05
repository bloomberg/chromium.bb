// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/signin/signin_manager_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/google_auto_login_helper.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "jni/SigninManager_jni.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud/cloud_policy_client.h"
#include "chrome/browser/policy/cloud/cloud_policy_core.h"
#include "chrome/browser/policy/cloud/cloud_policy_store.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_manager.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_android.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "google_apis/gaia/gaia_auth_util.h"
#endif

namespace {

// A BrowsingDataRemover::Observer that clears all Profile data and then
// invokes a callback and deletes itself.
class ProfileDataRemover : public BrowsingDataRemover::Observer {
 public:
  ProfileDataRemover(Profile* profile, const base::Closure& callback)
      : callback_(callback),
        origin_loop_(base::MessageLoopProxy::current()),
        remover_(BrowsingDataRemover::CreateForUnboundedRange(profile)) {
    remover_->AddObserver(this);
    remover_->Remove(BrowsingDataRemover::REMOVE_ALL, BrowsingDataHelper::ALL);
  }

  virtual ~ProfileDataRemover() {}

  virtual void OnBrowsingDataRemoverDone() OVERRIDE {
    remover_->RemoveObserver(this);
    origin_loop_->PostTask(FROM_HERE, callback_);
    origin_loop_->DeleteSoon(FROM_HERE, this);
  }

 private:
  base::Closure callback_;
  scoped_refptr<base::MessageLoopProxy> origin_loop_;
  BrowsingDataRemover* remover_;

  DISALLOW_COPY_AND_ASSIGN(ProfileDataRemover);
};

}  // namespace

SigninManagerAndroid::SigninManagerAndroid(JNIEnv* env, jobject obj)
    : profile_(NULL),
      weak_factory_(this) {
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
                 weak_factory_.GetWeakPtr()));
#else
  // This shouldn't be called when ShouldLoadPolicyForUser() is false.
  NOTREACHED();
  base::android::ScopedJavaLocalRef<jstring> domain;
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
        username_,
        cloud_policy_client_.Pass(),
        base::Bind(&SigninManagerAndroid::OnPolicyFetchDone,
                   weak_factory_.GetWeakPtr()));
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

base::android::ScopedJavaLocalRef<jstring>
SigninManagerAndroid::GetManagementDomain(JNIEnv* env, jobject obj) {
  base::android::ScopedJavaLocalRef<jstring> domain;

#if defined(ENABLE_CONFIGURATION_POLICY)
  policy::UserCloudPolicyManager* manager =
      policy::UserCloudPolicyManagerFactory::GetForProfile(profile_);
  policy::CloudPolicyStore* store = manager->core()->store();

  if (store && store->is_managed() && store->policy()->has_username()) {
    domain.Reset(
        base::android::ConvertUTF8ToJavaString(
            env, gaia::ExtractDomainName(store->policy()->username())));
  }
#endif

  return domain;
}

void SigninManagerAndroid::WipeProfileData(JNIEnv* env, jobject obj) {
  // The ProfileDataRemover deletes itself once done.
  new ProfileDataRemover(
      profile_,
      base::Bind(&SigninManagerAndroid::OnBrowsingDataRemoverDone,
                 weak_factory_.GetWeakPtr()));
}

#if defined(ENABLE_CONFIGURATION_POLICY)

void SigninManagerAndroid::OnPolicyRegisterDone(
    scoped_ptr<policy::CloudPolicyClient> client) {
  cloud_policy_client_ = client.Pass();

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> domain;
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

void SigninManagerAndroid::OnBrowsingDataRemoverDone() {
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile_);
  model->RemoveAll();

  // All the Profile data has been wiped. Clear the last signed in username as
  // well, so that the next signin doesn't trigger the acount change dialog.
  profile_->GetPrefs()->ClearPref(prefs::kGoogleServicesLastUsername);

  Java_SigninManager_onProfileDataWiped(base::android::AttachCurrentThread(),
                                        java_signin_manager_.obj());
}

void SigninManagerAndroid::LogInSignedInUser(JNIEnv* env, jobject obj) {
  // AutoLogin deletes itself.
  GoogleAutoLoginHelper* autoLogin = new GoogleAutoLoginHelper(profile_);
  autoLogin->LogIn();
}

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
