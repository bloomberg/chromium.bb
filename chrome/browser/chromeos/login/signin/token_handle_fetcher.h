// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_TOKEN_HANDLE_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_TOKEN_HANDLE_FETCHER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace identity {
class PrimaryAccountAccessTokenFetcher;
}

class TokenHandleUtil;
class Profile;

// This class is resposible for obtaining new token handle for user.
// It can be used in two ways. When user have just used GAIA signin there is
// an OAuth2 token available. If there is profile already loaded, then
// minting additional access token might be required.

class TokenHandleFetcher : public gaia::GaiaOAuthClient::Delegate,
                           public identity::IdentityManager::Observer {
 public:
  TokenHandleFetcher(TokenHandleUtil* util, const AccountId& account_id);
  ~TokenHandleFetcher() override;

  using TokenFetchingCallback =
      base::Callback<void(const AccountId&, bool success)>;

  // Get token handle for user who have just signed in via GAIA. This
  // request will be performed using signin profile.
  void FillForNewUser(const std::string& access_token,
                      const TokenFetchingCallback& callback);

  // Get token handle for existing user.
  void BackfillToken(Profile* profile, const TokenFetchingCallback& callback);

 private:
  // identity::IdentityManager::Observer override:
  void OnRefreshTokenUpdatedForAccount(const AccountInfo& account_info,
                                       bool is_valid) override;

  // AccessTokenFetcher::TokenCallback for PrimaryAccountAccessTokenFetcher.
  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  identity::AccessTokenInfo token_info);

  // GaiaOAuthClient::Delegate overrides:
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;
  void OnGetTokenInfoResponse(
      std::unique_ptr<base::DictionaryValue> token_info) override;

  void RequestAccessToken(const std::string& user_email);
  void FillForAccessToken(const std::string& access_token);

  // This is called before profile is detroyed.
  void OnProfileDestroyed();

  TokenHandleUtil* token_handle_util_ = nullptr;
  AccountId account_id_;
  identity::IdentityManager* identity_manager_ = nullptr;

  bool waiting_for_refresh_token_ = false;
  std::string account_without_token_;
  Profile* profile_ = nullptr;
  base::TimeTicks tokeninfo_response_start_time_ = base::TimeTicks();
  TokenFetchingCallback callback_;
  std::unique_ptr<gaia::GaiaOAuthClient> gaia_client_;
  std::unique_ptr<identity::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;
  std::unique_ptr<KeyedServiceShutdownNotifier::Subscription>
      profile_shutdown_notification_;

  DISALLOW_COPY_AND_ASSIGN(TokenHandleFetcher);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_TOKEN_HANDLE_FETCHER_H_
