// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_DICE_RESPONSE_HANDLER_H_
#define CHROME_BROWSER_SIGNIN_DICE_RESPONSE_HANDLER_H_

#include <memory>
#include <string>

#include "components/keyed_service/core/keyed_service.h"
#include "google_apis/gaia/gaia_auth_consumer.h"

namespace signin {
struct DiceResponseParams;
}

class AccountTrackerService;
class GaiaAuthFetcher;
class SigninClient;
class ProfileOAuth2TokenService;
class Profile;

// Processes the Dice responses from Gaia.
class DiceResponseHandler : public GaiaAuthConsumer, public KeyedService {
 public:
  // Returns the DiceResponseHandler associated with this profile.
  // May return nullptr if there is none (e.g. in incognito).
  static DiceResponseHandler* GetForProfile(Profile* profile);

  DiceResponseHandler(SigninClient* signin_client,
                      ProfileOAuth2TokenService* profile_oauth2_token_service,
                      AccountTrackerService* account_tracker_service);
  ~DiceResponseHandler() override;

  // Must be called when receiving a Dice response header.
  void ProcessDiceHeader(const signin::DiceResponseParams& dice_params);

 private:
  // Process the Dice signin action.
  void ProcessDiceSigninHeader(const std::string& gaia_id,
                               const std::string& email,
                               const std::string& authorization_code);

  // GaiaAuthConsumer implementation:
  void OnClientOAuthSuccess(const ClientOAuthResult& result) override;
  void OnClientOAuthFailure(const GoogleServiceAuthError& error) override;

  std::unique_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;
  std::string gaia_id_;
  std::string email_;
  SigninClient* signin_client_;
  ProfileOAuth2TokenService* token_service_;
  AccountTrackerService* account_tracker_service_;
};

#endif  // CHROME_BROWSER_SIGNIN_DICE_RESPONSE_HANDLER_H_
