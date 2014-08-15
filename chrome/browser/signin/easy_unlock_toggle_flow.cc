// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/easy_unlock_toggle_flow.h"

#include <vector>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/extensions/api/identity/oauth2_manifest_handler.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "extensions/browser/extension_system.h"
#include "google_apis/gaia/oauth2_api_call_flow.h"
#include "net/url_request/url_fetcher.h"

namespace {

const char kEasyUnlockToggleUrl[] =
    "https://www.googleapis.com/cryptauth/v1/deviceSync/toggleeasyunlock";

std::vector<std::string> GetScopes() {
  std::vector<std::string> scopes;
  scopes.push_back("https://www.googleapis.com/auth/proximity_auth");
  scopes.push_back("https://www.googleapis.com/auth/cryptauth");
  return scopes;
}

std::string GetEasyUnlockAppClientId(Profile * profile) {
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile);
  ExtensionService* extension_service = extension_system->extension_service();
  const extensions::Extension* easy_unlock_app =
      extension_service->GetInstalledExtension(
          extension_misc::kEasyUnlockAppId);
  if (!easy_unlock_app)
    return std::string();

  const extensions::OAuth2Info& oauth2_info =
      extensions::OAuth2Info::GetOAuth2Info(easy_unlock_app);
  return oauth2_info.client_id;
}

}  // namespace

class EasyUnlockToggleFlow::ToggleApiCall : public OAuth2ApiCallFlow {
 public:
  ToggleApiCall(EasyUnlockToggleFlow* flow,
                net::URLRequestContextGetter* context,
                const std::string& access_token,
                const std::string& phone_public_key,
                bool toggle_enable);
  virtual ~ToggleApiCall();

  // OAuth2ApiCallFlow
  virtual GURL CreateApiCallUrl() OVERRIDE;
  virtual std::string CreateApiCallBody() OVERRIDE;
  virtual std::string CreateApiCallBodyContentType() OVERRIDE;
  virtual void ProcessApiCallSuccess(const net::URLFetcher* source) OVERRIDE;
  virtual void ProcessApiCallFailure(const net::URLFetcher* source) OVERRIDE;
  virtual void ProcessNewAccessToken(const std::string& access_token) OVERRIDE;
  virtual void ProcessMintAccessTokenFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

 private:
  EasyUnlockToggleFlow* flow_;
  const std::string phone_public_key_;
  const bool toggle_enable_;

  DISALLOW_COPY_AND_ASSIGN(ToggleApiCall);
};

EasyUnlockToggleFlow::ToggleApiCall::ToggleApiCall(
    EasyUnlockToggleFlow* flow,
    net::URLRequestContextGetter* context,
    const std::string& access_token,
    const std::string& phone_public_key,
    bool toggle_enable)
    : OAuth2ApiCallFlow(context,
                        std::string(),
                        access_token,
                        GetScopes()),
      flow_(flow),
      phone_public_key_(phone_public_key),
      toggle_enable_(toggle_enable) {
}

EasyUnlockToggleFlow::ToggleApiCall::~ToggleApiCall() {
}

GURL EasyUnlockToggleFlow::ToggleApiCall::CreateApiCallUrl() {
  return GURL(kEasyUnlockToggleUrl);
}

std::string EasyUnlockToggleFlow::ToggleApiCall::CreateApiCallBody() {
  const char kBodyFormat[] = "{\"enable\":%s,\"publicKey\":\"%s\"}";
  return base::StringPrintf(
      kBodyFormat,
      toggle_enable_ ? "true" : "false",
      phone_public_key_.c_str());
}

std::string
EasyUnlockToggleFlow::ToggleApiCall::CreateApiCallBodyContentType() {
  return "application/json";
}

void EasyUnlockToggleFlow::ToggleApiCall::ProcessApiCallSuccess(
    const net::URLFetcher* source) {
  flow_->ReportToggleApiCallResult(true);
}

void EasyUnlockToggleFlow::ToggleApiCall::ProcessApiCallFailure(
    const net::URLFetcher* source) {
  flow_->ReportToggleApiCallResult(false);
}

void EasyUnlockToggleFlow::ToggleApiCall::ProcessNewAccessToken(
    const std::string& access_token) {
  NOTREACHED();
}

void EasyUnlockToggleFlow::ToggleApiCall::ProcessMintAccessTokenFailure(
    const GoogleServiceAuthError& error) {
  NOTREACHED();
}

EasyUnlockToggleFlow::EasyUnlockToggleFlow(Profile* profile,
                                           const std::string& phone_public_key,
                                           bool toggle_enable,
                                           const ToggleFlowCallback& callback)
    : OAuth2TokenService::Consumer("easy_unlock_toggle"),
      profile_(profile),
      phone_public_key_(phone_public_key),
      toggle_enable_(toggle_enable),
      callback_(callback) {
}

EasyUnlockToggleFlow::~EasyUnlockToggleFlow() {
}

void EasyUnlockToggleFlow::Start() {
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile_);
  token_request_ =
      token_service->StartRequest(signin_manager->GetAuthenticatedAccountId(),
                                  OAuth2TokenService::ScopeSet(),
                                  this);
}

void EasyUnlockToggleFlow::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK_EQ(token_request_.get(), request);
  token_request_.reset();

  mint_token_flow_.reset(
      new OAuth2MintTokenFlow(profile_->GetRequestContext(),
                              this,
                              OAuth2MintTokenFlow::Parameters(
                                  access_token,
                                  extension_misc::kEasyUnlockAppId,
                                  GetEasyUnlockAppClientId(profile_),
                                  GetScopes(),
                                  OAuth2MintTokenFlow::MODE_MINT_TOKEN_FORCE)));
  mint_token_flow_->Start();
}

void EasyUnlockToggleFlow::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK_EQ(token_request_.get(), request);
  token_request_.reset();

  LOG(ERROR) << "Easy unlock toggle flow, failed to get access token,"
             << "error=" << error.state();
  callback_.Run(false);
}

void EasyUnlockToggleFlow::OnMintTokenSuccess(const std::string& access_token,
                                              int time_to_live) {
  toggle_api_call_.reset(new ToggleApiCall(this,
                                           profile_->GetRequestContext(),
                                           access_token,
                                           phone_public_key_,
                                           toggle_enable_));
  toggle_api_call_->Start();
}

void EasyUnlockToggleFlow::OnMintTokenFailure(
    const GoogleServiceAuthError& error) {
  LOG(ERROR) << "Easy unlock toggle flow, failed to mint access token,"
             << "error=" << error.state();
  callback_.Run(false);
}

void EasyUnlockToggleFlow::OnIssueAdviceSuccess(
    const IssueAdviceInfo& issue_advice) {
  NOTREACHED();
}

void EasyUnlockToggleFlow::ReportToggleApiCallResult(bool success) {
  callback_.Run(success);
}
