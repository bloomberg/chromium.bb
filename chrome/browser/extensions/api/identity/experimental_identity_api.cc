// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/experimental_identity_api.h"

#include <set>
#include <string>
#include <vector>

#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/api/experimental_identity.h"
#include "chrome/common/extensions/api/identity.h"
#include "chrome/common/extensions/api/identity/oauth2_manifest_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/common/page_transition_types.h"
#include "google_apis/gaia/gaia_constants.h"
#include "googleurl/src/gurl.h"
#include "ui/base/window_open_disposition.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

namespace extensions {

namespace {

static const char kChromiumDomainRedirectUrlPattern[] =
    "https://%s.chromiumapp.org/";

}  // namespace

namespace identity_exp = api::experimental_identity;

ExperimentalIdentityGetAuthTokenFunction::
    ExperimentalIdentityGetAuthTokenFunction()
    : should_prompt_for_scopes_(false), should_prompt_for_signin_(false) {}

ExperimentalIdentityGetAuthTokenFunction::
    ~ExperimentalIdentityGetAuthTokenFunction() {}

bool ExperimentalIdentityGetAuthTokenFunction::RunImpl() {
  if (profile()->IsOffTheRecord()) {
    error_ = identity_constants::kOffTheRecord;
    return false;
  }

  scoped_ptr<identity_exp::GetAuthToken::Params> params(
      identity_exp::GetAuthToken::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  bool interactive = params->details.get() &&
      params->details->interactive.get() &&
      *params->details->interactive;

  should_prompt_for_scopes_ = interactive;
  should_prompt_for_signin_ = interactive;

  const OAuth2Info& oauth2_info = OAuth2Info::GetOAuth2Info(GetExtension());

  // Check that the necessary information is present in the manifest.
  if (oauth2_info.client_id.empty()) {
    error_ = identity_constants::kInvalidClientId;
    return false;
  }

  if (oauth2_info.scopes.size() == 0) {
    error_ = identity_constants::kInvalidScopes;
    return false;
  }

  // Balanced in CompleteFunctionWithResult|CompleteFunctionWithError
  AddRef();

  if (!HasLoginToken()) {
    if (!should_prompt_for_signin_) {
      error_ = identity_constants::kUserNotSignedIn;
      Release();
      return false;
    }
    // Display a login prompt.
    StartSigninFlow();
  } else {
    TokenService* token_service = TokenServiceFactory::GetForProfile(profile());
    refresh_token_ = token_service->GetOAuth2LoginRefreshToken();
    StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE);
  }

  return true;
}

void ExperimentalIdentityGetAuthTokenFunction::CompleteFunctionWithResult(
    const std::string& access_token) {
  SetResult(Value::CreateStringValue(access_token));
  SendResponse(true);
  Release();  // Balanced in RunImpl.
}

void ExperimentalIdentityGetAuthTokenFunction::CompleteFunctionWithError(
    const std::string& error) {
  error_ = error;
  SendResponse(false);
  Release();  // Balanced in RunImpl.
}

void ExperimentalIdentityGetAuthTokenFunction::StartSigninFlow() {
  // Display a login prompt. If the subsequent mint fails, don't display the
  // login prompt again.
  should_prompt_for_signin_ = false;
  ShowLoginPopup();
}

void ExperimentalIdentityGetAuthTokenFunction::StartMintTokenFlow(
    IdentityMintRequestQueue::MintType type) {
  if (!should_prompt_for_scopes_) {
    // Caller requested no interaction.

    if (type == IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE) {
      // GAIA told us to do a consent UI.
      CompleteFunctionWithError(identity_constants::kNoGrant);
      return;
    }
  }

  if (type == IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE) {
    StartGaiaRequest(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
  } else {
    DCHECK(type == IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE);
    install_ui_.reset(new ExtensionInstallPrompt(GetAssociatedWebContents()));
    ShowOAuthApprovalDialog(issue_advice_);
  }
}

void ExperimentalIdentityGetAuthTokenFunction::OnMintTokenSuccess(
    const std::string& access_token, int time_to_live) {
  CompleteFunctionWithResult(access_token);
}

void ExperimentalIdentityGetAuthTokenFunction::OnMintTokenFailure(
    const GoogleServiceAuthError& error) {
  switch (error.state()) {
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::ACCOUNT_DELETED:
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
      extensions::IdentityAPI::GetFactoryInstance()->GetForProfile(
          profile())->ReportAuthError(error);
      if (should_prompt_for_signin_) {
        // Display a login prompt and try again (once).
        StartSigninFlow();
        return;
      }
      break;
    default:
      // Return error to caller.
      break;
  }

  CompleteFunctionWithError(
      std::string(identity_constants::kAuthFailure) + error.ToString());
}

void ExperimentalIdentityGetAuthTokenFunction::OnIssueAdviceSuccess(
    const IssueAdviceInfo& issue_advice) {
  should_prompt_for_signin_ = false;
  // Existing grant was revoked and we used NO_FORCE, so we got info back
  // instead. Start a consent UI if we can.
  issue_advice_ = issue_advice;
  StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE);
}

void ExperimentalIdentityGetAuthTokenFunction::SigninSuccess(
    const std::string& token) {
  refresh_token_ = token;
  StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE);
}

