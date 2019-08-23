// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/signin/signin_manager_android.h"

#include <utility>
#include <vector>

#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "chrome/android/chrome_jni_headers/SigninManager_jni.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/internal/identity_manager/primary_account_manager.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "google_apis/gaia/gaia_auth_util.h"

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_mobile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_id_from_account_info.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/google/core/common/google_util.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_util.h"

using base::android::JavaParamRef;

namespace {

// Clears the information about the last signed-in user from |profile|.
void ClearLastSignedInUserForProfile(Profile* profile) {
  profile->GetPrefs()->ClearPref(prefs::kGoogleServicesLastAccountId);
  profile->GetPrefs()->ClearPref(prefs::kGoogleServicesLastUsername);
}

// A BrowsingDataRemover::Observer that clears Profile data and then invokes
// a callback and deletes itself. It can be configured to delete all data
// (for enterprise users) or only Google's service workers (for all users).
class ProfileDataRemover : public content::BrowsingDataRemover::Observer {
 public:
  ProfileDataRemover(Profile* profile,
                     bool all_data,
                     base::OnceClosure callback)
      : profile_(profile),
        all_data_(all_data),
        callback_(std::move(callback)),
        origin_runner_(base::ThreadTaskRunnerHandle::Get()),
        remover_(content::BrowserContext::GetBrowsingDataRemover(profile)) {
    remover_->AddObserver(this);

    if (all_data) {
      remover_->RemoveAndReply(
          base::Time(), base::Time::Max(),
          ChromeBrowsingDataRemoverDelegate::ALL_DATA_TYPES,
          ChromeBrowsingDataRemoverDelegate::ALL_ORIGIN_TYPES, this);
    } else {
      std::unique_ptr<content::BrowsingDataFilterBuilder> google_tld_filter =
          content::BrowsingDataFilterBuilder::Create(
              content::BrowsingDataFilterBuilder::WHITELIST);

      // TODO(msramek): BrowsingDataFilterBuilder was not designed for
      // large filters. Optimize it.
      for (const std::string& domain :
           google_util::GetGoogleRegistrableDomains()) {
        google_tld_filter->AddRegisterableDomain(domain);
      }

      remover_->RemoveWithFilterAndReply(
          base::Time(), base::Time::Max(),
          content::BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE,
          ChromeBrowsingDataRemoverDelegate::ALL_ORIGIN_TYPES,
          std::move(google_tld_filter), this);
    }
  }

  ~ProfileDataRemover() override {}

  void OnBrowsingDataRemoverDone() override {
    remover_->RemoveObserver(this);

    if (all_data_) {
      // All the Profile data has been wiped. Clear the last signed in username
      // as well, so that the next signin doesn't trigger the account
      // change dialog.
      ClearLastSignedInUserForProfile(profile_);
    }

    origin_runner_->PostTask(FROM_HERE, std::move(callback_));
    origin_runner_->DeleteSoon(FROM_HERE, this);
  }

 private:
  Profile* profile_;
  bool all_data_;
  base::OnceClosure callback_;
  scoped_refptr<base::SingleThreadTaskRunner> origin_runner_;
  content::BrowsingDataRemover* remover_;

  DISALLOW_COPY_AND_ASSIGN(ProfileDataRemover);
};

// Returns whether the user is a managed user or not.
bool ShouldLoadPolicyForUser(const std::string& username) {
  return !policy::BrowserPolicyConnector::IsNonEnterpriseUser(username);
}

}  // namespace

SigninManagerAndroid::SigninManagerAndroid(
    Profile* profile,
    signin::IdentityManager* identity_manager)
    : profile_(profile),
      identity_manager_(identity_manager),
      user_cloud_policy_manager_(profile_->GetUserCloudPolicyManager()),
      user_policy_signin_service_(
          policy::UserPolicySigninServiceFactory::GetForProfile(profile_)),
      weak_factory_(this) {
  DCHECK(profile_);
  DCHECK(identity_manager_);
  DCHECK(user_cloud_policy_manager_);
  DCHECK(user_policy_signin_service_);
  identity_manager_->AddObserver(this);

  signin_allowed_.Init(
      prefs::kSigninAllowed, profile_->GetPrefs(),
      base::Bind(&SigninManagerAndroid::OnSigninAllowedPrefChanged,
                 base::Unretained(this)));

  force_browser_signin_.Init(prefs::kForceBrowserSignin,
                             g_browser_process->local_state());

  java_signin_manager_ = Java_SigninManager_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      identity_manager_->LegacyGetAccountTrackerServiceJavaObject());
}

