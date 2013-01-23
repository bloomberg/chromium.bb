// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/policy_oauth_fetcher.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/user_cloud_policy_manager_chromeos.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace chromeos {

namespace {
const char kServiceScopeChromeOSDeviceManagement[] =
    "https://www.googleapis.com/auth/chromeosdevicemanagement";
}  // namespace

PolicyOAuthFetcher::PolicyOAuthFetcher(
    net::URLRequestContextGetter* context_getter,
    const std::string& oauth1_token,
    const std::string& oauth1_secret)
    : oauth_fetcher_(this,
                     context_getter,
                     kServiceScopeChromeOSDeviceManagement),
      oauth1_token_(oauth1_token),
      oauth1_secret_(oauth1_secret) {
}

PolicyOAuthFetcher::PolicyOAuthFetcher(
    net::URLRequestContextGetter* context_getter)
    : oauth_fetcher_(this,
                     context_getter,
                     kServiceScopeChromeOSDeviceManagement) {
}

PolicyOAuthFetcher::~PolicyOAuthFetcher() {
}

void PolicyOAuthFetcher::Start() {
  oauth_fetcher_.SetAutoFetchLimit(
      GaiaOAuthFetcher::OAUTH2_SERVICE_ACCESS_TOKEN);

  if (oauth1_token_.empty()) {
    oauth_fetcher_.StartGetOAuthTokenRequest();
  } else {
    oauth_fetcher_.StartOAuthWrapBridge(
        oauth1_token_, oauth1_secret_, GaiaConstants::kGaiaOAuthDuration,
        std::string(kServiceScopeChromeOSDeviceManagement));
  }
}

void PolicyOAuthFetcher::OnGetOAuthTokenSuccess(
    const std::string& oauth_token) {
  VLOG(1) << "Got OAuth request token";
}

void PolicyOAuthFetcher::OnGetOAuthTokenFailure(
    const GoogleServiceAuthError& error) {
  LOG(WARNING) << "Failed to get OAuth request token, error: " << error.state();
  SetPolicyToken("");
}

void PolicyOAuthFetcher::OnOAuthGetAccessTokenSuccess(
    const std::string& token,
    const std::string& secret) {
  VLOG(1) << "Got OAuth access token";
  oauth1_token_ = token;
  oauth1_secret_ = secret;
}

void PolicyOAuthFetcher::OnOAuthGetAccessTokenFailure(
      const GoogleServiceAuthError& error) {
  LOG(WARNING) << "Failed to get OAuth access token, error: " << error.state();
  SetPolicyToken("");
}

void PolicyOAuthFetcher::OnOAuthWrapBridgeSuccess(
    const std::string& service_name,
    const std::string& token,
    const std::string& expires_in) {
  VLOG(1) << "Got OAuth access token for " << service_name;
  SetPolicyToken(token);
}

void PolicyOAuthFetcher::OnOAuthWrapBridgeFailure(
    const std::string& service_name,
    const GoogleServiceAuthError& error) {
  LOG(WARNING) << "Failed to get OAuth access token for " << service_name
               << ", error: " << error.state();
  SetPolicyToken("");
}

void PolicyOAuthFetcher::SetPolicyToken(const std::string& token) {
  policy_token_ = token;

  policy::BrowserPolicyConnector* browser_policy_connector =
      g_browser_process->browser_policy_connector();
  policy::UserCloudPolicyManagerChromeOS* cloud_policy_manager =
      browser_policy_connector->GetUserCloudPolicyManager();
  if (cloud_policy_manager) {
    if (token.empty())
      cloud_policy_manager->CancelWaitForPolicyFetch();
    else
      cloud_policy_manager->RegisterClient(token);
  }
}

}  // namespace chromeos
