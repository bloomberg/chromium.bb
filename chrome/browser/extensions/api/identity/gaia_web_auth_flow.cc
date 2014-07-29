// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/gaia_web_auth_flow.h"

#include "base/debug/trace_event.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/escape.h"

namespace extensions {

GaiaWebAuthFlow::GaiaWebAuthFlow(Delegate* delegate,
                                 Profile* profile,
                                 const ExtensionTokenKey* token_key,
                                 const std::string& oauth2_client_id,
                                 const std::string& locale)
    : delegate_(delegate),
      profile_(profile),
      account_id_(token_key->account_id) {
  TRACE_EVENT_ASYNC_BEGIN2("identity",
                           "GaiaWebAuthFlow",
                           this,
                           "extension_id",
                           token_key->extension_id,
                           "account_id",
                           token_key->account_id);

  const char kOAuth2RedirectPathFormat[] = "/%s#";
  const char kOAuth2AuthorizeFormat[] =
      "?response_type=token&approval_prompt=force&authuser=0&"
      "client_id=%s&"
      "scope=%s&"
      "origin=chrome-extension://%s/&"
      "redirect_uri=%s:/%s&"
      "hl=%s";

  std::vector<std::string> scopes(token_key->scopes.begin(),
                                  token_key->scopes.end());
  std::vector<std::string> client_id_parts;
  base::SplitString(oauth2_client_id, '.', &client_id_parts);
  std::reverse(client_id_parts.begin(), client_id_parts.end());
  redirect_scheme_ = JoinString(client_id_parts, '.');

  redirect_path_prefix_ = base::StringPrintf(kOAuth2RedirectPathFormat,
                                             token_key->extension_id.c_str());

  auth_url_ =
      GaiaUrls::GetInstance()->oauth2_auth_url().Resolve(base::StringPrintf(
          kOAuth2AuthorizeFormat,
          oauth2_client_id.c_str(),
          net::EscapeUrlEncodedData(JoinString(scopes, ' '), true).c_str(),
          token_key->extension_id.c_str(),
          redirect_scheme_.c_str(),
          token_key->extension_id.c_str(),
          locale.c_str()));
}

GaiaWebAuthFlow::~GaiaWebAuthFlow() {
  TRACE_EVENT_ASYNC_END0("identity", "GaiaWebAuthFlow", this);

  if (web_flow_)
    web_flow_.release()->DetachDelegateAndDelete();
}

void GaiaWebAuthFlow::Start() {
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  ubertoken_fetcher_.reset(new UbertokenFetcher(token_service,
                                                this,
                                                profile_->GetRequestContext()));
  ubertoken_fetcher_->StartFetchingToken(account_id_);
}

void GaiaWebAuthFlow::OnUbertokenSuccess(const std::string& token) {
  TRACE_EVENT_ASYNC_STEP_PAST0(
      "identity", "GaiaWebAuthFlow", this, "OnUbertokenSuccess");

  const char kMergeSessionQueryFormat[] = "?uberauth=%s&"
                                          "continue=%s&"
                                          "source=appsv2";

  std::string merge_query = base::StringPrintf(
      kMergeSessionQueryFormat,
      net::EscapeUrlEncodedData(token, true).c_str(),
      net::EscapeUrlEncodedData(auth_url_.spec(), true).c_str());
  GURL merge_url(
      GaiaUrls::GetInstance()->merge_session_url().Resolve(merge_query));

  web_flow_ = CreateWebAuthFlow(merge_url);
  web_flow_->Start();
}

void GaiaWebAuthFlow::OnUbertokenFailure(const GoogleServiceAuthError& error) {
  TRACE_EVENT_ASYNC_STEP_PAST1("identity",
                               "GaiaWebAuthFlow",
                               this,
                               "OnUbertokenSuccess",
                               "error",
                               error.ToString());

  DVLOG(1) << "OnUbertokenFailure: " << error.error_message();
  delegate_->OnGaiaFlowFailure(
      GaiaWebAuthFlow::SERVICE_AUTH_ERROR, error, std::string());
}

void GaiaWebAuthFlow::OnAuthFlowFailure(WebAuthFlow::Failure failure) {
  GaiaWebAuthFlow::Failure gaia_failure;

  switch (failure) {
    case WebAuthFlow::WINDOW_CLOSED:
      gaia_failure = GaiaWebAuthFlow::WINDOW_CLOSED;
      break;
    case WebAuthFlow::LOAD_FAILED:
      DVLOG(1) << "OnAuthFlowFailure LOAD_FAILED";
      gaia_failure = GaiaWebAuthFlow::LOAD_FAILED;
      break;
    default:
      NOTREACHED() << "Unexpected error from web auth flow: " << failure;
      gaia_failure = GaiaWebAuthFlow::LOAD_FAILED;
      break;
  }

  TRACE_EVENT_ASYNC_STEP_PAST1("identity",
                               "GaiaWebAuthFlow",
                               this,
                               "OnAuthFlowFailure",
                               "error",
                               gaia_failure);

  delegate_->OnGaiaFlowFailure(
      gaia_failure,
      GoogleServiceAuthError(GoogleServiceAuthError::NONE),
      std::string());
}

void GaiaWebAuthFlow::OnAuthFlowURLChange(const GURL& url) {
  TRACE_EVENT_ASYNC_STEP_PAST0("identity",
                               "GaiaWebAuthFlow",
                               this,
                               "OnAuthFlowURLChange");

  const char kOAuth2RedirectAccessTokenKey[] = "access_token";
  const char kOAuth2RedirectErrorKey[] = "error";
  const char kOAuth2ExpiresInKey[] = "expires_in";

  // The format of the target URL is:
  //     reversed.oauth.client.id:/extensionid#access_token=TOKEN
  //
  // Because there is no double slash, everything after the scheme is
  // interpreted as a path, including the fragment.

  if (url.scheme() == redirect_scheme_ && !url.has_host() && !url.has_port() &&
      StartsWithASCII(url.GetContent(), redirect_path_prefix_, true)) {
    web_flow_.release()->DetachDelegateAndDelete();

    std::string fragment = url.GetContent().substr(
        redirect_path_prefix_.length(), std::string::npos);
    std::vector<std::pair<std::string, std::string> > pairs;
    base::SplitStringIntoKeyValuePairs(fragment, '=', '&', &pairs);
    std::string access_token;
    std::string error;
    std::string expiration;

    for (std::vector<std::pair<std::string, std::string> >::iterator
             it = pairs.begin();
         it != pairs.end();
         ++it) {
      if (it->first == kOAuth2RedirectAccessTokenKey)
        access_token = it->second;
      else if (it->first == kOAuth2RedirectErrorKey)
        error = it->second;
      else if (it->first == kOAuth2ExpiresInKey)
        expiration = it->second;
    }

    if (access_token.empty() && error.empty()) {
      delegate_->OnGaiaFlowFailure(
          GaiaWebAuthFlow::INVALID_REDIRECT,
          GoogleServiceAuthError(GoogleServiceAuthError::NONE),
          std::string());
    } else if (!error.empty()) {
      delegate_->OnGaiaFlowFailure(
          GaiaWebAuthFlow::OAUTH_ERROR,
          GoogleServiceAuthError(GoogleServiceAuthError::NONE),
          error);
    } else {
      delegate_->OnGaiaFlowCompleted(access_token, expiration);
    }
  }
}

void GaiaWebAuthFlow::OnAuthFlowTitleChange(const std::string& title) {
  // On the final page the title will be "Loading <redirect-url>".
  // Treat it as though we'd really been redirected to <redirect-url>.
  const char kRedirectPrefix[] = "Loading ";
  std::string prefix(kRedirectPrefix);

  if (StartsWithASCII(title, prefix, true)) {
    GURL url(title.substr(prefix.length(), std::string::npos));
    if (url.is_valid())
      OnAuthFlowURLChange(url);
  }
}

scoped_ptr<WebAuthFlow> GaiaWebAuthFlow::CreateWebAuthFlow(GURL url) {
  return scoped_ptr<WebAuthFlow>(new WebAuthFlow(this,
                                                 profile_,
                                                 url,
                                                 WebAuthFlow::INTERACTIVE));
}

}  // namespace extensions
