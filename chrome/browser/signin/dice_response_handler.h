// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_DICE_RESPONSE_HANDLER_H_
#define CHROME_BROWSER_SIGNIN_DICE_RESPONSE_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "google_apis/gaia/gaia_auth_consumer.h"

namespace signin {
struct DiceResponseParams;
}

class AccountTrackerService;
class GaiaAuthFetcher;
class GoogleServiceAuthError;
class SigninClient;
class SigninManager;
class ProfileOAuth2TokenService;
class Profile;

// Exposed for testing.
extern const int kDiceTokenFetchTimeoutSeconds;

// Delegate interface for processing a dice request.
class ProcessDiceHeaderObserver {
 public:
  virtual ~ProcessDiceHeaderObserver() = default;

  // Called before starting to fetch a refresh token.
  virtual void WillStartRefreshTokenFetch(const std::string& gaia_id,
                                          const std::string& email) = 0;

  // Called after the refresh token was fetched and added in the token service.
  virtual void DidFinishRefreshTokenFetch(const std::string& gaia_id,
                                          const std::string& email) = 0;
};

// Processes the Dice responses from Gaia.
class DiceResponseHandler : public KeyedService {
 public:
  // Returns the DiceResponseHandler associated with this profile.
  // May return nullptr if there is none (e.g. in incognito).
  static DiceResponseHandler* GetForProfile(Profile* profile);

  DiceResponseHandler(SigninClient* signin_client,
                      SigninManager* signin_manager,
                      ProfileOAuth2TokenService* profile_oauth2_token_service,
                      AccountTrackerService* account_tracker_service,
                      AccountReconcilor* account_reconcilor);
  ~DiceResponseHandler() override;

  // Must be called when receiving a Dice response header.
  void ProcessDiceHeader(const signin::DiceResponseParams& dice_params,
                         std::unique_ptr<ProcessDiceHeaderObserver> observer);

  // Returns the number of pending DiceTokenFetchers. Exposed for testing.
  size_t GetPendingDiceTokenFetchersCountForTesting() const;

 private:
  // Helper class to fetch a refresh token from an authorization code.
  class DiceTokenFetcher : public GaiaAuthConsumer {
   public:
    DiceTokenFetcher(const std::string& gaia_id,
                     const std::string& email,
                     const std::string& authorization_code,
                     SigninClient* signin_client,
                     AccountReconcilor* account_reconcilor,
                     std::unique_ptr<ProcessDiceHeaderObserver> observer,
                     DiceResponseHandler* dice_response_handler);
    ~DiceTokenFetcher() override;

    const std::string& gaia_id() const { return gaia_id_; }
    const std::string& email() const { return email_; }
    const std::string& authorization_code() const {
      return authorization_code_;
    }

   private:
    // Called by |timeout_closure_| when the request times out.
    void OnTimeout();

    // GaiaAuthConsumer implementation:
    void OnClientOAuthSuccess(
        const GaiaAuthConsumer::ClientOAuthResult& result) override;
    void OnClientOAuthFailure(const GoogleServiceAuthError& error) override;

    // Lock the account reconcilor while tokens are being fetched.
    std::unique_ptr<AccountReconcilor::Lock> account_reconcilor_lock_;

    std::string gaia_id_;
    std::string email_;
    std::string authorization_code_;
    std::unique_ptr<ProcessDiceHeaderObserver> observer_;
    DiceResponseHandler* dice_response_handler_;
    base::CancelableClosure timeout_closure_;
    std::unique_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;

    DISALLOW_COPY_AND_ASSIGN(DiceTokenFetcher);
  };

  // Deletes the token fetcher.
  void DeleteTokenFetcher(DiceTokenFetcher* token_fetcher);

  // Returns true if it is acceptable to get a new token for the account.
  // Always returns true when using kDice.
  bool CanGetTokenForAccount(const std::string& gaia_id,
                             const std::string& email);

  // Process the Dice signin action.
  void ProcessDiceSigninHeader(
      const std::string& gaia_id,
      const std::string& email,
      const std::string& authorization_code,
      std::unique_ptr<ProcessDiceHeaderObserver> observer);

  // Process the Dice signout action.
  void ProcessDiceSignoutHeader(const std::vector<std::string>& gaia_ids,
                                const std::vector<std::string>& emails);

  // Called after exchanging an OAuth 2.0 authorization code for a refresh token
  // after DiceAction::SIGNIN.
  void OnTokenExchangeSuccess(
      DiceTokenFetcher* token_fetcher,
      std::string gaia_id,
      std::string email,
      std::string refresh_token,
      std::unique_ptr<ProcessDiceHeaderObserver> observer);
  void OnTokenExchangeFailure(DiceTokenFetcher* token_fetcher,
                              const GoogleServiceAuthError& error);

  SigninManager* signin_manager_;
  SigninClient* signin_client_;
  ProfileOAuth2TokenService* token_service_;
  AccountTrackerService* account_tracker_service_;
  AccountReconcilor* account_reconcilor_;
  std::vector<std::unique_ptr<DiceTokenFetcher>> token_fetchers_;

  DISALLOW_COPY_AND_ASSIGN(DiceResponseHandler);
};

#endif  // CHROME_BROWSER_SIGNIN_DICE_RESPONSE_HANDLER_H_