void ExperimentalIdentityGetAuthTokenFunction::SigninFailed() {
  CompleteFunctionWithError(identity_constants::kUserNotSignedIn);
}

void ExperimentalIdentityGetAuthTokenFunction::InstallUIProceed() {
  // The user has accepted the scopes, so we may now force (recording a grant
  // and receiving a token).
  StartGaiaRequest(OAuth2MintTokenFlow::MODE_MINT_TOKEN_FORCE);
}

void ExperimentalIdentityGetAuthTokenFunction::InstallUIAbort(
    bool user_initiated) {
  CompleteFunctionWithError(identity_constants::kUserRejected);
}

void ExperimentalIdentityGetAuthTokenFunction::StartGaiaRequest(
    OAuth2MintTokenFlow::Mode mode) {
  mint_token_flow_.reset(CreateMintTokenFlow(mode));
  mint_token_flow_->Start();
}

void ExperimentalIdentityGetAuthTokenFunction::ShowLoginPopup() {
  signin_flow_.reset(new IdentitySigninFlow(this, profile()));
  signin_flow_->Start();
}

void ExperimentalIdentityGetAuthTokenFunction::ShowOAuthApprovalDialog(
    const IssueAdviceInfo& issue_advice) {
  install_ui_->ConfirmIssueAdvice(this, GetExtension(), issue_advice);
}

OAuth2MintTokenFlow*
ExperimentalIdentityGetAuthTokenFunction::CreateMintTokenFlow(
    OAuth2MintTokenFlow::Mode mode) {
  const OAuth2Info& oauth2_info = OAuth2Info::GetOAuth2Info(GetExtension());
  OAuth2MintTokenFlow* mint_token_flow =
      new OAuth2MintTokenFlow(
          profile()->GetRequestContext(),
          this,
          OAuth2MintTokenFlow::Parameters(
              refresh_token_,
              GetExtension()->id(),
              oauth2_info.client_id,
              oauth2_info.scopes,
              mode));
#if defined(OS_CHROMEOS)
  if (chrome::IsRunningInForcedAppMode()) {
    std::string chrome_client_id;
    std::string chrome_client_secret;
    if (chromeos::UserManager::Get()->GetAppModeChromeClientOAuthInfo(
           &chrome_client_id, &chrome_client_secret)) {
      mint_token_flow->SetChromeOAuthClientInfo(chrome_client_id,
                                                chrome_client_secret);
    }
  }
#endif
  return mint_token_flow;
}

bool ExperimentalIdentityGetAuthTokenFunction::HasLoginToken() const {
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile());
  return token_service->HasOAuthLoginToken();
}

