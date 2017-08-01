// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/contextual_suggestions_fetcher_impl.h"

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/ntp_snippets/category.h"
#include "components/signin/core/browser/access_token_fetcher.h"
#include "components/strings/grit/components_strings.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "ui/base/l10n/l10n_util.h"

using net::HttpRequestHeaders;
using net::URLFetcher;
using net::URLRequestContextGetter;

namespace ntp_snippets {

using internal::ContextualJsonRequest;
using internal::FetchResult;

namespace {

const char kContentSuggestionsApiScope[] =
    "https://www.googleapis.com/auth/chrome-content-suggestions";
const char kAuthorizationRequestHeaderFormat[] = "Bearer %s";

std::string FetchResultToString(FetchResult result) {
  switch (result) {
    case FetchResult::SUCCESS:
      return "OK";
    case FetchResult::URL_REQUEST_STATUS_ERROR:
      return "URLRequestStatus error";
    case FetchResult::HTTP_ERROR:
      return "HTTP error";
    case FetchResult::JSON_PARSE_ERROR:
      return "Received invalid JSON";
    case FetchResult::INVALID_SNIPPET_CONTENT_ERROR:
      return "Invalid / empty list.";
    case FetchResult::OAUTH_TOKEN_ERROR:
      return "Error in obtaining an OAuth2 access token.";
    case FetchResult::MISSING_API_KEY:
      return "No API key available.";
    case FetchResult::RESULT_MAX:
      break;
  }
  NOTREACHED();
  return "Unknown error";
}

Status FetchResultToStatus(FetchResult result) {
  switch (result) {
    case FetchResult::SUCCESS:
      return Status::Success();
    // Permanent errors occur if it is more likely that the error originated
    // from the client.
    case FetchResult::OAUTH_TOKEN_ERROR:
    case FetchResult::MISSING_API_KEY:
      return Status(StatusCode::PERMANENT_ERROR, FetchResultToString(result));
    // Temporary errors occur if it's more likely that the client behaved
    // correctly but the server failed to respond as expected.
    // TODO(fhorschig): Revisit HTTP_ERROR once the rescheduling was reworked.
    case FetchResult::HTTP_ERROR:
    case FetchResult::URL_REQUEST_STATUS_ERROR:
    case FetchResult::INVALID_SNIPPET_CONTENT_ERROR:
    case FetchResult::JSON_PARSE_ERROR:
      return Status(StatusCode::TEMPORARY_ERROR, FetchResultToString(result));
    case FetchResult::RESULT_MAX:
      break;
  }
  NOTREACHED();
  return Status(StatusCode::PERMANENT_ERROR, std::string());
}

std::string GetFetchEndpoint() {
  return "https://alpha-chromecontentsuggestions-pa.sandbox.googleapis.com/v1"
         "/publicdebate"
         "/getsuggestions";
}

// Creates suggestions from dictionary values in |list| and adds them to
// |suggestions|. Returns true on success, false if anything went wrong.
bool AddSuggestionsFromListValue(bool content_suggestions_api,
                                 const base::ListValue& list,
                                 RemoteSuggestion::PtrVector* suggestions) {
  for (const auto& value : list) {
    const base::DictionaryValue* dict = nullptr;
    if (!value.GetAsDictionary(&dict)) {
      return false;
    }

    std::string s;
    dict->GetAsString(&s);
    DVLOG(1) << "AddSuggestionsFromListValue " << s;
    std::unique_ptr<RemoteSuggestion> suggestion =
        RemoteSuggestion::CreateFromContextualSuggestionsDictionary(*dict);
    suggestions->push_back(std::move(suggestion));
  }
  return true;
}

}  // namespace

ContextualSuggestionsFetcherImpl::ContextualSuggestionsFetcherImpl(
    SigninManagerBase* signin_manager,
    OAuth2TokenService* token_service,
    scoped_refptr<URLRequestContextGetter> url_request_context_getter,
    PrefService* pref_service,
    const ParseJSONCallback& parse_json_callback)
    : signin_manager_(signin_manager),
      token_service_(token_service),
      url_request_context_getter_(std::move(url_request_context_getter)),
      parse_json_callback_(parse_json_callback),
      fetch_url_(GetFetchEndpoint()) {}

ContextualSuggestionsFetcherImpl::~ContextualSuggestionsFetcherImpl() = default;

const std::string& ContextualSuggestionsFetcherImpl::GetLastStatusForTesting()
    const {
  return last_status_;
}
const std::string& ContextualSuggestionsFetcherImpl::GetLastJsonForTesting()
    const {
  return last_fetch_json_;
}
const GURL& ContextualSuggestionsFetcherImpl::GetFetchUrlForTesting() const {
  return fetch_url_;
}

void ContextualSuggestionsFetcherImpl::FetchContextualSuggestions(
    const GURL& url,
    SuggestionsAvailableCallback callback) {
  ContextualJsonRequest::Builder builder;
  builder.SetParseJsonCallback(parse_json_callback_)
      .SetUrlRequestContextGetter(url_request_context_getter_)
      .SetContentUrl(url);

  pending_requests_.emplace(std::move(builder), std::move(callback));
  StartTokenRequest();
}

void ContextualSuggestionsFetcherImpl::StartRequest(
    ContextualJsonRequest::Builder builder,
    SuggestionsAvailableCallback callback,
    const std::string& oauth_access_token) {
  builder.SetUrl(fetch_url_)
      .SetAuthentication(signin_manager_->GetAuthenticatedAccountId(),
                         base::StringPrintf(kAuthorizationRequestHeaderFormat,
                                            oauth_access_token.c_str()));
  DVLOG(1) << "ContextualSuggestionsFetcherImpl::StartRequest";
  std::unique_ptr<ContextualJsonRequest> request = builder.Build();
  ContextualJsonRequest* raw_request = request.get();
  raw_request->Start(base::BindOnce(
      &ContextualSuggestionsFetcherImpl::JsonRequestDone,
      base::Unretained(this), std::move(request), std::move(callback)));
}

void ContextualSuggestionsFetcherImpl::StartTokenRequest() {
  // If there is already an ongoing token request, just wait for that.
  if (token_fetcher_) {
    return;
  }

  OAuth2TokenService::ScopeSet scopes{kContentSuggestionsApiScope};
  token_fetcher_ = base::MakeUnique<AccessTokenFetcher>(
      "ntp_snippets", signin_manager_, token_service_, scopes,
      base::BindOnce(
          &ContextualSuggestionsFetcherImpl::AccessTokenFetchFinished,
          base::Unretained(this)));
}

void ContextualSuggestionsFetcherImpl::AccessTokenFetchFinished(
    const GoogleServiceAuthError& error,
    const std::string& access_token) {
  // Delete the fetcher only after we leave this method (which is called from
  // the fetcher itself).
  DCHECK(token_fetcher_);
  std::unique_ptr<AccessTokenFetcher> token_fetcher_deleter(
      std::move(token_fetcher_));

  if (error.state() != GoogleServiceAuthError::NONE) {
    AccessTokenError(error);
    return;
  }

  DCHECK(!access_token.empty());

  while (!pending_requests_.empty()) {
    std::pair<ContextualJsonRequest::Builder, SuggestionsAvailableCallback>
        builder_and_callback = std::move(pending_requests_.front());
    pending_requests_.pop();
    StartRequest(std::move(builder_and_callback.first),
                 std::move(builder_and_callback.second), access_token);
  }
}

void ContextualSuggestionsFetcherImpl::AccessTokenError(
    const GoogleServiceAuthError& error) {
  DCHECK_NE(error.state(), GoogleServiceAuthError::NONE);

  LOG(WARNING) << "ContextualSuggestionsFetcherImpl::AccessTokenError "
                  "Unable to get token: "
               << error.ToString();

  while (!pending_requests_.empty()) {
    std::pair<ContextualJsonRequest::Builder, SuggestionsAvailableCallback>
        builder_and_callback = std::move(pending_requests_.front());

    FetchFinished(OptionalSuggestions(), std::move(builder_and_callback.second),
                  FetchResult::OAUTH_TOKEN_ERROR,
                  /*error_details=*/
                  base::StringPrintf(" (%s)", error.ToString().c_str()));
    pending_requests_.pop();
  }
}

void ContextualSuggestionsFetcherImpl::JsonRequestDone(
    std::unique_ptr<ContextualJsonRequest> request,
    SuggestionsAvailableCallback callback,
    std::unique_ptr<base::Value> result,
    FetchResult status_code,
    const std::string& error_details) {
  DCHECK(request);

  DVLOG(1) << "ContextualSuggestionsFetcherImpl::JsonRequestDone status_code="
           << static_cast<int>(status_code)
           << " error_details=" << error_details;
  last_fetch_json_ = request->GetResponseString();

  // TODO(gaschler): Add UMA metrics for fetch duration of the request

  if (!result) {
    FetchFinished(OptionalSuggestions(), std::move(callback), status_code,
                  error_details);
    return;
  }

  OptionalSuggestions optional_suggestions = RemoteSuggestion::PtrVector();
  if (!JsonToSuggestions(*result, &optional_suggestions.value())) {
    DLOG(WARNING) << "Received invalid suggestions: " << last_fetch_json_;
    FetchFinished(OptionalSuggestions(), std::move(callback),
                  FetchResult::INVALID_SNIPPET_CONTENT_ERROR, std::string());
    return;
  }

  FetchFinished(std::move(optional_suggestions), std::move(callback),
                FetchResult::SUCCESS, std::string());
}

void ContextualSuggestionsFetcherImpl::FetchFinished(
    OptionalSuggestions optional_suggestions,
    SuggestionsAvailableCallback callback,
    FetchResult fetch_result,
    const std::string& error_details) {
  DCHECK(fetch_result == FetchResult::SUCCESS ||
         !optional_suggestions.has_value());

  last_status_ = FetchResultToString(fetch_result) + error_details;

  DVLOG(1) << "Fetch finished: " << last_status_;

  std::move(callback).Run(FetchResultToStatus(fetch_result),
                          std::move(optional_suggestions));
}

bool ContextualSuggestionsFetcherImpl::JsonToSuggestions(
    const base::Value& parsed,
    RemoteSuggestion::PtrVector* suggestions) {
  const base::DictionaryValue* top_dict = nullptr;
  if (!parsed.GetAsDictionary(&top_dict)) {
    return false;
  }

  const base::ListValue* categories_value = nullptr;
  if (!top_dict->GetList("categories", &categories_value)) {
    return false;
  }

  for (const auto& v : *categories_value) {
    const base::DictionaryValue* category_value = nullptr;
    if (!(v.GetAsDictionary(&category_value))) {
      return false;
    }

    const base::ListValue* suggestions_list = nullptr;
    // Absence of a list of suggestions is treated as an empty list, which
    // is permissible.
    if (category_value->GetList("suggestions", &suggestions_list)) {
      if (!AddSuggestionsFromListValue(true, *suggestions_list, suggestions)) {
        return false;
      }
    }
  }

  return true;
}

}  // namespace ntp_snippets
