// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_IOS_BROWSER_PROFILE_OAUTH2_TOKEN_SERVICE_IOS_H_
#define COMPONENTS_SIGNIN_IOS_BROWSER_PROFILE_OAUTH2_TOKEN_SERVICE_IOS_H_

#include <string>

#include "base/threading/thread_checker.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_error_controller.h"

class OAuth2AccessTokenFetcher;

namespace ios{
class ProfileOAuth2TokenServiceIOSProvider;
}

// A specialization of ProfileOAuth2TokenService that will be returned by
// ProfileOAuth2TokenServiceFactory for OS_IOS when iOS authentication service
// is used to lookup OAuth2 tokens.
//
// See |ProfileOAuth2TokenService| for usage details.
//
// Note: Requests should be started from the UI thread. To start a
// request from aother thread, please use OAuth2TokenServiceRequest.
class ProfileOAuth2TokenServiceIOS : public ProfileOAuth2TokenService {
 public:
  // KeyedService
  void Shutdown() override;

  // OAuth2TokenService
  bool RefreshTokenIsAvailable(const std::string& account_id) const override;

  void InvalidateOAuth2Token(const std::string& account_id,
                             const std::string& client_id,
                             const ScopeSet& scopes,
                             const std::string& access_token) override;

  // ProfileOAuth2TokenService
  void Initialize(SigninClient* client,
                  SigninErrorController* signin_error_controller) override;
  void LoadCredentials(const std::string& primary_account_id) override;
  std::vector<std::string> GetAccounts() override;
  void UpdateAuthError(const std::string& account_id,
                       const GoogleServiceAuthError& error) override;

  // This method should not be called when using shared authentication.
  void UpdateCredentials(const std::string& account_id,
                         const std::string& refresh_token) override;

  // Removes all credentials from this instance of |ProfileOAuth2TokenService|,
  // however, it does not revoke the identities from the device.
  // Subsequent calls to |RefreshTokenIsAvailable| will return |false|.
  void RevokeAllCredentials() override;

  // Reloads accounts from the provider. Fires |OnRefreshTokenAvailable| for
  // each new account. Fires |OnRefreshTokenRevoked| for each account that was
  // removed.
  // It expects that there is already a primary account id.
  void ReloadCredentials();

  // Sets the primary account and then reloads the accounts from the provider.
  // Should be called when the user signs in to a new account.
  // |primary_account_id| must not be an empty string.
  void ReloadCredentials(const std::string& primary_account_id);

  // Sets the account that should be ignored by this token service.
  // |ReloadCredentials| needs to be called for this change to be effective.
  void ExcludeSecondaryAccount(const std::string& account_id);
  void IncludeSecondaryAccount(const std::string& account_id);
  void ExcludeSecondaryAccounts(const std::vector<std::string>& account_ids);

  // Excludes all secondary accounts. |ReloadCredentials| needs to be called for
  // this change to be effective.
  void ExcludeAllSecondaryAccounts();

 protected:
  friend class ProfileOAuth2TokenServiceFactory;
  friend class ProfileOAuth2TokenServiceIOSTest;
  FRIEND_TEST_ALL_PREFIXES(ProfileOAuth2TokenServiceIOSTest,
                           ExcludeSecondaryAccounts);
  FRIEND_TEST_ALL_PREFIXES(ProfileOAuth2TokenServiceIOSTest,
                           LoadRevokeCredentialsClearsExcludedAccounts);

  ProfileOAuth2TokenServiceIOS();
  ~ProfileOAuth2TokenServiceIOS() override;

  OAuth2AccessTokenFetcher* CreateAccessTokenFetcher(
      const std::string& account_id,
      net::URLRequestContextGetter* getter,
      OAuth2AccessTokenConsumer* consumer) override;

  // Protected and virtual to be overriden by fake for testing.

  // Adds |account_id| to |accounts_| if it does not exist or udpates
  // the auth error state of |account_id| if it exists. Fires
  // |OnRefreshTokenAvailable| if the account info is updated.
  virtual void AddOrUpdateAccount(const std::string& account_id);

  // Removes |account_id| from |accounts_|. Fires |OnRefreshTokenRevoked|
  // if the account info is removed.
  virtual void RemoveAccount(const std::string& account_id);

 private:
  class AccountInfo : public SigninErrorController::AuthStatusProvider {
   public:
    AccountInfo(SigninErrorController* signin_error_controller,
                const std::string& account_id);
    ~AccountInfo() override;

    void SetLastAuthError(const GoogleServiceAuthError& error);

    // SigninErrorController::AuthStatusProvider implementation.
    std::string GetAccountId() const override;
    std::string GetUsername() const override;
    GoogleServiceAuthError GetAuthStatus() const override;

    bool marked_for_removal() const { return marked_for_removal_; }
    void set_marked_for_removal(bool marked_for_removal) {
      marked_for_removal_ = marked_for_removal;
    }

   private:
    SigninErrorController* signin_error_controller_;
    std::string account_id_;
    GoogleServiceAuthError last_auth_error_;
    bool marked_for_removal_;

    DISALLOW_COPY_AND_ASSIGN(AccountInfo);
  };

  // Maps the |account_id| of accounts known to ProfileOAuth2TokenService
  // to information about the account.
  typedef std::map<std::string, linked_ptr<AccountInfo> > AccountInfoMap;

  // Returns the iOS provider;
  ios::ProfileOAuth2TokenServiceIOSProvider* GetProvider();

  // Returns the account ids that should be ignored by this token service.
  std::set<std::string> GetExcludedSecondaryAccounts();

  // Returns true if this token service should exclude all secondary accounts.
  bool GetExcludeAllSecondaryAccounts();

  // Clears exclude secondary accounts preferences.
  void ClearExcludedSecondaryAccounts();

  // The primary account id.
  std::string primary_account_id_;

  // Info about the existing accounts.
  AccountInfoMap accounts_;

  // Calls to this class are expected to be made from the browser UI thread.
  // The purpose of this checker is to detect access to
  // ProfileOAuth2TokenService from multiple threads in upstream code.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ProfileOAuth2TokenServiceIOS);
};

#endif  // COMPONENTS_SIGNIN_IOS_BROWSER_PROFILE_OAUTH2_TOKEN_SERVICE_IOS_H_