ExperimentalIdentityLaunchWebAuthFlowFunction::
    ExperimentalIdentityLaunchWebAuthFlowFunction() {}

ExperimentalIdentityLaunchWebAuthFlowFunction::
    ~ExperimentalIdentityLaunchWebAuthFlowFunction() {}

bool ExperimentalIdentityLaunchWebAuthFlowFunction::RunImpl() {
  if (profile()->IsOffTheRecord()) {
    error_ = identity_constants::kOffTheRecord;
    return false;
  }

  scoped_ptr<identity_exp::LaunchWebAuthFlow::Params> params(
      identity_exp::LaunchWebAuthFlow::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  const identity_exp::ExperimentalWebAuthFlowDetails& details = params->details;

  GURL auth_url(params->details.url);
  WebAuthFlow::Mode mode =
      params->details.interactive && *params->details.interactive ?
      WebAuthFlow::INTERACTIVE : WebAuthFlow::SILENT;

  // Set up acceptable target URLs. (Includes chrome-extension scheme
  // for this version of the API.)
  InitFinalRedirectURLPrefixes(GetExtension()->id());

  // The bounds attributes are optional, but using 0 when they're not available
  // does the right thing.
  gfx::Rect initial_bounds;
  if (details.width)
    initial_bounds.set_width(*details.width);
  if (details.height)
    initial_bounds.set_height(*details.height);
  if (details.left)
    initial_bounds.set_x(*details.left);
  if (details.top)
    initial_bounds.set_y(*details.top);

  AddRef();  // Balanced in OnAuthFlowSuccess/Failure.

  Browser* current_browser = this->GetCurrentBrowser();
  chrome::HostDesktopType host_desktop_type = current_browser ?
      current_browser->host_desktop_type() : chrome::GetActiveDesktop();
  auth_flow_.reset(new WebAuthFlow(
      this, profile(), auth_url, mode, initial_bounds,
      host_desktop_type));
  auth_flow_->Start();
  return true;
}

bool ExperimentalIdentityLaunchWebAuthFlowFunction::IsFinalRedirectURL(
    const GURL& url) const {
  std::vector<GURL>::const_iterator iter;
  for (iter = final_prefixes_.begin(); iter != final_prefixes_.end(); ++iter) {
    if (url.GetWithEmptyPath() == *iter) {
      return true;
    }
  }
  return false;
}

void ExperimentalIdentityLaunchWebAuthFlowFunction::
    InitFinalRedirectURLPrefixes(const std::string& extension_id) {
  final_prefixes_.push_back(Extension::GetBaseURLFromExtensionId(extension_id));
  final_prefixes_.push_back(GURL(base::StringPrintf(
      kChromiumDomainRedirectUrlPattern, extension_id.c_str())));
}

void ExperimentalIdentityLaunchWebAuthFlowFunction::OnAuthFlowFailure(
    WebAuthFlow::Failure failure) {
  switch (failure) {
    case WebAuthFlow::WINDOW_CLOSED:
      error_ = identity_constants::kUserRejected;
      break;
    case WebAuthFlow::INTERACTION_REQUIRED:
      error_ = identity_constants::kInteractionRequired;
      break;
    default:
      NOTREACHED() << "Unexpected error from web auth flow: " << failure;
      error_ = identity_constants::kInvalidRedirect;
      break;
  }
  SendResponse(false);
  Release();  // Balanced in RunImpl.
}

void ExperimentalIdentityLaunchWebAuthFlowFunction::OnAuthFlowURLChange(
    const GURL& redirect_url) {
  if (IsFinalRedirectURL(redirect_url)) {
    SetResult(Value::CreateStringValue(redirect_url.spec()));
    SendResponse(true);
    Release();  // Balanced in RunImpl.
  }
}

void ExperimentalIdentityLaunchWebAuthFlowFunction::
    InitFinalRedirectURLPrefixesForTest(const std::string& extension_id) {
  final_prefixes_.clear();
  InitFinalRedirectURLPrefixes(extension_id);
}

}  // namespace extensions
