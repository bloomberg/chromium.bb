// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_api.h"

#include "base/values.h"
#include "chrome/common/extensions/api/experimental_identity.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "googleurl/src/gurl.h"

namespace extensions {

namespace {

const char kInvalidClientId[] = "Invalid OAuth2 Client ID.";
const char kInvalidScopes[] = "Invalid OAuth2 scopes.";
const char kInvalidRedirect[] = "Did not redirect to the right URL.";
const char kAuthFailure[] = "OAuth2 request failed: ";
const char kNoGrant[] = "OAuth2 not granted or revoked.";
const char kUserRejected[] = "The user did not approve access.";

}  // namespace

namespace GetAuthToken = extensions::api::experimental_identity::GetAuthToken;
namespace LaunchWebAuthFlow =
    extensions::api::experimental_identity::LaunchWebAuthFlow;

IdentityGetAuthTokenFunction::IdentityGetAuthTokenFunction()
    : interactive_(false) {}
IdentityGetAuthTokenFunction::~IdentityGetAuthTokenFunction() {}

bool IdentityGetAuthTokenFunction::RunImpl() {
  scoped_ptr<GetAuthToken::Params> params(GetAuthToken::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  if (params->details.get() && params->details->interactive.get())
    interactive_ = *params->details->interactive;

  // Balanced in OnIssueAdviceSuccess|OnMintTokenSuccess|OnMintTokenFailure|
  // InstallUIAbort.
  AddRef();

  if (StartFlow(ExtensionInstallPrompt::ShouldAutomaticallyApproveScopes() ?
                    OAuth2MintTokenFlow::MODE_MINT_TOKEN_FORCE :
                    OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE)) {
    return true;
  } else {
    Release();
    return false;
  }
}

void IdentityGetAuthTokenFunction::OnMintTokenSuccess(
    const std::string& access_token) {
  SetResult(Value::CreateStringValue(access_token));
  SendResponse(true);
  Release();  // Balanced in RunImpl.
}

void IdentityGetAuthTokenFunction::OnMintTokenFailure(
    const GoogleServiceAuthError& error) {
  error_ = std::string(kAuthFailure) + error.ToString();
  SendResponse(false);
  Release();  // Balanced in RunImpl.
}

void IdentityGetAuthTokenFunction::OnIssueAdviceSuccess(
    const IssueAdviceInfo& issue_advice) {
  // Existing grant was revoked and we used NO_FORCE, so we got info back
  // instead.
  if (interactive_) {
    install_ui_.reset(
        chrome::CreateExtensionInstallPromptWithBrowser(GetCurrentBrowser()));
    install_ui_->ConfirmIssueAdvice(this, GetExtension(), issue_advice);
  } else {
    error_ = kNoGrant;
    SendResponse(false);
    Release();  // Balanced in RunImpl.
  }
}

void IdentityGetAuthTokenFunction::InstallUIProceed() {
  DCHECK(install_ui_->record_oauth2_grant());
  // The user has accepted the scopes, so we may now force (recording a grant
  // and receiving a token).
  bool success = StartFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_FORCE);
  DCHECK(success);
}

void IdentityGetAuthTokenFunction::InstallUIAbort(bool user_initiated) {
  error_ = kUserRejected;
  SendResponse(false);
  Release();  // Balanced in RunImpl.
}

bool IdentityGetAuthTokenFunction::StartFlow(OAuth2MintTokenFlow::Mode mode) {
  const Extension* extension = GetExtension();
  Extension::OAuth2Info oauth2_info = extension->oauth2_info();

  if (oauth2_info.client_id.empty()) {
    error_ = kInvalidClientId;
    return false;
  }

  if (oauth2_info.scopes.size() == 0) {
    error_ = kInvalidScopes;
    return false;
  }

  TokenService* token_service = TokenServiceFactory::GetForProfile(profile());
  flow_.reset(new OAuth2MintTokenFlow(
      profile()->GetRequestContext(),
      this,
      OAuth2MintTokenFlow::Parameters(
          token_service->GetOAuth2LoginRefreshToken(),
          extension->id(),
          oauth2_info.client_id,
          oauth2_info.scopes,
          mode)));
  flow_->Start();
  return true;
}

IdentityLaunchWebAuthFlowFunction::IdentityLaunchWebAuthFlowFunction() {}
IdentityLaunchWebAuthFlowFunction::~IdentityLaunchWebAuthFlowFunction() {}

bool IdentityLaunchWebAuthFlowFunction::RunImpl() {
  scoped_ptr<LaunchWebAuthFlow::Params> params(
      LaunchWebAuthFlow::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL auth_url(params->details.url);
  WebAuthFlow::Mode mode =
      params->details.interactive.get() && *params->details.interactive ?
      WebAuthFlow::INTERACTIVE : WebAuthFlow::SILENT;

  AddRef();  // Balanced in OnAuthFlowSuccess/Failure.
  auth_flow_.reset(new WebAuthFlow(
      this, profile(), GetExtension()->id(), auth_url, mode));
  auth_flow_->Start();
  return true;
}

void IdentityLaunchWebAuthFlowFunction::OnAuthFlowSuccess(
    const std::string& redirect_url) {
  SetResult(Value::CreateStringValue(redirect_url));
  SendResponse(true);
  Release();  // Balanced in RunImpl.
}

void IdentityLaunchWebAuthFlowFunction::OnAuthFlowFailure() {
  error_ = kInvalidRedirect;
  SendResponse(false);
  Release();  // Balanced in RunImpl.
}

}  // namespace extensions
