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
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/android_profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "jni/SigninManager_jni.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_mobile.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/url_request/url_request_context_getter.h"
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
  profile_ = ProfileManager::GetActiveUserProfile();
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
  service->RegisterForPolicy(
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
  if (!dm_token_.empty()) {
    policy::UserPolicySigninService* service =
        policy::UserPolicySigninServiceFactory::GetForProfile(profile_);
    service->FetchPolicyForSignedInUser(
        username_,
        dm_token_,
        client_id_,
        profile_->GetRequestContext(),
        base::Bind(&SigninManagerAndroid::OnPolicyFetchDone,
                   weak_factory_.GetWeakPtr()));
    dm_token_.clear();
    client_id_.clear();
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
  SigninManagerFactory::GetForProfile(profile_)->SignOut(
      signin_metrics::USER_CLICKED_SIGNOUT_SETTINGS);
}

base::android::ScopedJavaLocalRef<jstring>
SigninManagerAndroid::GetManagementDomain(JNIEnv* env, jobject obj) {
  base::android::ScopedJavaLocalRef<jstring> domain;

#if defined(ENABLE_CONFIGURATION_POLICY)
  policy::UserCloudPolicyManager* manager =
      policy::UserCloudPolicyManagerFactory::GetForBrowserContext(profile_);
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
    const std::string& dm_token,
    const std::string& client_id) {
  dm_token_ = dm_token;
  client_id_ = client_id;

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> domain;
  if (!dm_token_.empty()) {
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
  model->RemoveAllUserBookmarks();

  // All the Profile data has been wiped. Clear the last signed in username as
  // well, so that the next signin doesn't trigger the acount change dialog.
  ClearLastSignedInUser();

  Java_SigninManager_onProfileDataWiped(base::android::AttachCurrentThread(),
                                        java_signin_manager_.obj());
}

void SigninManagerAndroid::ClearLastSignedInUser(JNIEnv* env, jobject obj) {
  ClearLastSignedInUser();
}

void SigninManagerAndroid::ClearLastSignedInUser() {
  profile_->GetPrefs()->ClearPref(prefs::kGoogleServicesLastUsername);
}

void SigninManagerAndroid::MergeSessionCompleted(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  merge_session_helper_->RemoveObserver(this);
  merge_session_helper_.reset();
}

void SigninManagerAndroid::LogInSignedInUser(JNIEnv* env, jobject obj) {
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile_);
  if (switches::IsNewProfileManagement()) {
    // New Mirror code path that just fires the events and let the
    // Account Reconcilor handles everything.
    AndroidProfileOAuth2TokenService* token_service =
        ProfileOAuth2TokenServiceFactory::GetPlatformSpecificForProfile(
            profile_);
    const std::string& primary_acct =
        signin_manager->GetAuthenticatedAccountId();
    token_service->ValidateAccounts(primary_acct, true);

  } else {
    DVLOG(1) << "SigninManagerAndroid::LogInSignedInUser "
        " Manually calling MergeSessionHelper";
    // Old code path that doesn't depend on the new Account Reconcilor.
    // We manually login.

    ProfileOAuth2TokenService* token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
    merge_session_helper_.reset(new MergeSessionHelper(
        token_service, profile_->GetRequestContext(), this));
    merge_session_helper_->LogIn(signin_manager->GetAuthenticatedAccountId());
  }
}

static jlong Init(JNIEnv* env, jobject obj) {
  SigninManagerAndroid* signin_manager_android =
      new SigninManagerAndroid(env, obj);
  return reinterpret_cast<intptr_t>(signin_manager_android);
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

static jboolean IsNewProfileManagementEnabled(JNIEnv* env, jclass clazz) {
  return switches::IsNewProfileManagement();
}

// static
bool SigninManagerAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
