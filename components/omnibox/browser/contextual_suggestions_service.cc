// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/contextual_suggestions_service.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/stringprintf.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

// Server address for the experimental suggestions service.
const char kExperimentalServerAddress[] =
    "https://cuscochromeextension-pa.googleapis.com/v1/zerosuggestions";

}  // namespace

ContextualSuggestionsService::ContextualSuggestionsService(
    SigninManagerBase* signin_manager,
    OAuth2TokenService* token_service,
    net::URLRequestContextGetter* request_context)
    : request_context_(request_context),
      signin_manager_(signin_manager),
      token_service_(token_service),
      token_fetcher_(nullptr) {}

ContextualSuggestionsService::~ContextualSuggestionsService() {}

void ContextualSuggestionsService::CreateContextualSuggestionsRequest(
    const std::string& current_url,
    const TemplateURLService* template_url_service,
    net::URLFetcherDelegate* fetcher_delegate,
    ContextualSuggestionsCallback callback) {
  const GURL experimental_suggest_url =
      ExperimentalZeroSuggestURL(current_url, template_url_service);
  const bool is_experimental = experimental_suggest_url.is_valid();
  const GURL suggest_url =
      is_experimental
          ? experimental_suggest_url
          : ContextualSuggestionsUrl(current_url, template_url_service);

  std::unique_ptr<net::URLFetcher> fetcher =
      CreateRequest(suggest_url, is_experimental, fetcher_delegate);

  const bool should_fetch_access_token = is_experimental &&
                                         (signin_manager_ != nullptr) &&
                                         (token_service_ != nullptr);
  // If authentication services are unavailable or if this request is still
  // waiting for an oauth2 token, run the contextual service without access
  // tokens.
  if (!should_fetch_access_token || (token_fetcher_ != nullptr)) {
    std::move(callback).Run(std::move(fetcher));
    return;
  }

  // Create the oauth2 token fetcher.
  const OAuth2TokenService::ScopeSet scopes{
      "https://www.googleapis.com/auth/cusco-chrome-extension"};
  token_fetcher_ = base::MakeUnique<AccessTokenFetcher>(
      "contextual_suggestions_service", signin_manager_, token_service_, scopes,
      base::BindOnce(&ContextualSuggestionsService::AccessTokenAvailable,
                     base::Unretained(this), std::move(fetcher),
                     std::move(callback)));
}

void ContextualSuggestionsService::StopCreatingContextualSuggestionsRequest() {
  std::unique_ptr<AccessTokenFetcher> token_fetcher_deleter(
      std::move(token_fetcher_));
}

// static
GURL ContextualSuggestionsService::ContextualSuggestionsUrl(
    const std::string& current_url,
    const TemplateURLService* template_url_service) {
  if (template_url_service == nullptr) {
    return GURL();
  }

  const TemplateURL* search_engine =
      template_url_service->GetDefaultSearchProvider();
  if (search_engine == nullptr) {
    return GURL();
  }

  const TemplateURLRef& suggestion_url_ref =
      search_engine->suggestions_url_ref();
  const SearchTermsData& search_terms_data =
      template_url_service->search_terms_data();
  base::string16 prefix;
  TemplateURLRef::SearchTermsArgs search_term_args(prefix);
  if (!current_url.empty()) {
    search_term_args.current_page_url = current_url;
  }
  return GURL(suggestion_url_ref.ReplaceSearchTerms(search_term_args,
                                                    search_terms_data));
}

GURL ContextualSuggestionsService::ExperimentalZeroSuggestURL(
    const std::string& current_url,
    const TemplateURLService* template_url_service) const {
  if (current_url.empty()) {
    return GURL();
  }

  if (!base::FeatureList::IsEnabled(omnibox::kZeroSuggestRedirectToChrome) ||
      template_url_service == nullptr) {
    return GURL();
  }

  // Check that the default search engine is Google.
  const TemplateURL& default_provider_url =
      *template_url_service->GetDefaultSearchProvider();
  const SearchTermsData& search_terms_data =
      template_url_service->search_terms_data();
  if (default_provider_url.GetEngineType(search_terms_data) !=
      SEARCH_ENGINE_GOOGLE) {
    return GURL();
  }

  // Assemble the actual suggest URL.
  const std::string server_address(kExperimentalServerAddress);
  const int experiment_id =
      OmniboxFieldTrial::GetZeroSuggestRedirectToChromeExperimentId();
  // The experimental suggest endpoint does not handle URLs terminating with a
  // slash properly, causing some urls like http://example.com/some/path/ to
  // lose the last slash, which changes the URL. If we add an extra ampersand,
  // as in the else case here it handles the URL properly.
  const std::string additional_parameters =
      (experiment_id >= 0)
          ? base::StringPrintf("&experiment_id=%d", experiment_id)
          : "&";
  GURL suggest_url(server_address + "/url=" + net::EscapePath(current_url) +
                   additional_parameters);

  // Check that the suggest URL for redirect to chrome field trial is valid.
  if (!suggest_url.is_valid()) {
    return GURL();
  }

  // Check that the suggest URL for redirect to chrome is HTTPS.
  if (!suggest_url.SchemeIsCryptographic()) {
    return GURL();
  }

  return suggest_url;
}

