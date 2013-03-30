// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_api.h"

#include "base/lazy_instance.h"
#include "base/values.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
#include "chrome/common/extensions/api/experimental_identity.h"
#include "chrome/common/extensions/api/identity/oauth2_manifest_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/common/page_transition_types.h"
#include "googleurl/src/gurl.h"
#include "ui/base/window_open_disposition.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

namespace extensions {

namespace identity_constants {
const char kInvalidClientId[] = "Invalid OAuth2 Client ID.";
const char kInvalidScopes[] = "Invalid OAuth2 scopes.";
const char kAuthFailure[] = "OAuth2 request failed: ";
const char kNoGrant[] = "OAuth2 not granted or revoked.";
const char kUserRejected[] = "The user did not approve access.";
const char kUserNotSignedIn[] = "The user is not signed in.";
const char kInvalidRedirect[] = "Did not redirect to the right URL.";
}  // namespace identity_constants

namespace GetAuthToken = api::experimental_identity::GetAuthToken;
namespace LaunchWebAuthFlow = api::experimental_identity::LaunchWebAuthFlow;
namespace identity = api::experimental_identity;

IdentityGetAuthTokenFunction::IdentityGetAuthTokenFunction()
    : interactive_(false) {}
IdentityGetAuthTokenFunction::~IdentityGetAuthTokenFunction() {}

bool IdentityGetAuthTokenFunction::RunImpl() {
  scoped_ptr<GetAuthToken::Params> params(GetAuthToken::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  if (params->details.get() && params->details->interactive.get())
    interactive_ = *params->details->interactive;

  const OAuth2Info& oauth2_info = OAuth2Info::GetOAuth2Info(GetExtension());

  // Check that the necessary information is present in the manfist.
  if (oauth2_info.client_id.empty()) {
    error_ = identity_constants::kInvalidClientId;
    return false;
  }

  if (oauth2_info.scopes.size() == 0) {
    error_ = identity_constants::kInvalidScopes;
    return false;
  }

  // Balanced in OnIssueAdviceSuccess|OnMintTokenSuccess|OnMintTokenFailure|
  // InstallUIAbort|OnLoginUIClosed.
  AddRef();

  if (!HasLoginToken()) {
    if (StartLogin()) {
      return true;
    } else {
      Release();
      return false;
    }
  }

  if (StartFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE)) {
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
  error_ = std::string(identity_constants::kAuthFailure) + error.ToString();
  SendResponse(false);
  Release();  // Balanced in RunImpl.
}

void IdentityGetAuthTokenFunction::OnIssueAdviceSuccess(
    const IssueAdviceInfo& issue_advice) {
  // Existing grant was revoked and we used NO_FORCE, so we got info back
  // instead.
  if (interactive_) {
    install_ui_.reset(new ExtensionInstallPrompt(GetAssociatedWebContents()));
    ShowOAuthApprovalDialog(issue_advice);
  } else {
    error_ = identity_constants::kNoGrant;
    SendResponse(false);
    Release();  // Balanced in RunImpl.
  }
}

void IdentityGetAuthTokenFunction::OnLoginUIClosed(
    LoginUIService::LoginUI* ui) {
  StopObservingLoginService();
  if (!StartFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE)) {
    SendResponse(false);
    Release();
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
  error_ = identity_constants::kUserRejected;
  SendResponse(false);
  Release();  // Balanced in RunImpl.
}

bool IdentityGetAuthTokenFunction::StartFlow(OAuth2MintTokenFlow::Mode mode) {
  if (!HasLoginToken()) {
    error_ = identity_constants::kUserNotSignedIn;
    return false;
  }

  flow_.reset(CreateMintTokenFlow(mode));
  flow_->Start();
  return true;
}

bool IdentityGetAuthTokenFunction::StartLogin() {
  if (!interactive_) {
    error_ = identity_constants::kUserNotSignedIn;
    return false;
  }

  ShowLoginPopup();
  return true;
}

void IdentityGetAuthTokenFunction::StartObservingLoginService() {
  LoginUIService* login_ui_service =
      LoginUIServiceFactory::GetForProfile(profile());
  login_ui_service->AddObserver(this);
}

void IdentityGetAuthTokenFunction::StopObservingLoginService() {
  LoginUIService* login_ui_service =
      LoginUIServiceFactory::GetForProfile(profile());
  login_ui_service->RemoveObserver(this);
}

void IdentityGetAuthTokenFunction::ShowLoginPopup() {
  StartObservingLoginService();

  LoginUIService* login_ui_service =
      LoginUIServiceFactory::GetForProfile(profile());
  login_ui_service->ShowLoginPopup();
}

void IdentityGetAuthTokenFunction::ShowOAuthApprovalDialog(
    const IssueAdviceInfo& issue_advice) {
  install_ui_->ConfirmIssueAdvice(this, GetExtension(), issue_advice);
}

OAuth2MintTokenFlow* IdentityGetAuthTokenFunction::CreateMintTokenFlow(
    OAuth2MintTokenFlow::Mode mode) {
  const OAuth2Info& oauth2_info = OAuth2Info::GetOAuth2Info(GetExtension());
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile());
  OAuth2MintTokenFlow* mint_token_flow =
      new OAuth2MintTokenFlow(
          profile()->GetRequestContext(),
          this,
          OAuth2MintTokenFlow::Parameters(
              token_service->GetOAuth2LoginRefreshToken(),
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

bool IdentityGetAuthTokenFunction::HasLoginToken() const {
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile());
  return token_service->HasOAuthLoginToken();
}

IdentityLaunchWebAuthFlowFunction::IdentityLaunchWebAuthFlowFunction() {}
IdentityLaunchWebAuthFlowFunction::~IdentityLaunchWebAuthFlowFunction() {}

bool IdentityLaunchWebAuthFlowFunction::RunImpl() {
  scoped_ptr<LaunchWebAuthFlow::Params> params(
      LaunchWebAuthFlow::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  const identity::WebAuthFlowDetails& details = params->details;

  GURL auth_url(details.url);
  WebAuthFlow::Mode mode =
      details.interactive && *details.interactive ?
      WebAuthFlow::INTERACTIVE : WebAuthFlow::SILENT;

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
      this, profile(), GetExtension()->id(), auth_url, mode, initial_bounds,
      host_desktop_type));
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
  error_ = identity_constants::kInvalidRedirect;
  SendResponse(false);
  Release();  // Balanced in RunImpl.
}

IdentityAPI::IdentityAPI(Profile* profile) {
  (new OAuth2ManifestHandler)->Register();
}

IdentityAPI::~IdentityAPI() {
}

static base::LazyInstance<ProfileKeyedAPIFactory<IdentityAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<IdentityAPI>* IdentityAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

}  // namespace extensions
