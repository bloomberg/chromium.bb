// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_api.h"

#include <set>
#include <string>
#include <vector>

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
#include "chrome/common/extensions/api/identity.h"
#include "chrome/common/extensions/api/identity/oauth2_manifest_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
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
const char kOffTheRecord[] = "Identity API is disabled in incognito windows.";

const int kCachedIssueAdviceTTLSeconds = 1;
}  // namespace identity_constants

namespace {

static const char kChromiumDomainRedirectUrlPattern[] =
    "https://%s.chromiumapp.org/";

}  // namespace

namespace identity = api::identity;

IdentityGetAuthTokenFunction::IdentityGetAuthTokenFunction()
    : should_prompt_for_scopes_(false),
      should_prompt_for_signin_(false) {}

IdentityGetAuthTokenFunction::~IdentityGetAuthTokenFunction() {}

bool IdentityGetAuthTokenFunction::RunImpl() {
  if (profile()->IsOffTheRecord()) {
    error_ = identity_constants::kOffTheRecord;
    return false;
  }

  scoped_ptr<identity::GetAuthToken::Params> params(
      identity::GetAuthToken::Params::Create(*args_));
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

void IdentityGetAuthTokenFunction::CompleteFunctionWithResult(
    const std::string& access_token) {
  SetResult(Value::CreateStringValue(access_token));
  SendResponse(true);
  Release();  // Balanced in RunImpl.
}

void IdentityGetAuthTokenFunction::CompleteFunctionWithError(
    const std::string& error) {
  error_ = error;
  SendResponse(false);
  Release();  // Balanced in RunImpl.
}

void IdentityGetAuthTokenFunction::StartSigninFlow() {
  // All cached tokens are invalid because the user is not signed in.
  IdentityAPI* id_api =
      extensions::IdentityAPI::GetFactoryInstance()->GetForProfile(profile_);
  id_api->EraseAllCachedTokens();
  // Display a login prompt. If the subsequent mint fails, don't display the
  // login prompt again.
  should_prompt_for_signin_ = false;
  ShowLoginPopup();
}

void IdentityGetAuthTokenFunction::StartMintTokenFlow(
    IdentityMintRequestQueue::MintType type) {
  mint_token_flow_type_ = type;

  // Flows are serialized to prevent excessive traffic to GAIA, and
  // to consolidate UI pop-ups.
  const OAuth2Info& oauth2_info = OAuth2Info::GetOAuth2Info(GetExtension());
  std::set<std::string> scopes(oauth2_info.scopes.begin(),
                               oauth2_info.scopes.end());
  IdentityAPI* id_api =
      extensions::IdentityAPI::GetFactoryInstance()->GetForProfile(profile_);

  if (!should_prompt_for_scopes_) {
    // Caller requested no interaction.

    if (type == IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE) {
      // GAIA told us to do a consent UI.
      CompleteFunctionWithError(identity_constants::kNoGrant);
      return;
    }
    if (!id_api->mint_queue()->empty(
            IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE,
            GetExtension()->id(), scopes)) {
      // Another call is going through a consent UI.
      CompleteFunctionWithError(identity_constants::kNoGrant);
      return;
    }
  }
  id_api->mint_queue()->RequestStart(type,
                                     GetExtension()->id(),
                                     scopes,
                                     this);
}

void IdentityGetAuthTokenFunction::CompleteMintTokenFlow() {
  IdentityMintRequestQueue::MintType type = mint_token_flow_type_;

  const OAuth2Info& oauth2_info = OAuth2Info::GetOAuth2Info(GetExtension());
  std::set<std::string> scopes(oauth2_info.scopes.begin(),
                               oauth2_info.scopes.end());

  extensions::IdentityAPI::GetFactoryInstance()->GetForProfile(
      profile_)->mint_queue()->RequestComplete(type,
                                               GetExtension()->id(),
                                               scopes,
                                               this);
}

void IdentityGetAuthTokenFunction::StartMintToken(
    IdentityMintRequestQueue::MintType type) {
  const OAuth2Info& oauth2_info = OAuth2Info::GetOAuth2Info(GetExtension());
  IdentityAPI* id_api = IdentityAPI::GetFactoryInstance()->GetForProfile(
      profile());
  IdentityTokenCacheValue cache_entry = id_api->GetCachedToken(
      GetExtension()->id(), oauth2_info.scopes);
  IdentityTokenCacheValue::CacheValueStatus cache_status =
      cache_entry.status();

  if (type == IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE) {
    switch (cache_status) {
      case IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND:
        StartGaiaRequest(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
        break;

      case IdentityTokenCacheValue::CACHE_STATUS_TOKEN:
        CompleteMintTokenFlow();
        CompleteFunctionWithResult(cache_entry.token());
        break;

      case IdentityTokenCacheValue::CACHE_STATUS_ADVICE:
        CompleteMintTokenFlow();
        should_prompt_for_signin_ = false;
        issue_advice_ = cache_entry.issue_advice();
        StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE);
        break;
    }
  } else {
    DCHECK(type == IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE);

    if (cache_status == IdentityTokenCacheValue::CACHE_STATUS_TOKEN) {
      CompleteMintTokenFlow();
      CompleteFunctionWithResult(cache_entry.token());
    } else {
      install_ui_.reset(new ExtensionInstallPrompt(GetAssociatedWebContents()));
      ShowOAuthApprovalDialog(issue_advice_);
    }
  }
}

void IdentityGetAuthTokenFunction::OnMintTokenSuccess(
    const std::string& access_token, int time_to_live) {
  const OAuth2Info& oauth2_info = OAuth2Info::GetOAuth2Info(GetExtension());
  IdentityTokenCacheValue token(access_token,
                                base::TimeDelta::FromSeconds(time_to_live));
  IdentityAPI::GetFactoryInstance()->GetForProfile(profile())->SetCachedToken(
      GetExtension()->id(), oauth2_info.scopes, token);

  CompleteMintTokenFlow();
  CompleteFunctionWithResult(access_token);
}

void IdentityGetAuthTokenFunction::OnMintTokenFailure(
    const GoogleServiceAuthError& error) {
  CompleteMintTokenFlow();

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

void IdentityGetAuthTokenFunction::OnIssueAdviceSuccess(
    const IssueAdviceInfo& issue_advice) {
  const OAuth2Info& oauth2_info = OAuth2Info::GetOAuth2Info(GetExtension());
  IdentityAPI::GetFactoryInstance()->GetForProfile(profile())->SetCachedToken(
      GetExtension()->id(), oauth2_info.scopes,
      IdentityTokenCacheValue(issue_advice));
  CompleteMintTokenFlow();

  should_prompt_for_signin_ = false;
  // Existing grant was revoked and we used NO_FORCE, so we got info back
  // instead. Start a consent UI if we can.
  issue_advice_ = issue_advice;
  StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE);
}

void IdentityGetAuthTokenFunction::SigninSuccess(const std::string& token) {
  refresh_token_ = token;
  StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE);
}

