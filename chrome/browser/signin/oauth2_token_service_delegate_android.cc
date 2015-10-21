// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/oauth2_token_service_delegate_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_android.h"
#include "components/signin/core/browser/account_info.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "jni/OAuth2TokenService_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;

namespace {

// Callback from FetchOAuth2TokenWithUsername().
// Arguments:
// - the error, or NONE if the token fetch was successful.
// - the OAuth2 access token.
// - the expiry time of the token (may be null, indicating that the expiry
//   time is unknown.
typedef base::Callback<void(const GoogleServiceAuthError&,
                            const std::string&,
                            const base::Time&)> FetchOAuth2TokenCallback;

class AndroidAccessTokenFetcher : public OAuth2AccessTokenFetcher {
 public:
  AndroidAccessTokenFetcher(OAuth2AccessTokenConsumer* consumer,
                            const std::string& account_id);
  ~AndroidAccessTokenFetcher() override;

  // Overrides from OAuth2AccessTokenFetcher:
  void Start(const std::string& client_id,
             const std::string& client_secret,
             const std::vector<std::string>& scopes) override;
  void CancelRequest() override;

  // Handles an access token response.
  void OnAccessTokenResponse(const GoogleServiceAuthError& error,
                             const std::string& access_token,
                             const base::Time& expiration_time);

 private:
  std::string CombineScopes(const std::vector<std::string>& scopes);

  std::string account_id_;
  bool request_was_cancelled_;
  base::WeakPtrFactory<AndroidAccessTokenFetcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AndroidAccessTokenFetcher);
};

AndroidAccessTokenFetcher::AndroidAccessTokenFetcher(
    OAuth2AccessTokenConsumer* consumer,
    const std::string& account_id)
    : OAuth2AccessTokenFetcher(consumer),
      account_id_(account_id),
      request_was_cancelled_(false),
      weak_factory_(this) {
}

AndroidAccessTokenFetcher::~AndroidAccessTokenFetcher() {
}

void AndroidAccessTokenFetcher::Start(const std::string& client_id,
                                      const std::string& client_secret,
                                      const std::vector<std::string>& scopes) {
  JNIEnv* env = AttachCurrentThread();
  std::string scope = CombineScopes(scopes);
  ScopedJavaLocalRef<jstring> j_username =
      ConvertUTF8ToJavaString(env, account_id_);
  ScopedJavaLocalRef<jstring> j_scope = ConvertUTF8ToJavaString(env, scope);
  scoped_ptr<FetchOAuth2TokenCallback> heap_callback(
      new FetchOAuth2TokenCallback(
          base::Bind(&AndroidAccessTokenFetcher::OnAccessTokenResponse,
                     weak_factory_.GetWeakPtr())));

  // Call into Java to get a new token.
  Java_OAuth2TokenService_getOAuth2AuthToken(
      env, base::android::GetApplicationContext(), j_username.obj(),
      j_scope.obj(), reinterpret_cast<intptr_t>(heap_callback.release()));
}

void AndroidAccessTokenFetcher::CancelRequest() {
  request_was_cancelled_ = true;
}

void AndroidAccessTokenFetcher::OnAccessTokenResponse(
    const GoogleServiceAuthError& error,
    const std::string& access_token,
    const base::Time& expiration_time) {
  if (request_was_cancelled_) {
    // Ignore the callback if the request was cancelled.
    return;
  }
  if (error.state() == GoogleServiceAuthError::NONE) {
    FireOnGetTokenSuccess(access_token, expiration_time);
  } else {
    FireOnGetTokenFailure(error);
  }
}

// static
std::string AndroidAccessTokenFetcher::CombineScopes(
    const std::vector<std::string>& scopes) {
  // The Android AccountManager supports multiple scopes separated by a space:
  // https://code.google.com/p/google-api-java-client/wiki/OAuth2#Android
  std::string scope;
  for (std::vector<std::string>::const_iterator it = scopes.begin();
       it != scopes.end(); ++it) {
    if (!scope.empty())
      scope += " ";
    scope += *it;
  }
  return scope;
}

}  // namespace

bool OAuth2TokenServiceDelegateAndroid::is_testing_profile_ = false;

OAuth2TokenServiceDelegateAndroid::ErrorInfo::ErrorInfo()
    : error(GoogleServiceAuthError::NONE) {}

