// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_BOOTSTRAP_USER_CONTEXT_INITIALIZER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_BOOTSTRAP_USER_CONTEXT_INITIALIZER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/login/auth/user_context.h"
#include "google_apis/gaia/gaia_oauth_client.h"

namespace chromeos {

// Performs initialization work for adding a new account via Easy bootstrap.
// The class takes an authorization code then prepares a UserContext that
// could be used to create a new account.
class BootstrapUserContextInitializer final
    : public gaia::GaiaOAuthClient::Delegate {
 public:
  // Callback type to be invoked after initialization work is done.
  typedef base::Callback<void(bool success, const UserContext& user_context)>
      CompleteCallback;

  BootstrapUserContextInitializer();
  ~BootstrapUserContextInitializer() override;

  // Run initialization work with given |auth_code|. |callback| will be invoked
  // when the work is done.
  void Start(const std::string& auth_code, const CompleteCallback& callback);

  const UserContext& user_context() const { return user_context_; }

  static void SetCompleteCallbackForTesting(CompleteCallback* callback);

 private:
  void InitializeUserContext();
  void Notify(bool success);

  // Start refresh token and user info fetching.
  void StartTokenFetch(const std::string& auth_code);

  // gaia::GaiaOAuthClient::Delegate
  void OnGetTokensResponse(const std::string& refresh_token,
                           const std::string& access_token,
                           int expires_in_seconds) override;
  void OnRefreshTokenResponse(const std::string& access_token,
                              int expires_in_seconds) override;
  void OnGetUserInfoResponse(
      scoped_ptr<base::DictionaryValue> user_info) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

  CompleteCallback callback_;
  scoped_ptr<gaia::GaiaOAuthClient> token_fetcher_;

  UserContext user_context_;

  static CompleteCallback* complete_callback_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(BootstrapUserContextInitializer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_BOOTSTRAP_USER_CONTEXT_INITIALIZER_H_
