// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_api.h"

#include "base/lazy_instance.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/api/experimental_identity.h"
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

namespace identity_constants {
const char kInvalidClientId[] = "Invalid OAuth2 Client ID.";
const char kInvalidScopes[] = "Invalid OAuth2 scopes.";
const char kAuthFailure[] = "OAuth2 request failed: ";
const char kNoGrant[] = "OAuth2 not granted or revoked.";
const char kUserRejected[] = "The user did not approve access.";
const char kUserNotSignedIn[] = "The user is not signed in.";
const char kInteractionRequired[] = "User interaction required.";
const char kInvalidRedirect[] = "Did not redirect to the right URL.";
}  // namespace identity_constants

namespace {

static const char kChromiumDomainRedirectUrlPattern[] =
    "https://%s.chromiumapp.org/";

}  // namespace

namespace GetAuthToken = api::experimental_identity::GetAuthToken;
namespace LaunchWebAuthFlow = api::experimental_identity::LaunchWebAuthFlow;
namespace identity = api::experimental_identity;

IdentityGetAuthTokenFunction::IdentityGetAuthTokenFunction()
    : should_prompt_for_scopes_(false),
      should_prompt_for_signin_(false) {}
IdentityGetAuthTokenFunction::~IdentityGetAuthTokenFunction() {}

bool IdentityGetAuthTokenFunction::RunImpl() {
  scoped_ptr<GetAuthToken::Params> params(GetAuthToken::Params::Create(*args_));
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

  // Balanced in OnIssueAdviceSuccess|OnMintTokenSuccess|OnMintTokenFailure|
  // InstallUIAbort|SigninFailed.
  AddRef();

  if (!HasLoginToken()) {
    if (!should_prompt_for_signin_) {
      error_ = identity_constants::kUserNotSignedIn;
      Release();
      return false;
    }
    // Display a login prompt. If the subsequent mint fails, don't display the
    // prompt again.
    should_prompt_for_signin_ = false;
    ShowLoginPopup();
  } else {
    TokenService* token_service = TokenServiceFactory::GetForProfile(profile());
    refresh_token_ = token_service->GetOAuth2LoginRefreshToken();
    StartFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
  }

  return true;
}

void IdentityGetAuthTokenFunction::OnMintTokenSuccess(
    const std::string& access_token) {
  SetResult(Value::CreateStringValue(access_token));
  SendResponse(true);
  Release();  // Balanced in RunImpl.
}

void IdentityGetAuthTokenFunction::OnMintTokenFailure(
    const GoogleServiceAuthError& error) {
  switch (error.state()) {
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::ACCOUNT_DELETED:
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
      extensions::IdentityAPI::GetFactoryInstance()->GetForProfile(
          profile())->ReportAuthError(error);
      if (should_prompt_for_signin_) {
        // Display a login prompt and try again (once).
        should_prompt_for_signin_ = false;
        ShowLoginPopup();
        return;
      }
      break;
    default:
      // Return error to caller.
      break;
  }

  error_ = std::string(identity_constants::kAuthFailure) + error.ToString();
  SendResponse(false);
  Release();  // Balanced in RunImpl.
}

void IdentityGetAuthTokenFunction::OnIssueAdviceSuccess(
    const IssueAdviceInfo& issue_advice) {
  should_prompt_for_signin_ = false;
  // Existing grant was revoked and we used NO_FORCE, so we got info back
  // instead.
  if (should_prompt_for_scopes_) {
    install_ui_.reset(new ExtensionInstallPrompt(GetAssociatedWebContents()));
    ShowOAuthApprovalDialog(issue_advice);
  } else {
    error_ = identity_constants::kNoGrant;
    SendResponse(false);
    Release();  // Balanced in RunImpl.
  }
}

void IdentityGetAuthTokenFunction::SigninSuccess(const std::string& token) {
  refresh_token_ = token;
  StartFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
}

void IdentityGetAuthTokenFunction::SigninFailed() {
  error_ = identity_constants::kUserNotSignedIn;
  SendResponse(false);
  Release();
}

void IdentityGetAuthTokenFunction::InstallUIProceed() {
  DCHECK(install_ui_->record_oauth2_grant());
  // The user has accepted the scopes, so we may now force (recording a grant
  // and receiving a token).
  StartFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_FORCE);
}

