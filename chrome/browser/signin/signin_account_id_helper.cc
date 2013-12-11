// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_account_id_helper.h"

#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "google_apis/gaia/gaia_oauth_client.h"

// TODO(guohui): this class should be moved to a more generic place for reuse.
class SigninAccountIdHelper::AccountIdFetcher
    : public OAuth2TokenService::Consumer,
      public gaia::GaiaOAuthClient::Delegate {
 public:
  AccountIdFetcher(Profile* profile,
                   SigninAccountIdHelper* signin_account_id_helper);
  virtual ~AccountIdFetcher();

  // OAuth2TokenService::Consumer implementation.
  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE;

  // gaia::GaiaOAuthClient::Delegate implementation.
  virtual void OnGetUserIdResponse(const std::string& gaia_id) OVERRIDE;
  virtual void OnOAuthError() OVERRIDE;
  virtual void OnNetworkError(int response_code) OVERRIDE;

 private:
  void Start();

  Profile* profile_;
  SigninAccountIdHelper* signin_account_id_helper_;

  scoped_ptr<OAuth2TokenService::Request> login_token_request_;
  scoped_ptr<gaia::GaiaOAuthClient> gaia_oauth_client_;

  DISALLOW_COPY_AND_ASSIGN(AccountIdFetcher);
};

SigninAccountIdHelper::AccountIdFetcher::AccountIdFetcher(
    Profile* profile,
    SigninAccountIdHelper* signin_account_id_helper)
    : profile_(profile),
      signin_account_id_helper_(signin_account_id_helper) {
  Start();
}

SigninAccountIdHelper::AccountIdFetcher::~AccountIdFetcher() {}

void SigninAccountIdHelper::AccountIdFetcher::Start() {
  ProfileOAuth2TokenService* service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  login_token_request_ = service->StartRequest(
      service->GetPrimaryAccountId(), OAuth2TokenService::ScopeSet(), this);
}

void SigninAccountIdHelper::AccountIdFetcher::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK_EQ(request, login_token_request_.get());

  gaia_oauth_client_.reset(
      new gaia::GaiaOAuthClient(profile_->GetRequestContext()));

  const int kMaxGetUserIdRetries = 3;
  gaia_oauth_client_->GetUserId(access_token, kMaxGetUserIdRetries, this);
}

void SigninAccountIdHelper::AccountIdFetcher::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  VLOG(1) << "OnGetTokenFailure: " << error.error_message();
  DCHECK_EQ(request, login_token_request_.get());
  signin_account_id_helper_->OnPrimaryAccountIdFetched("");
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void SigninAccountIdHelper::AccountIdFetcher::OnGetUserIdResponse(
    const std::string& account_id) {
  signin_account_id_helper_->OnPrimaryAccountIdFetched(account_id);
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void SigninAccountIdHelper::AccountIdFetcher::OnOAuthError() {
  VLOG(1) << "OnOAuthError";
  signin_account_id_helper_->OnPrimaryAccountIdFetched("");
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void SigninAccountIdHelper::AccountIdFetcher::OnNetworkError(
    int response_code) {
  VLOG(1) << "OnNetworkError " << response_code;
  signin_account_id_helper_->OnPrimaryAccountIdFetched("");
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

SigninAccountIdHelper::SigninAccountIdHelper(Profile* profile)
    : profile_(profile) {
  registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_SIGNED_OUT,
                 content::Source<Profile>(profile_));

  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  std::string primary_email = token_service->GetPrimaryAccountId();
  if (!primary_email.empty() &&
      token_service->RefreshTokenIsAvailable(primary_email)) {
    // AccountIdFetcher will delete itself.
    new AccountIdFetcher(profile_, this);
  }
  token_service->AddObserver(this);
}

SigninAccountIdHelper::~SigninAccountIdHelper() {
  ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)->
      RemoveObserver(this);
}

void SigninAccountIdHelper::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_GOOGLE_SIGNED_OUT)
    profile_->GetPrefs()->ClearPref(prefs::kGoogleServicesUserAccountId);
}

void SigninAccountIdHelper::OnRefreshTokenAvailable(const std::string& email) {
  SigninManagerBase* manager =
      SigninManagerFactory::GetForProfile(profile_);
  if (email == manager->GetAuthenticatedUsername()) {
    std::string current_account_id =
      profile_->GetPrefs()->GetString(prefs::kGoogleServicesUserAccountId);
    if (current_account_id.empty()) {
      // AccountIdFetcher will delete itself.
      new AccountIdFetcher(profile_, this);
    }
  }
}

void SigninAccountIdHelper::OnPrimaryAccountIdFetched(
    const std::string& account_id) {
  if (!account_id.empty()) {
    profile_->GetPrefs()->SetString(
        prefs::kGoogleServicesUserAccountId, account_id);
  }
}