OAuth2TokenServiceDelegateAndroid::ErrorInfo::ErrorInfo(
    const GoogleServiceAuthError& error)
    : error(error) {}

OAuth2TokenServiceDelegateAndroid::OAuth2TokenServiceDelegateAndroid(
    AccountTrackerService* account_tracker_service)
    : account_tracker_service_(account_tracker_service),
      fire_refresh_token_loaded_(RT_LOAD_NOT_START) {
  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::ctor";
  DCHECK(account_tracker_service_);
  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> local_java_ref =
      Java_OAuth2TokenService_create(env,
                                     base::android::GetApplicationContext(),
                                     reinterpret_cast<intptr_t>(this));
  java_ref_.Reset(env, local_java_ref.obj());

  if (account_tracker_service_->GetMigrationState() ==
      AccountTrackerService::MIGRATION_IN_PROGRESS) {
    std::vector<std::string> accounts = GetAccounts();
    std::vector<std::string> accounts_id;
    for (auto account_name : accounts) {
      AccountInfo account_info =
          account_tracker_service_->FindAccountInfoByEmail(account_name);
      DCHECK(!account_info.gaia.empty());
      accounts_id.push_back(account_info.gaia);
    }
    ScopedJavaLocalRef<jobjectArray> java_accounts(
        base::android::ToJavaArrayOfStrings(env, accounts_id));
    Java_OAuth2TokenService_saveStoredAccounts(
        env, base::android::GetApplicationContext(), java_accounts.obj());
  }

  if (!is_testing_profile_) {
    Java_OAuth2TokenService_validateAccounts(
        AttachCurrentThread(), java_ref_.obj(),
        base::android::GetApplicationContext(), JNI_TRUE);
  }
}

OAuth2TokenServiceDelegateAndroid::~OAuth2TokenServiceDelegateAndroid() {
}

// static
ScopedJavaLocalRef<jobject> OAuth2TokenServiceDelegateAndroid::GetForProfile(
    JNIEnv* env,
    jclass clazz,
    jobject j_profile_android) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile_android);
  ProfileOAuth2TokenService* service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  OAuth2TokenServiceDelegate* delegate = service->GetDelegate();
  return ScopedJavaLocalRef<jobject>(
      static_cast<OAuth2TokenServiceDelegateAndroid*>(delegate)->java_ref_);
}

static ScopedJavaLocalRef<jobject> GetForProfile(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& j_profile_android) {
  return OAuth2TokenServiceDelegateAndroid::GetForProfile(env, clazz,
                                                          j_profile_android);
}

bool OAuth2TokenServiceDelegateAndroid::RefreshTokenIsAvailable(
    const std::string& account_id) const {
  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::RefreshTokenIsAvailable"
           << " account= " << account_id;
  std::string account_name = MapAccountIdToAccountName(account_id);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_account_id =
      ConvertUTF8ToJavaString(env, account_name);
  jboolean refresh_token_is_available =
      Java_OAuth2TokenService_hasOAuth2RefreshToken(
          env, base::android::GetApplicationContext(), j_account_id.obj());
  return refresh_token_is_available == JNI_TRUE;
}

bool OAuth2TokenServiceDelegateAndroid::RefreshTokenHasError(
    const std::string& account_id) const {
  auto it = errors_.find(account_id);
  // TODO(rogerta): should we distinguish between transient and persistent?
  return it == errors_.end() ? false : IsError(it->second.error);
}

void OAuth2TokenServiceDelegateAndroid::UpdateAuthError(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::UpdateAuthError"
           << " account=" << account_id
           << " error=" << error.ToString();
  if (error.state() == GoogleServiceAuthError::NONE) {
    errors_.erase(account_id);
  } else {
    // TODO(rogerta): should we distinguish between transient and persistent?
    errors_[account_id] = ErrorInfo(error);
  }
}

std::vector<std::string> OAuth2TokenServiceDelegateAndroid::GetAccounts() {
  std::vector<std::string> accounts;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> j_accounts =
      Java_OAuth2TokenService_getAccounts(
          env, base::android::GetApplicationContext());
  // TODO(fgorski): We may decide to filter out some of the accounts.
  base::android::AppendJavaStringArrayToStringVector(env, j_accounts.obj(),
                                                     &accounts);
  return accounts;
}

