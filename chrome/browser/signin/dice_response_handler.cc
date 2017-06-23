// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_response_handler.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace {

class DiceResponseHandlerFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns an instance of the factory singleton.
  static DiceResponseHandlerFactory* GetInstance() {
    return base::Singleton<DiceResponseHandlerFactory>::get();
  }

  static DiceResponseHandler* GetForProfile(Profile* profile) {
    return static_cast<DiceResponseHandler*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

 private:
  friend struct base::DefaultSingletonTraits<DiceResponseHandlerFactory>;

  DiceResponseHandlerFactory()
      : BrowserContextKeyedServiceFactory(
            "DiceResponseHandler",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(AccountTrackerServiceFactory::GetInstance());
    DependsOn(ChromeSigninClientFactory::GetInstance());
    DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  }

  ~DiceResponseHandlerFactory() override {}

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    if (context->IsOffTheRecord())
      return nullptr;

    Profile* profile = static_cast<Profile*>(context);
    return new DiceResponseHandler(
        ChromeSigninClientFactory::GetForProfile(profile),
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
        AccountTrackerServiceFactory::GetForProfile(profile));
  }
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DiceTokenFetcher
////////////////////////////////////////////////////////////////////////////////

DiceResponseHandler::DiceTokenFetcher::DiceTokenFetcher(
    const std::string& gaia_id,
    const std::string& email,
    const std::string& authorization_code,
    SigninClient* signin_client,
    DiceResponseHandler* dice_response_handler)
    : gaia_id_(gaia_id),
      email_(email),
      authorization_code_(authorization_code),
      dice_response_handler_(dice_response_handler) {
  gaia_auth_fetcher_ = signin_client->CreateGaiaAuthFetcher(
      this, GaiaConstants::kChromeSource,
      signin_client->GetURLRequestContext());
  gaia_auth_fetcher_->StartAuthCodeForOAuth2TokenExchange(authorization_code_);

  // TODO(droger): The token exchange must complete quickly or be cancelled. Add
  // a timeout logic.
}

DiceResponseHandler::DiceTokenFetcher::~DiceTokenFetcher() {}

void DiceResponseHandler::DiceTokenFetcher::OnClientOAuthSuccess(
    const GaiaAuthConsumer::ClientOAuthResult& result) {
  dice_response_handler_->OnTokenExchangeSuccess(this, gaia_id_, email_,
                                                 result);
}

void DiceResponseHandler::DiceTokenFetcher::OnClientOAuthFailure(
    const GoogleServiceAuthError& error) {
  dice_response_handler_->OnTokenExchangeFailure(this, error);
}

////////////////////////////////////////////////////////////////////////////////
// DiceResponseHandler
////////////////////////////////////////////////////////////////////////////////

// static
DiceResponseHandler* DiceResponseHandler::GetForProfile(Profile* profile) {
  return DiceResponseHandlerFactory::GetForProfile(profile);
}

DiceResponseHandler::DiceResponseHandler(
    SigninClient* signin_client,
    ProfileOAuth2TokenService* profile_oauth2_token_service,
    AccountTrackerService* account_tracker_service)
    : signin_client_(signin_client),
      token_service_(profile_oauth2_token_service),
      account_tracker_service_(account_tracker_service) {
  DCHECK(signin_client_);
  DCHECK(token_service_);
  DCHECK(account_tracker_service_);
}

DiceResponseHandler::~DiceResponseHandler() {}

void DiceResponseHandler::ProcessDiceHeader(
    const signin::DiceResponseParams& dice_params) {
  DCHECK_EQ(switches::AccountConsistencyMethod::kDice,
            switches::GetAccountConsistencyMethod());

  switch (dice_params.user_intention) {
    case signin::DiceAction::SIGNIN:
      ProcessDiceSigninHeader(dice_params.obfuscated_gaia_id, dice_params.email,
                              dice_params.authorization_code);
      return;
    case signin::DiceAction::SIGNOUT:
    case signin::DiceAction::SINGLE_SESSION_SIGNOUT:
      LOG(ERROR) << "Signout through Dice is not implemented.";
      return;
    case signin::DiceAction::NONE:
      NOTREACHED() << "Invalid Dice response parameters.";
      return;
  }

  NOTREACHED();
  return;
}

void DiceResponseHandler::ProcessDiceSigninHeader(
    const std::string& gaia_id,
    const std::string& email,
    const std::string& authorization_code) {
  DCHECK(!gaia_id.empty());
  DCHECK(!email.empty());
  DCHECK(!authorization_code.empty());

  for (auto it = token_fetchers_.begin(); it != token_fetchers_.end(); ++it) {
    if ((it->get()->gaia_id() == gaia_id) && (it->get()->email() == email) &&
        (it->get()->authorization_code() == authorization_code)) {
      return;  // There is already a request in flight with the same parameters.
    }
  }

  token_fetchers_.push_back(base::MakeUnique<DiceTokenFetcher>(
      gaia_id, email, authorization_code, signin_client_, this));
}

void DiceResponseHandler::DeleteTokenFetcher(DiceTokenFetcher* token_fetcher) {
  for (auto it = token_fetchers_.begin(); it != token_fetchers_.end(); ++it) {
    if (it->get() == token_fetcher) {
      token_fetchers_.erase(it);
      return;
    }
  }
  NOTREACHED();
}

void DiceResponseHandler::OnTokenExchangeSuccess(
    DiceTokenFetcher* token_fetcher,
    const std::string& gaia_id,
    const std::string& email,
    const GaiaAuthConsumer::ClientOAuthResult& result) {
  std::string account_id =
      account_tracker_service_->SeedAccountInfo(gaia_id, email);
  VLOG(1) << "Dice OAuth success for account: " << account_id;
  token_service_->UpdateCredentials(account_id, result.refresh_token);
  DeleteTokenFetcher(token_fetcher);
}

void DiceResponseHandler::OnTokenExchangeFailure(
    DiceTokenFetcher* token_fetcher,
    const GoogleServiceAuthError& error) {
  // TODO(droger): Handle authentication errors.
  VLOG(1) << "Dice OAuth failed with error: " << error.ToString();
  DeleteTokenFetcher(token_fetcher);
}
