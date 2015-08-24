// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_OAUTH2_TOKEN_SERVICE_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_SIGNIN_OAUTH2_TOKEN_SERVICE_DELEGATE_ANDROID_H_

#include <jni.h>
#include <map>
#include <string>

#include "base/android/jni_weak_ref.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_token_service_delegate.h"

// A specialization of OAuth2TokenServiceDelegate that will be returned by
// OAuth2TokenServiceDelegateFactory for OS_ANDROID.  This instance uses
// native Android features to lookup OAuth2 tokens.
//
// See |OAuth2TokenServiceDelegate| for usage details.
//
// Note: requests should be started from the UI thread. To start a
// request from other thread, please use OAuth2TokenServiceRequest.
class OAuth2TokenServiceDelegateAndroid : public OAuth2TokenServiceDelegate {
 public:
  // Registers the OAuth2TokenServiceDelegateAndroid's native methods through
  // JNI.
  static bool Register(JNIEnv* env);

  // Creates a new instance of the OAuth2TokenServiceDelegateAndroid.
  static OAuth2TokenServiceDelegateAndroid* Create();

  // Returns a reference to the Java instance of this service.
  static jobject GetForProfile(JNIEnv* env,
                               jclass clazz,
                               jobject j_profile_android);

  // Called by the TestingProfile class to disable account validation in
  // tests.  This prevents the token service from trying to look up system
  // accounts which requires special permission.
  static void set_is_testing_profile() { is_testing_profile_ = true; }

  void Initialize();

  // OAuth2TokenServiceDelegate overrides:
  bool RefreshTokenIsAvailable(const std::string& account_id) const override;
  bool RefreshTokenHasError(const std::string& account_id) const override;
  void UpdateAuthError(const std::string& account_id,
                       const GoogleServiceAuthError& error) override;
  std::vector<std::string> GetAccounts() override;

  // Lists account at the OS level.
  std::vector<std::string> GetSystemAccounts();

  void ValidateAccounts(JNIEnv* env,
                        jobject obj,
                        jstring current_account,
                        jboolean force_notifications);

  // Takes a the signed in sync account as well as all the other
  // android account ids and check the token status of each.  If
  // |force_notifications| is true, TokenAvailable notifications will
  // be sent anyway, even if the account was already known.
  void ValidateAccounts(const std::string& signed_in_account,
                        bool force_notifications);

  // Triggers a notification to all observers of the OAuth2TokenService that a
  // refresh token is now available. This may cause observers to retry
  // operations that require authentication.
  virtual void FireRefreshTokenAvailableFromJava(JNIEnv* env,
                                                 jobject obj,
                                                 const jstring account_name);
  // Triggers a notification to all observers of the OAuth2TokenService that a
  // refresh token is now available.
  virtual void FireRefreshTokenRevokedFromJava(JNIEnv* env,
                                               jobject obj,
                                               const jstring account_name);
  // Triggers a notification to all observers of the OAuth2TokenService that all
  // refresh tokens have now been loaded.
  virtual void FireRefreshTokensLoadedFromJava(JNIEnv* env, jobject obj);

  // Overridden from OAuth2TokenService to complete signout of all
  // OA2TService aware accounts.
  void RevokeAllCredentials() override;

 protected:
  friend class ProfileOAuth2TokenServiceFactory;
  OAuth2TokenServiceDelegateAndroid();
  ~OAuth2TokenServiceDelegateAndroid() override;

  OAuth2AccessTokenFetcher* CreateAccessTokenFetcher(
      const std::string& account_id,
      net::URLRequestContextGetter* getter,
      OAuth2AccessTokenConsumer* consumer) override;

  // Overridden from OAuth2TokenService to intercept token fetch requests and
  // redirect them to the Account Manager.
  void InvalidateAccessToken(const std::string& account_id,
                             const std::string& client_id,
                             const OAuth2TokenService::ScopeSet& scopes,
                             const std::string& access_token) override;

  // Called to notify observers when a refresh token is available.
  void FireRefreshTokenAvailable(const std::string& account_id) override;
  // Called to notify observers when a refresh token has been revoked.
  void FireRefreshTokenRevoked(const std::string& account_id) override;
  // Called to notify observers when refresh tokans have been loaded.
  void FireRefreshTokensLoaded() override;

 private:
  struct ErrorInfo {
    ErrorInfo();
    explicit ErrorInfo(const GoogleServiceAuthError& error);
    GoogleServiceAuthError error;
  };

  // Return whether |signed_in_account| is valid and we have access
  // to all the tokens in |curr_account_ids|. If |force_notifications| is true,
  // TokenAvailable notifications will be sent anyway, even if the account was
  // already known.
  bool ValidateAccounts(const std::string& signed_in_account,
                        const std::vector<std::string>& prev_account_ids,
                        const std::vector<std::string>& curr_account_ids,
                        std::vector<std::string>& refreshed_ids,
                        std::vector<std::string>& revoked_ids,
                        bool force_notifications);

  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  // Maps account_id to the last error for that account.
  std::map<std::string, ErrorInfo> errors_;

  static bool is_testing_profile_;

  DISALLOW_COPY_AND_ASSIGN(OAuth2TokenServiceDelegateAndroid);
};

#endif  // CHROME_BROWSER_SIGNIN_OAUTH2_TOKEN_SERVICE_DELEGATE_ANDROID_H_
