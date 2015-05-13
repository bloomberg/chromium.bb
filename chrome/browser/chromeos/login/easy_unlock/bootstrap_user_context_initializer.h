// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_BOOTSTRAP_USER_CONTEXT_INITIALIZER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_BOOTSTRAP_USER_CONTEXT_INITIALIZER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_types.h"
#include "chrome/browser/signin/easy_unlock_auth_attempt.h"
#include "chrome/browser/signin/easy_unlock_service_observer.h"
#include "chromeos/login/auth/user_context.h"
#include "google_apis/gaia/gaia_oauth_client.h"

namespace chromeos {

// Performs initialization work for adding a new account via Easy bootstrap.
// The class takes an authorization code then prepares a UserContext that
// could be used to create a new account.
class BootstrapUserContextInitializer final
    : public gaia::GaiaOAuthClient::Delegate,
      public EasyUnlockServiceObserver {
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
  bool random_key_used() const { return random_key_used_; }

  static void SetCompleteCallbackForTesting(CompleteCallback* callback);

 private:
  void Notify(bool success);

  // Starts to check existing Smart lock keys to use.
  void StartCheckExistingKeys();

  void OnGetEasyUnlockData(bool success,
                           const EasyUnlockDeviceKeyDataList& data_list);
  void OnEasyUnlockAuthenticated(EasyUnlockAuthAttempt::Type auth_attempt_type,
                                 bool success,
                                 const std::string& user_id,
                                 const std::string& key_secret,
                                 const std::string& key_label);
  void CreateRandomKey();

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

  // EasyUnlockServiceObserver:
  void OnScreenlockStateChanged(proximity_auth::ScreenlockState state) override;

  CompleteCallback callback_;
  scoped_ptr<gaia::GaiaOAuthClient> token_fetcher_;

  UserContext user_context_;
  bool random_key_used_;

  base::WeakPtrFactory<BootstrapUserContextInitializer> weak_ptr_factory_;

  static CompleteCallback* complete_callback_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(BootstrapUserContextInitializer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_BOOTSTRAP_USER_CONTEXT_INITIALIZER_H_