std::vector<std::string>
OAuth2TokenServiceDelegateAndroid::GetSystemAccountNames() {
  std::vector<std::string> account_names;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> j_accounts =
      Java_OAuth2TokenService_getSystemAccountNames(
          env, base::android::GetApplicationContext());
  base::android::AppendJavaStringArrayToStringVector(env, j_accounts.obj(),
                                                     &account_names);
  return account_names;
}

OAuth2AccessTokenFetcher*
OAuth2TokenServiceDelegateAndroid::CreateAccessTokenFetcher(
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    OAuth2AccessTokenConsumer* consumer) {
  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::CreateAccessTokenFetcher"
           << " account= " << account_id;
  ValidateAccountId(account_id);
  return new AndroidAccessTokenFetcher(consumer,
                                       MapAccountIdToAccountName(account_id));
}

void OAuth2TokenServiceDelegateAndroid::InvalidateAccessToken(
    const std::string& account_id,
    const std::string& client_id,
    const OAuth2TokenService::ScopeSet& scopes,
    const std::string& access_token) {
  ValidateAccountId(account_id);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_access_token =
      ConvertUTF8ToJavaString(env, access_token);
  Java_OAuth2TokenService_invalidateOAuth2AuthToken(
      env, base::android::GetApplicationContext(), j_access_token.obj());
}

void OAuth2TokenServiceDelegateAndroid::ValidateAccounts(
    JNIEnv* env,
    jobject obj,
    jstring j_current_acc,
    jboolean j_force_notifications) {
  std::string signed_in_account;
  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::ValidateAccounts from java";
  if (j_current_acc)
    signed_in_account = ConvertJavaStringToUTF8(env, j_current_acc);
  if (!signed_in_account.empty())
    signed_in_account = gaia::CanonicalizeEmail(signed_in_account);

  // Clear any auth errors so that client can retry to get access tokens.
  errors_.clear();

  ValidateAccounts(MapAccountNameToAccountId(signed_in_account),
                   j_force_notifications != JNI_FALSE);
}

void OAuth2TokenServiceDelegateAndroid::ValidateAccounts(
    const std::string& signed_in_account,
    bool force_notifications) {
  std::vector<std::string> prev_stored_ids = GetAccounts();
  std::vector<std::string> prev_ids;
  std::vector<std::string> curr_ids = GetSystemAccountNames();
  std::vector<std::string> refreshed_ids;
  std::vector<std::string> revoked_ids;
  bool account_validation_result = true;

  for (size_t i = 0; i < curr_ids.size(); ++i)
    curr_ids[i] = MapAccountNameToAccountId(curr_ids[i]);

  for (size_t i = 0; i < prev_stored_ids.size(); ++i) {
    if (ValidateAccountId(prev_stored_ids[i]))
      prev_ids.push_back(prev_stored_ids[i]);
  }

  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::ValidateAccounts:"
           << " sigined_in_account=" << signed_in_account
           << " prev_ids=" << prev_ids.size() << " curr_ids=" << curr_ids.size()
           << " force=" << (force_notifications ? "true" : "false");

  account_validation_result =
      ValidateAccounts(signed_in_account, prev_ids, curr_ids, refreshed_ids,
                       revoked_ids, force_notifications);

  ScopedBatchChange batch(this);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> java_accounts;
  if (account_validation_result) {
    java_accounts = base::android::ToJavaArrayOfStrings(env, curr_ids);
  } else {
    java_accounts =
        base::android::ToJavaArrayOfStrings(env, std::vector<std::string>());
  }

  // Save the current accounts in the token service before calling
  // FireRefreshToken* methods.
  Java_OAuth2TokenService_saveStoredAccounts(
      env, base::android::GetApplicationContext(), java_accounts.obj());

  for (std::vector<std::string>::iterator it = refreshed_ids.begin();
       it != refreshed_ids.end(); it++) {
    FireRefreshTokenAvailable(*it);
  }
  for (std::vector<std::string>::iterator it = revoked_ids.begin();
       it != revoked_ids.end(); it++) {
    FireRefreshTokenRevoked(*it);
  }
  if (fire_refresh_token_loaded_ == RT_WAIT_FOR_VALIDATION) {
    fire_refresh_token_loaded_ = RT_LOADED;
    FireRefreshTokensLoaded();
  } else if (fire_refresh_token_loaded_ == RT_LOAD_NOT_START) {
    fire_refresh_token_loaded_ = RT_HAS_BEEN_VALIDATED;
  }

  // Clear accounts no longer exist on device from AccountTrackerService.
  std::vector<AccountInfo> accounts_info =
      account_tracker_service_->GetAccounts();
  for (auto info : accounts_info) {
    auto it = curr_ids.begin();
    for (; it != curr_ids.end(); ++it) {
      if (*it == info.account_id)
        break;
    }
    if (it == curr_ids.end())
      account_tracker_service_->RemoveAccount(info.account_id);
  }

  // No need to wait for SigninManager to finish migration if not signed in.
  if (account_tracker_service_->GetMigrationState() ==
          AccountTrackerService::MIGRATION_IN_PROGRESS &&
      signed_in_account.empty()) {
    account_tracker_service_->SetMigrationDone();
  }
}