void IdentityGetAuthTokenFunction::InstallUIAbort(bool user_initiated) {
  error_ = identity_constants::kUserRejected;
  SendResponse(false);
  Release();  // Balanced in RunImpl.
}

void IdentityGetAuthTokenFunction::StartFlow(OAuth2MintTokenFlow::Mode mode) {
  signin_flow_.reset(NULL);
  mint_token_flow_.reset(CreateMintTokenFlow(mode));
  mint_token_flow_->Start();
}

void IdentityGetAuthTokenFunction::ShowLoginPopup() {
  signin_flow_.reset(new IdentitySigninFlow(this, profile()));
  signin_flow_->Start();
}

void IdentityGetAuthTokenFunction::ShowOAuthApprovalDialog(
    const IssueAdviceInfo& issue_advice) {
  install_ui_->ConfirmIssueAdvice(this, GetExtension(), issue_advice);
}

OAuth2MintTokenFlow* IdentityGetAuthTokenFunction::CreateMintTokenFlow(
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

bool IdentityLaunchWebAuthFlowFunction::IsFinalRedirectURL(
    const GURL& url) const {
  std::vector<GURL>::const_iterator iter;
  for (iter = final_prefixes_.begin(); iter != final_prefixes_.end(); ++iter) {
    if (url.GetWithEmptyPath() == *iter) {
      return true;
    }
  }
  return false;
}

void IdentityLaunchWebAuthFlowFunction::InitFinalRedirectURLPrefixesForTest(
    const std::string& extension_id) {
  final_prefixes_.clear();
  InitFinalRedirectURLPrefixes(extension_id);
}

void IdentityLaunchWebAuthFlowFunction::InitFinalRedirectURLPrefixes(
    const std::string& extension_id) {
  final_prefixes_.push_back(
      Extension::GetBaseURLFromExtensionId(extension_id));
  final_prefixes_.push_back(GURL(base::StringPrintf(
      kChromiumDomainRedirectUrlPattern, extension_id.c_str())));
}

void IdentityLaunchWebAuthFlowFunction::OnAuthFlowFailure(
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

void IdentityLaunchWebAuthFlowFunction::OnAuthFlowURLChange(
    const GURL& redirect_url) {
  if (IsFinalRedirectURL(redirect_url)) {
    SetResult(Value::CreateStringValue(redirect_url.spec()));
    SendResponse(true);
    Release();  // Balanced in RunImpl.
  }
}

IdentityAPI::IdentityAPI(Profile* profile)
    : profile_(profile),
      signin_manager_(NULL),
      error_(GoogleServiceAuthError::NONE) {
  (new OAuth2ManifestHandler)->Register();
}

IdentityAPI::~IdentityAPI() {
}

void IdentityAPI::Initialize() {
  signin_manager_ = SigninManagerFactory::GetForProfile(profile_);
  signin_manager_->signin_global_error()->AddProvider(this);

  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_AVAILABLE,
                 content::Source<TokenService>(token_service));
}

void IdentityAPI::ReportAuthError(const GoogleServiceAuthError& error) {
  if (!signin_manager_)
    Initialize();

  error_ = error;
  signin_manager_->signin_global_error()->AuthStatusChanged();
}

void IdentityAPI::Shutdown() {
  if (signin_manager_)
    signin_manager_->signin_global_error()->RemoveProvider(this);
}

static base::LazyInstance<ProfileKeyedAPIFactory<IdentityAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<IdentityAPI>* IdentityAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

GoogleServiceAuthError IdentityAPI::GetAuthStatus() const {
  return error_;
}

void IdentityAPI::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  CHECK(type == chrome::NOTIFICATION_TOKEN_AVAILABLE);
  TokenService::TokenAvailableDetails* token_details =
      content::Details<TokenService::TokenAvailableDetails>(details).ptr();
  if (token_details->service() ==
      GaiaConstants::kGaiaOAuth2LoginRefreshToken) {
    error_ = GoogleServiceAuthError::AuthErrorNone();
    signin_manager_->signin_global_error()->AuthStatusChanged();
  }
}

template <>
void ProfileKeyedAPIFactory<IdentityAPI>::DeclareFactoryDependencies() {
  DependsOn(ExtensionSystemFactory::GetInstance());
  DependsOn(TokenServiceFactory::GetInstance());
  DependsOn(SigninManagerFactory::GetInstance());
}

}  // namespace extensions