base::android::ScopedJavaLocalRef<jobject>
SigninManagerAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_signin_manager_);
}

SigninManagerAndroid::~SigninManagerAndroid() {}

void SigninManagerAndroid::Shutdown() {
  identity_manager_->RemoveObserver(this);
  Java_SigninManager_destroy(base::android::AttachCurrentThread(),
                             java_signin_manager_);
}

jboolean SigninManagerAndroid::SetPrimaryAccount(
    JNIEnv* env,
    const JavaParamRef<jstring>& username) {
  DVLOG(1) << "SigninManagerAndroid::SetPrimaryAccount";

  auto account =
      identity_manager_
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
              base::android::ConvertJavaStringToUTF8(env, username));
  DCHECK(account.has_value());
  auto* account_mutator = identity_manager_->GetPrimaryAccountMutator();
  return account_mutator->SetPrimaryAccount(account->account_id);
}

void SigninManagerAndroid::SignOut(JNIEnv* env,
                                   jint signoutReason) {
  auto* account_mutator = identity_manager_->GetPrimaryAccountMutator();

  // GetPrimaryAccountMutator() returns nullptr on ChromeOS only.
  DCHECK(account_mutator);
  account_mutator->ClearPrimaryAccount(
      signin::PrimaryAccountMutator::ClearAccountsAction::kDefault,
      static_cast<signin_metrics::ProfileSignout>(signoutReason),
      // Always use IGNORE_METRIC for the profile deletion argument. Chrome
      // Android has just a single-profile which is never deleted upon
      // sign-out.
      signin_metrics::SignoutDelete::IGNORE_METRIC);
}

void SigninManagerAndroid::ClearLastSignedInUser(JNIEnv* env) {
  ClearLastSignedInUserForProfile(profile_);
}

void SigninManagerAndroid::LogInSignedInUser(JNIEnv* env) {
  // With the account consistency enabled let the account Reconcilor handles
  // everything.
  // TODO(https://crbug.com/930094): Determine the right long-term flow here.
  identity_manager_->LegacyReloadAccountsFromSystem();
}

bool SigninManagerAndroid::IsSigninAllowed() const {
  return signin_allowed_.GetValue();
}

jboolean SigninManagerAndroid::IsSigninAllowedByPolicy(JNIEnv* env) const {
  return IsSigninAllowed();
}

jboolean SigninManagerAndroid::IsForceSigninEnabled(JNIEnv* env) {
  return force_browser_signin_.GetValue();
}

jboolean SigninManagerAndroid::IsSignedInOnNative(JNIEnv* env) {
  return identity_manager_->HasPrimaryAccount();
}

void SigninManagerAndroid::OnPrimaryAccountCleared(
    const CoreAccountInfo& previous_primary_account_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  Java_SigninManager_onNativeSignOut(base::android::AttachCurrentThread(),
                                     java_signin_manager_);
}

void SigninManagerAndroid::OnSigninAllowedPrefChanged() const {
  Java_SigninManager_onSigninAllowedByPolicyChanged(
      base::android::AttachCurrentThread(), java_signin_manager_,
      IsSigninAllowed());
}

void SigninManagerAndroid::StopApplyingCloudPolicy(JNIEnv* env) {
  user_policy_signin_service_->ShutdownUserCloudPolicyManager();
}

void SigninManagerAndroid::RegisterPolicyWithAccount(
    const CoreAccountInfo& account,
    RegisterPolicyWithAccountCallback callback) {
  if (!ShouldLoadPolicyForUser(account.email)) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  user_policy_signin_service_->RegisterForPolicyWithAccountId(
      account.email, account.account_id,
      base::AdaptCallbackForRepeating(base::BindOnce(
          [](RegisterPolicyWithAccountCallback callback,
             const std::string& dm_token, const std::string& client_id) {
            base::Optional<ManagementCredentials> credentials;
            if (!dm_token.empty()) {
              credentials.emplace(dm_token, client_id);
            }
            std::move(callback).Run(credentials);
          },
          std::move(callback))));
}