bool OAuth2TokenServiceDelegateAndroid::ValidateAccounts(
    const std::string& signed_in_account,
    const std::vector<std::string>& prev_account_ids,
    const std::vector<std::string>& curr_account_ids,
    std::vector<std::string>& refreshed_ids,
    std::vector<std::string>& revoked_ids,
    bool force_notifications) {
  if (std::find(curr_account_ids.begin(), curr_account_ids.end(),
                signed_in_account) != curr_account_ids.end()) {
    // Test to see if an account is removed from the Android AccountManager.
    // If so, invoke FireRefreshTokenRevoked to notify the reconcilor.
    for (std::vector<std::string>::const_iterator it = prev_account_ids.begin();
         it != prev_account_ids.end(); it++) {
      if (*it == signed_in_account)
        continue;

      if (std::find(curr_account_ids.begin(), curr_account_ids.end(), *it) ==
          curr_account_ids.end()) {
        DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::ValidateAccounts:"
                 << "revoked=" << *it;
        revoked_ids.push_back(*it);
      }
    }

    if (force_notifications ||
        std::find(prev_account_ids.begin(), prev_account_ids.end(),
                  signed_in_account) == prev_account_ids.end()) {
      // Always fire the primary signed in account first.
      DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::ValidateAccounts:"
               << "refreshed=" << signed_in_account;
      refreshed_ids.push_back(signed_in_account);
    }

    for (std::vector<std::string>::const_iterator it = curr_account_ids.begin();
         it != curr_account_ids.end(); it++) {
      if (*it != signed_in_account) {
        if (force_notifications ||
            std::find(prev_account_ids.begin(), prev_account_ids.end(), *it) ==
                prev_account_ids.end()) {
          DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::ValidateAccounts:"
                   << "refreshed=" << *it;
          refreshed_ids.push_back(*it);
        }
      }
    }
    return true;
  } else {
    // Currently signed in account does not any longer exist among accounts on
    // system together with all other accounts.
    if (std::find(prev_account_ids.begin(), prev_account_ids.end(),
                  signed_in_account) != prev_account_ids.end()) {
      DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::ValidateAccounts:"
               << "revoked=" << signed_in_account;
      revoked_ids.push_back(signed_in_account);
    }
    for (std::vector<std::string>::const_iterator it = prev_account_ids.begin();
         it != prev_account_ids.end(); it++) {
      if (*it == signed_in_account)
        continue;
      DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::ValidateAccounts:"
               << "revoked=" << *it;
      revoked_ids.push_back(*it);
    }
    return false;
  }
}

void OAuth2TokenServiceDelegateAndroid::FireRefreshTokenAvailableFromJava(
    JNIEnv* env,
    jobject obj,
    const jstring account_name) {
  std::string account_id =
      MapAccountNameToAccountId(ConvertJavaStringToUTF8(env, account_name));
  // Notify native observers.
  FireRefreshTokenAvailable(account_id);
}

void OAuth2TokenServiceDelegateAndroid::FireRefreshTokenAvailable(
    const std::string& account_id) {
  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::FireRefreshTokenAvailable id="
           << account_id;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> account_name =
      ConvertUTF8ToJavaString(env, MapAccountIdToAccountName(account_id));
  Java_OAuth2TokenService_notifyRefreshTokenAvailable(env, java_ref_.obj(),
                                                      account_name.obj());
  OAuth2TokenServiceDelegate::FireRefreshTokenAvailable(account_id);
}

