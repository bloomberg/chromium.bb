// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/android/signin/signin_manager_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/pref_service.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/browsing_data/browsing_data_remover_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/oauth2_token_service_delegate_android.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "google_apis/gaia/gaia_constants.h"
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

using bookmarks::BookmarkModel;

namespace {

// A BrowsingDataRemover::Observer that clears all Profile data and then
// invokes a callback and deletes itself.
class ProfileDataRemover : public BrowsingDataRemover::Observer {
 public:
  ProfileDataRemover(Profile* profile, const base::Closure& callback)
      : callback_(callback),
        origin_runner_(base::ThreadTaskRunnerHandle::Get()),
        remover_(BrowsingDataRemoverFactory::GetForBrowserContext(profile)) {
    remover_->AddObserver(this);
    remover_->Remove(BrowsingDataRemover::Unbounded(),
                     BrowsingDataRemover::REMOVE_ALL, BrowsingDataHelper::ALL);
  }

  ~ProfileDataRemover() override {}

  void OnBrowsingDataRemoverDone() override {
    remover_->RemoveObserver(this);
    origin_runner_->PostTask(FROM_HERE, callback_);
    origin_runner_->DeleteSoon(FROM_HERE, this);
  }

 private:
  base::Closure callback_;
  scoped_refptr<base::SingleThreadTaskRunner> origin_runner_;
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
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kSigninAllowed,
      base::Bind(&SigninManagerAndroid::OnSigninAllowedPrefChanged,
                 base::Unretained(this)));
}

SigninManagerAndroid::~SigninManagerAndroid() {}

void SigninManagerAndroid::CheckPolicyBeforeSignIn(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& username) {
#if defined(ENABLE_CONFIGURATION_POLICY)
  username_ = base::android::ConvertJavaStringToUTF8(env, username);
  policy::UserPolicySigninService* service =
      policy::UserPolicySigninServiceFactory::GetForProfile(profile_);
  service->RegisterForPolicy(
      username_, AccountTrackerServiceFactory::GetForProfile(profile_)
                     ->FindAccountInfoByEmail(username_)
                     .account_id,
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

void SigninManagerAndroid::FetchPolicyBeforeSignIn(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
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

void SigninManagerAndroid::OnSignInCompleted(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& username) {
  DVLOG(1) << "SigninManagerAndroid::OnSignInCompleted";
  SigninManagerFactory::GetForProfile(profile_)->OnExternalSigninCompleted(
      base::android::ConvertJavaStringToUTF8(env, username));
}

void SigninManagerAndroid::SignOut(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
  SigninManagerFactory::GetForProfile(profile_)
      ->SignOut(signin_metrics::USER_CLICKED_SIGNOUT_SETTINGS,
                signin_metrics::SignoutDelete::IGNORE_METRIC);
}

base::android::ScopedJavaLocalRef<jstring>
SigninManagerAndroid::GetManagementDomain(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
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

void SigninManagerAndroid::WipeProfileData(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& callback) {
  base::android::ScopedJavaGlobalRef<jobject> java_callback;
  java_callback.Reset(env, callback);

  // The ProfileDataRemover deletes itself once done.
  new ProfileDataRemover(
      profile_, base::Bind(&SigninManagerAndroid::OnBrowsingDataRemoverDone,
                           weak_factory_.GetWeakPtr(), java_callback));
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

void SigninManagerAndroid::OnBrowsingDataRemoverDone(
    const base::android::ScopedJavaGlobalRef<jobject>& callback) {
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile_);
  model->RemoveAllUserBookmarks();

  // All the Profile data has been wiped. Clear the last signed in username as
  // well, so that the next signin doesn't trigger the acount change dialog.
  ClearLastSignedInUser();

  Java_SigninManager_onProfileDataWiped(base::android::AttachCurrentThread(),
                                        java_signin_manager_.obj(),
                                        callback.obj());
}

void SigninManagerAndroid::ClearLastSignedInUser(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  ClearLastSignedInUser();
}

void SigninManagerAndroid::ClearLastSignedInUser() {
  profile_->GetPrefs()->ClearPref(prefs::kGoogleServicesLastAccountId);
  profile_->GetPrefs()->ClearPref(prefs::kGoogleServicesLastUsername);
}

void SigninManagerAndroid::LogInSignedInUser(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile_);
  // With the account consistency enabled let the account Reconcilor handles
  // everything.
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  const std::string& primary_acct = signin_manager->GetAuthenticatedAccountId();

  static_cast<OAuth2TokenServiceDelegateAndroid*>(token_service->GetDelegate())
      ->ValidateAccounts(primary_acct, true);
}

jboolean SigninManagerAndroid::IsSigninAllowedByPolicy(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return SigninManagerFactory::GetForProfile(profile_)->IsSigninAllowed();
}

jboolean SigninManagerAndroid::IsSignedInOnNative(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return SigninManagerFactory::GetForProfile(profile_)->IsAuthenticated();
}

void SigninManagerAndroid::OnSigninAllowedPrefChanged() {
  Java_SigninManager_onSigninAllowedByPolicyChanged(
      base::android::AttachCurrentThread(), java_signin_manager_.obj(),
      SigninManagerFactory::GetForProfile(profile_)->IsSigninAllowed());
}

static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  SigninManagerAndroid* signin_manager_android =
      new SigninManagerAndroid(env, obj);
  return reinterpret_cast<intptr_t>(signin_manager_android);
}

static jboolean ShouldLoadPolicyForUser(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_username) {
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