void IdentityGetAuthTokenFunction::SigninFailed() {
  CompleteFunctionWithError(identity_constants::kUserNotSignedIn);
}

void IdentityGetAuthTokenFunction::InstallUIProceed() {
  // The user has accepted the scopes, so we may now force (recording a grant
  // and receiving a token).
  StartGaiaRequest(OAuth2MintTokenFlow::MODE_MINT_TOKEN_FORCE);
}

void IdentityGetAuthTokenFunction::InstallUIAbort(bool user_initiated) {
  CompleteMintTokenFlow();
  CompleteFunctionWithError(identity_constants::kUserRejected);
}

void IdentityGetAuthTokenFunction::StartGaiaRequest(
    OAuth2MintTokenFlow::Mode mode) {
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

IdentityRemoveCachedAuthTokenFunction::IdentityRemoveCachedAuthTokenFunction() {
}

IdentityRemoveCachedAuthTokenFunction::
    ~IdentityRemoveCachedAuthTokenFunction() {
}

bool IdentityRemoveCachedAuthTokenFunction::RunImpl() {
  if (profile()->IsOffTheRecord()) {
    error_ = identity_constants::kOffTheRecord;
    return false;
  }

  scoped_ptr<identity::RemoveCachedAuthToken::Params> params(
      identity::RemoveCachedAuthToken::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  IdentityAPI::GetFactoryInstance()->GetForProfile(profile())->EraseCachedToken(
      GetExtension()->id(), params->details.token);
  return true;
}

IdentityLaunchWebAuthFlowFunction::IdentityLaunchWebAuthFlowFunction() {}
IdentityLaunchWebAuthFlowFunction::~IdentityLaunchWebAuthFlowFunction() {}

bool IdentityLaunchWebAuthFlowFunction::RunImpl() {
  if (profile()->IsOffTheRecord()) {
    error_ = identity_constants::kOffTheRecord;
    return false;
  }

  scoped_ptr<identity::LaunchWebAuthFlow::Params> params(
      identity::LaunchWebAuthFlow::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL auth_url(params->details.url);
  WebAuthFlow::Mode mode =
      params->details.interactive && *params->details.interactive ?
      WebAuthFlow::INTERACTIVE : WebAuthFlow::SILENT;

  // Set up acceptable target URLs. (Does not include chrome-extension
  // scheme for this version of the API.)
  InitFinalRedirectURLPrefixes(GetExtension()->id());

  gfx::Rect initial_bounds;

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

IdentityTokenCacheValue::IdentityTokenCacheValue()
    : status_(CACHE_STATUS_NOTFOUND) {
}

IdentityTokenCacheValue::IdentityTokenCacheValue(
    const IssueAdviceInfo& issue_advice) : status_(CACHE_STATUS_ADVICE),
                                           issue_advice_(issue_advice) {
  expiration_time_ = base::Time::Now() + base::TimeDelta::FromSeconds(
      identity_constants::kCachedIssueAdviceTTLSeconds);
}

IdentityTokenCacheValue::IdentityTokenCacheValue(
    const std::string& token, base::TimeDelta time_to_live)
    : status_(CACHE_STATUS_TOKEN),
      token_(token) {
  base::TimeDelta zero_delta;
  if (time_to_live < zero_delta)
    time_to_live = zero_delta;

  expiration_time_ = base::Time::Now() + time_to_live;
}

IdentityTokenCacheValue::~IdentityTokenCacheValue() {
}

IdentityTokenCacheValue::CacheValueStatus
    IdentityTokenCacheValue::status() const {
  if (is_expired())
    return IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND;
  else
    return status_;
}

const IssueAdviceInfo& IdentityTokenCacheValue::issue_advice() const {
  return issue_advice_;
}

const std::string& IdentityTokenCacheValue::token() const {
  return token_;
}

bool IdentityTokenCacheValue::is_expired() const {
  return status_ == CACHE_STATUS_NOTFOUND ||
      expiration_time_ < base::Time::Now();
}

IdentityAPI::IdentityAPI(Profile* profile)
    : profile_(profile),
      signin_manager_(NULL),
      error_(GoogleServiceAuthError::NONE) {
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

IdentityMintRequestQueue* IdentityAPI::mint_queue() {
    return &mint_queue_;
}

void IdentityAPI::SetCachedToken(const std::string& extension_id,
                                 const std::vector<std::string> scopes,
                                 const IdentityTokenCacheValue& token_data) {
  std::set<std::string> scopeset(scopes.begin(), scopes.end());
  TokenCacheKey key(extension_id, scopeset);

  std::map<TokenCacheKey, IdentityTokenCacheValue>::iterator it =
      token_cache_.find(key);
  if (it != token_cache_.end() && it->second.status() <= token_data.status())
    token_cache_.erase(it);

  token_cache_.insert(std::make_pair(key, token_data));
}

void IdentityAPI::EraseCachedToken(const std::string& extension_id,
                                   const std::string& token) {
  std::map<TokenCacheKey, IdentityTokenCacheValue>::iterator it;
  for (it = token_cache_.begin(); it != token_cache_.end(); ++it) {
    if (it->first.extension_id == extension_id &&
        it->second.status() == IdentityTokenCacheValue::CACHE_STATUS_TOKEN &&
        it->second.token() == token) {
      token_cache_.erase(it);
      break;
    }
  }
}

void IdentityAPI::EraseAllCachedTokens() {
  token_cache_.clear();
}

const IdentityTokenCacheValue& IdentityAPI::GetCachedToken(
    const std::string& extension_id, const std::vector<std::string> scopes) {
  std::set<std::string> scopeset(scopes.begin(), scopes.end());
  TokenCacheKey key(extension_id, scopeset);
  return token_cache_[key];
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

IdentityAPI::TokenCacheKey::TokenCacheKey(const std::string& extension_id,
                                          const std::set<std::string> scopes)
    : extension_id(extension_id),
      scopes(scopes) {
}

IdentityAPI::TokenCacheKey::~TokenCacheKey() {
}

bool IdentityAPI::TokenCacheKey::operator<(
    const IdentityAPI::TokenCacheKey& rhs) const {
  if (extension_id < rhs.extension_id)
    return true;
  else if (rhs.extension_id < extension_id)
    return false;

  return scopes < rhs.scopes;
}

}  // namespace extensions
