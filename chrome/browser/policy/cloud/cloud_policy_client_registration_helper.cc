// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/cloud_policy_client_registration_helper.h"

#include <vector>

#include "base/logging.h"
#include "base/time/time.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"

namespace policy {

namespace {

// OAuth2 scope for the userinfo service.
const char kServiceScopeGetUserInfo[] =
    "https://www.googleapis.com/auth/userinfo.email";

// The key under which the hosted-domain value is stored in the UserInfo
// response.
const char kGetHostedDomainKey[] = "hd";

}  // namespace

CloudPolicyClientRegistrationHelper::CloudPolicyClientRegistrationHelper(
    net::URLRequestContextGetter* context,
    CloudPolicyClient* client,
    bool should_force_load_policy,
    enterprise_management::DeviceRegisterRequest::Type registration_type)
    : context_(context),
      client_(client),
      should_force_load_policy_(should_force_load_policy),
      registration_type_(registration_type) {
  DCHECK(context_);
  DCHECK(client_);
}

CloudPolicyClientRegistrationHelper::~CloudPolicyClientRegistrationHelper() {
  // Clean up any pending observers in case the browser is shutdown while
  // trying to register for policy.
  if (client_)
    client_->RemoveObserver(this);
}

void CloudPolicyClientRegistrationHelper::StartRegistrationWithLoginToken(
    const std::string& login_refresh_token,
    const base::Closure& callback) {
  DVLOG(1) << "Starting registration process with login token";
  DCHECK(!client_->is_registered());
  callback_ = callback;
  client_->AddObserver(this);

  // Start fetching an OAuth2 access token for the device management and
  // userinfo services.
  oauth2_access_token_fetcher_.reset(
      new OAuth2AccessTokenFetcher(this, context_));
  std::vector<std::string> scopes;
  scopes.push_back(GaiaConstants::kDeviceManagementServiceOAuth);
  scopes.push_back(kServiceScopeGetUserInfo);
  GaiaUrls* gaia_urls = GaiaUrls::GetInstance();
  oauth2_access_token_fetcher_->Start(
      gaia_urls->oauth2_chrome_client_id(),
      gaia_urls->oauth2_chrome_client_secret(),
      login_refresh_token,
      scopes);
}

void CloudPolicyClientRegistrationHelper::StartRegistrationWithServicesToken(
    const std::string& services_token,
    const base::Closure& callback) {
  DVLOG(1) << "Starting registration process with services token";
  DCHECK(!client_->is_registered());
  callback_ = callback;
  client_->AddObserver(this);
  OnGetTokenSuccess(services_token, base::Time());
}

void CloudPolicyClientRegistrationHelper::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {
  DLOG(WARNING) << "Could not fetch access token for "
                << GaiaConstants::kDeviceManagementServiceOAuth;
  oauth2_access_token_fetcher_.reset();

  // Invoke the callback to let them know the fetch failed.
  RequestCompleted();
}

void CloudPolicyClientRegistrationHelper::OnGetTokenSuccess(
    const std::string& access_token,
    const base::Time& expiration_time) {
  // Cache the access token to be used after the GetUserInfo call.
  oauth_access_token_ = access_token;
  DVLOG(1) << "Fetched new scoped OAuth token:" << oauth_access_token_;
  oauth2_access_token_fetcher_.reset();
  // Now we've gotten our access token - contact GAIA to see if this is a
  // hosted domain.
  user_info_fetcher_.reset(new UserInfoFetcher(this, context_));
  user_info_fetcher_->Start(oauth_access_token_);
}

void CloudPolicyClientRegistrationHelper::OnGetUserInfoFailure(
    const GoogleServiceAuthError& error) {
  DVLOG(1) << "Failed to fetch user info from GAIA: " << error.state();
  user_info_fetcher_.reset();
  RequestCompleted();
}

void CloudPolicyClientRegistrationHelper::OnGetUserInfoSuccess(
    const base::DictionaryValue* data) {
  user_info_fetcher_.reset();
  if (!data->HasKey(kGetHostedDomainKey) && !should_force_load_policy_) {
    DVLOG(1) << "User not from a hosted domain - skipping registration";
    RequestCompleted();
    return;
  }
  DVLOG(1) << "Registering CloudPolicyClient for user from hosted domain";
  // The user is from a hosted domain, so it's OK to register the
  // CloudPolicyClient and make requests to DMServer.
  if (client_->is_registered()) {
    // Client should not be registered yet.
    NOTREACHED();
    RequestCompleted();
    return;
  }

  // Kick off registration of the CloudPolicyClient with our newly minted
  // oauth_access_token_.
  client_->Register(registration_type_, oauth_access_token_,
                    std::string(), false, std::string());
}

void CloudPolicyClientRegistrationHelper::OnPolicyFetched(
    CloudPolicyClient* client) {
  // Ignored.
}

void CloudPolicyClientRegistrationHelper::OnRegistrationStateChanged(
    CloudPolicyClient* client) {
  DVLOG(1) << "Client registration succeeded";
  DCHECK_EQ(client, client_);
  DCHECK(client->is_registered());
  RequestCompleted();
}

void CloudPolicyClientRegistrationHelper::OnClientError(
    CloudPolicyClient* client) {
  DVLOG(1) << "Client registration failed";
  DCHECK_EQ(client, client_);
  RequestCompleted();
}

void CloudPolicyClientRegistrationHelper::RequestCompleted() {
  if (client_) {
    client_->RemoveObserver(this);
    // |client_| may be freed by the callback so clear it now.
    client_ = NULL;
    callback_.Run();
  }
}

}  // namespace policy