void SigninManagerAndroid::FetchAndApplyCloudPolicy(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_username,
    const base::android::JavaParamRef<jobject>& j_callback) {
  std::string username =
      base::android::ConvertJavaStringToUTF8(env, j_username);
  DCHECK(!username.empty());
  // TODO(bsazonov): Remove after migrating the sign-in flow to CoreAccountId.
  // ExtractDomainName Dchecks that username is a valid email, in practice
  // this checks that @ is present and is not the last character.
  gaia::ExtractDomainName(username);
  CoreAccountInfo account =
      identity_manager_
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
              username)
          .value();

  auto callback =
      base::BindOnce(base::android::RunRunnableAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback));

  RegisterPolicyWithAccount(
      account,
      base::BindOnce(&SigninManagerAndroid::OnPolicyRegisterDone,
                     weak_factory_.GetWeakPtr(), account, std::move(callback)));
}

void SigninManagerAndroid::OnPolicyRegisterDone(
    const CoreAccountInfo& account,
    base::OnceCallback<void()> policy_callback,
    const base::Optional<ManagementCredentials>& credentials) {
  if (credentials) {
    FetchPolicyBeforeSignIn(account, std::move(policy_callback),
                            credentials.value());
  } else {
    // User's account does not have a policy to fetch.
    std::move(policy_callback).Run();
  }
}

void SigninManagerAndroid::FetchPolicyBeforeSignIn(
    const CoreAccountInfo& account,
    base::OnceCallback<void()> policy_callback,
    const ManagementCredentials& credentials) {
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetURLLoaderFactoryForBrowserProcess();
  user_policy_signin_service_->FetchPolicyForSignedInUser(
      AccountIdFromAccountInfo(account), credentials.dm_token,
      credentials.client_id, url_loader_factory,
      base::AdaptCallbackForRepeating(
          base::BindOnce([](base::OnceCallback<void()> callback,
                            bool success) { std::move(callback).Run(); },
                         std::move(policy_callback))));
}

void SigninManagerAndroid::IsAccountManaged(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_username,
    const JavaParamRef<jobject>& j_callback) {
  base::android::ScopedJavaGlobalRef<jobject> callback(env, j_callback);
  std::string username =
      base::android::ConvertJavaStringToUTF8(env, j_username);

  base::Optional<CoreAccountInfo> account =
      identity_manager_
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
              username);

  RegisterPolicyWithAccount(
      account.value_or(CoreAccountInfo{}),
      base::BindOnce(
          [](base::android::ScopedJavaGlobalRef<jobject> callback,
             const base::Optional<ManagementCredentials>& credentials) {
            base::android::RunBooleanCallbackAndroid(callback,
                                                     credentials.has_value());
          },
          callback));
}

base::android::ScopedJavaLocalRef<jstring>
SigninManagerAndroid::GetManagementDomain(JNIEnv* env) {
  base::android::ScopedJavaLocalRef<jstring> domain;

  policy::CloudPolicyStore* store = user_cloud_policy_manager_->core()->store();

  if (store && store->is_managed() && store->policy()->has_username()) {
    domain.Reset(base::android::ConvertUTF8ToJavaString(
        env, gaia::ExtractDomainName(store->policy()->username())));
  }

  return domain;
}

void SigninManagerAndroid::WipeProfileData(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_callback) {
  WipeData(
      profile_, true /* all data */,
      base::BindOnce(base::android::RunRunnableAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback)));
}

void SigninManagerAndroid::WipeGoogleServiceWorkerCaches(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_callback) {
  WipeData(
      profile_, false /* only Google service worker caches */,
      base::BindOnce(base::android::RunRunnableAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback)));
}

// static
void SigninManagerAndroid::WipeData(Profile* profile,
                                    bool all_data,
                                    base::OnceClosure callback) {
  // The ProfileDataRemover deletes itself once done.
  new ProfileDataRemover(profile, all_data, std::move(callback));
}

base::android::ScopedJavaLocalRef<jstring> JNI_SigninManager_ExtractDomainName(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_email) {
  std::string email = base::android::ConvertJavaStringToUTF8(env, j_email);
  std::string domain = gaia::ExtractDomainName(email);
  return base::android::ConvertUTF8ToJavaString(env, domain);
}

jboolean JNI_SigninManager_IsMobileIdentityConsistencyEnabled(JNIEnv* env) {
  return base::FeatureList::IsEnabled(signin::kMiceFeature);
}
