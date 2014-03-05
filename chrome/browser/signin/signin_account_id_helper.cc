// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_account_id_helper.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/common/pref_names.h"
#include "google_apis/gaia/gaia_oauth_client.h"

// TODO(guohui): this class should be moved to a more generic place for reuse.
class SigninAccountIdHelper::GaiaIdFetcher
    : public OAuth2TokenService::Consumer,
      public gaia::GaiaOAuthClient::Delegate {
 public:
  GaiaIdFetcher(SigninManagerBase* signin_manager,
                SigninAccountIdHelper* signin_account_id_helper);
  virtual ~GaiaIdFetcher();

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

  SigninManagerBase* signin_manager_;
  SigninAccountIdHelper* signin_account_id_helper_;

  scoped_ptr<OAuth2TokenService::Request> login_token_request_;
  scoped_ptr<gaia::GaiaOAuthClient> gaia_oauth_client_;

  DISALLOW_COPY_AND_ASSIGN(GaiaIdFetcher);
};

SigninAccountIdHelper::GaiaIdFetcher::GaiaIdFetcher(
    SigninManagerBase* signin_manager,
    SigninAccountIdHelper* signin_account_id_helper)
    : OAuth2TokenService::Consumer("gaia_id_fetcher"),
      signin_manager_(signin_manager),
      signin_account_id_helper_(signin_account_id_helper) {
  Start();
}

SigninAccountIdHelper::GaiaIdFetcher::~GaiaIdFetcher() {}

void SigninAccountIdHelper::GaiaIdFetcher::Start() {
  ProfileOAuth2TokenService* service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(
          signin_manager_->profile());
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert("https://www.googleapis.com/auth/userinfo.profile");
  login_token_request_ = service->StartRequest(
      signin_manager_->GetAuthenticatedAccountId(), scopes, this);
}

void SigninAccountIdHelper::GaiaIdFetcher::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK_EQ(request, login_token_request_.get());

  gaia_oauth_client_.reset(
      new gaia::GaiaOAuthClient(
          signin_manager_->profile()->GetRequestContext()));

  const int kMaxGetUserIdRetries = 3;
  gaia_oauth_client_->GetUserId(access_token, kMaxGetUserIdRetries, this);
}

void SigninAccountIdHelper::GaiaIdFetcher::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  VLOG(1) << "OnGetTokenFailure: " << error.error_message();
  DCHECK_EQ(request, login_token_request_.get());
  signin_account_id_helper_->OnPrimaryAccountIdFetched("");
}

void SigninAccountIdHelper::GaiaIdFetcher::OnGetUserIdResponse(
    const std::string& gaia_id) {
  signin_account_id_helper_->OnPrimaryAccountIdFetched(gaia_id);
}

void SigninAccountIdHelper::GaiaIdFetcher::OnOAuthError() {
  VLOG(1) << "OnOAuthError";
}

void SigninAccountIdHelper::GaiaIdFetcher::OnNetworkError(
    int response_code) {
  VLOG(1) << "OnNetworkError " << response_code;
}

SigninAccountIdHelper::SigninAccountIdHelper(SigninManagerBase* signin_manager)
    : signin_manager_(signin_manager) {
  DCHECK(signin_manager_);
  signin_manager_->AddObserver(this);
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(
          signin_manager_->profile());
  std::string primary_email = signin_manager_->GetAuthenticatedAccountId();
  if (!primary_email.empty() &&
      token_service->RefreshTokenIsAvailable(primary_email) &&
      !disable_for_test_) {
    id_fetcher_.reset(new GaiaIdFetcher(signin_manager_, this));
  }
  token_service->AddObserver(this);
}

SigninAccountIdHelper::~SigninAccountIdHelper() {
  signin_manager_->RemoveObserver(this);
  ProfileOAuth2TokenServiceFactory::GetForProfile(signin_manager_->profile())->
      RemoveObserver(this);
}

void SigninAccountIdHelper::GoogleSignedOut(const std::string& username) {
  signin_manager_->profile()->GetPrefs()->ClearPref(
      prefs::kGoogleServicesUserAccountId);
}

void SigninAccountIdHelper::OnRefreshTokenAvailable(
    const std::string& account_id) {
  if (account_id == signin_manager_->GetAuthenticatedAccountId()) {
    std::string current_gaia_id =
      signin_manager_->profile()->GetPrefs()->
          GetString(prefs::kGoogleServicesUserAccountId);
    if (current_gaia_id.empty() && !disable_for_test_) {
      id_fetcher_.reset(new GaiaIdFetcher(signin_manager_, this));
    }
  }
}

void SigninAccountIdHelper::OnPrimaryAccountIdFetched(
    const std::string& gaia_id) {
  if (!gaia_id.empty()) {
    signin_manager_->profile()->GetPrefs()->SetString(
        prefs::kGoogleServicesUserAccountId, gaia_id);
  }
}

// static
bool SigninAccountIdHelper::disable_for_test_ = false;

// static
void SigninAccountIdHelper::SetDisableForTest(bool disable_for_test) {
  disable_for_test_ = disable_for_test;
}