void OAuth2TokenServiceDelegateAndroid::FireRefreshTokenRevokedFromJava(
    JNIEnv* env,
    jobject obj,
    const jstring account_name) {
  std::string account_id =
      MapAccountNameToAccountId(ConvertJavaStringToUTF8(env, account_name));
  // Notify native observers.
  FireRefreshTokenRevoked(account_id);
}

void OAuth2TokenServiceDelegateAndroid::FireRefreshTokenRevoked(
    const std::string& account_id) {
  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::FireRefreshTokenRevoked id="
           << account_id;
  JNIEnv* env = AttachCurrentThread();
  std::string account_name = MapAccountIdToAccountName(account_id);
  // Do not crash in case of missed information.
  if (!account_name.empty()) {
    ScopedJavaLocalRef<jstring> account =
        ConvertUTF8ToJavaString(env, account_name);
    Java_OAuth2TokenService_notifyRefreshTokenRevoked(env, java_ref_.obj(),
                                                      account.obj());
  }
  OAuth2TokenServiceDelegate::FireRefreshTokenRevoked(account_id);
}

void OAuth2TokenServiceDelegateAndroid::FireRefreshTokensLoadedFromJava(
    JNIEnv* env,
    jobject obj) {
  // Notify native observers.
  FireRefreshTokensLoaded();
}

void OAuth2TokenServiceDelegateAndroid::FireRefreshTokensLoaded() {
  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::FireRefreshTokensLoaded";
  JNIEnv* env = AttachCurrentThread();
  Java_OAuth2TokenService_notifyRefreshTokensLoaded(env, java_ref_.obj());
  OAuth2TokenServiceDelegate::FireRefreshTokensLoaded();
}

void OAuth2TokenServiceDelegateAndroid::RevokeAllCredentials() {
  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::RevokeAllCredentials";
  ScopedBatchChange batch(this);
  std::vector<std::string> accounts_to_revoke = GetAccounts();

  // Clear accounts in the token service before calling
  // |FireRefreshTokenRevoked|.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> java_accounts(
      base::android::ToJavaArrayOfStrings(env, std::vector<std::string>()));
  Java_OAuth2TokenService_saveStoredAccounts(
      env, base::android::GetApplicationContext(), java_accounts.obj());

  for (const std::string& account : accounts_to_revoke)
    FireRefreshTokenRevoked(account);
}

void OAuth2TokenServiceDelegateAndroid::LoadCredentials(
    const std::string& primary_account_id) {
  if (fire_refresh_token_loaded_ == RT_HAS_BEEN_VALIDATED) {
    fire_refresh_token_loaded_ = RT_LOADED;
    FireRefreshTokensLoaded();
  } else if (fire_refresh_token_loaded_ == RT_LOAD_NOT_START) {
    fire_refresh_token_loaded_ = RT_WAIT_FOR_VALIDATION;
  }
}

std::string OAuth2TokenServiceDelegateAndroid::MapAccountIdToAccountName(
    const std::string& account_id) const {
  std::string account_name =
      account_tracker_service_->GetAccountInfo(account_id).email;
  DCHECK(!account_name.empty() || account_id.empty());
  return account_name;
}

std::string OAuth2TokenServiceDelegateAndroid::MapAccountNameToAccountId(
    const std::string& account_name) const {
  std::string account_id =
      account_tracker_service_->FindAccountInfoByEmail(account_name).account_id;
  DCHECK(!account_id.empty() || account_name.empty());
  return account_id;
}

// Called from Java when fetching of an OAuth2 token is finished. The
// |authToken| param is only valid when |result| is true.
void OAuth2TokenFetched(JNIEnv* env,
                        const JavaParamRef<jclass>& clazz,
                        const JavaParamRef<jstring>& authToken,
                        jboolean isTransientError,
                        jlong nativeCallback) {
  std::string token;
  if (authToken)
    token = ConvertJavaStringToUTF8(env, authToken);
  scoped_ptr<FetchOAuth2TokenCallback> heap_callback(
      reinterpret_cast<FetchOAuth2TokenCallback*>(nativeCallback));
  GoogleServiceAuthError
      err(authToken
              ? GoogleServiceAuthError::NONE
              : isTransientError
                    ? GoogleServiceAuthError::CONNECTION_FAILED
                    : GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  heap_callback->Run(err, token, base::Time());
}

// static
bool OAuth2TokenServiceDelegateAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