std::unique_ptr<net::URLFetcher> ContextualSuggestionsService::CreateRequest(
    const GURL& suggest_url,
    bool is_experimental,
    net::URLFetcherDelegate* fetcher_delegate) const {
  DCHECK(suggest_url.is_valid());

  net::NetworkTrafficAnnotationTag annotation_tag =
      is_experimental
          ? net::DefineNetworkTrafficAnnotation(
                "omnibox_zerosuggest_experimental", R"(
        semantics {
          sender: "Omnibox"
          description:
            "When the user focuses the omnibox, Chrome can provide search or "
            "navigation suggestions from the default search provider in the "
            "omnibox dropdown, based on the current page URL.\n"
            "This is limited to users whose default search engine is Google, "
            "as no other search engines currently support this kind of "
            "suggestion."
          trigger: "The omnibox receives focus."
          data: "The user's OAuth2 credentials and the URL of the current page."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this feature via the 'Use a prediction service "
            "to help complete searches and URLs typed in the address bar' "
            "settings under 'Privacy'. The feature is enabled by default."
          chrome_policy {
            SearchSuggestEnabled {
                policy_options {mode: MANDATORY}
                SearchSuggestEnabled: false
            }
          }
        })")
          : net::DefineNetworkTrafficAnnotation("omnibox_zerosuggest", R"(
        semantics {
          sender: "Omnibox"
          description:
            "When the user focuses the omnibox, Chrome can provide search or "
            "navigation suggestions from the default search provider in the "
            "omnibox dropdown, based on the current page URL.\n"
            "This is limited to users whose default search engine is Google, "
            "as no other search engines currently support this kind of "
            "suggestion."
          trigger: "The omnibox receives focus."
          data: "The URL of the current page."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "Users can control this feature via the 'Use a prediction service "
            "to help complete searches and URLs typed in the address bar' "
            "settings under 'Privacy'. The feature is enabled by default."
          chrome_policy {
            SearchSuggestEnabled {
                policy_options {mode: MANDATORY}
                SearchSuggestEnabled: false
            }
          }
        })");

  const int kFetcherID = 1;
  std::unique_ptr<net::URLFetcher> fetcher =
      net::URLFetcher::Create(kFetcherID, suggest_url, net::URLFetcher::GET,
                              fetcher_delegate, annotation_tag);
  fetcher->SetRequestContext(request_context_);
  data_use_measurement::DataUseUserData::AttachToFetcher(
      fetcher.get(), data_use_measurement::DataUseUserData::OMNIBOX);

  // Add Chrome experiment state to the request headers.
  net::HttpRequestHeaders headers;
  // Note: It's OK to pass |is_signed_in| false if it's unknown, as it does
  // not affect transmission of experiments coming from the variations server.
  variations::AppendVariationHeaders(fetcher->GetOriginalURL(),
                                     /*incognito=*/false, /*uma_enabled=*/false,
                                     /*is_signed_in=*/false, &headers);
  for (net::HttpRequestHeaders::Iterator it(headers); it.GetNext();) {
    fetcher->AddExtraRequestHeader(it.name() + ":" + it.value());
  }

  if (is_experimental) {
    fetcher->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                          net::LOAD_DO_NOT_SAVE_COOKIES);
  } else {
    fetcher->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES);
  }

  return fetcher;
}

void ContextualSuggestionsService::AccessTokenAvailable(
    std::unique_ptr<net::URLFetcher> fetcher,
    ContextualSuggestionsCallback callback,
    const GoogleServiceAuthError& error,
    const std::string& access_token) {
  DCHECK(token_fetcher_);
  std::unique_ptr<AccessTokenFetcher> token_fetcher_deleter(
      std::move(token_fetcher_));

  // If there were no errors obtaining the access token, append it to the
  // request as a header.
  if (error.state() == GoogleServiceAuthError::NONE) {
    DCHECK(!access_token.empty());
    fetcher->AddExtraRequestHeader(
        base::StringPrintf("Authorization: Bearer %s", access_token.c_str()));
  }

  std::move(callback).Run(std::move(fetcher));
}
