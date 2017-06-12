// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/remote_suggestions_fetcher.h"

#include <cstdlib>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/remote/request_params.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/signin/core/browser/access_token_fetcher.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/strings/grit/components_strings.h"
#include "components/variations/variations_associated_data.h"
#include "net/url_request/url_fetcher.h"
#include "ui/base/l10n/l10n_util.h"

using net::URLFetcher;
using net::URLRequestContextGetter;
using net::HttpRequestHeaders;
using net::URLRequestStatus;
using translate::LanguageModel;

namespace ntp_snippets {

using internal::JsonRequest;
using internal::FetchResult;

namespace {

const char kContentSuggestionsApiScope[] =
    "https://www.googleapis.com/auth/chrome-content-suggestions";
const char kSnippetsServerNonAuthorizedFormat[] = "%s?key=%s";
const char kAuthorizationRequestHeaderFormat[] = "Bearer %s";

// Variation parameter for chrome-content-suggestions backend.
const char kContentSuggestionsBackend[] = "content_suggestions_backend";

const int kFetchTimeHistogramResolution = 5;

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

int GetMinuteOfTheDay(bool local_time,
                      bool reduced_resolution,
                      base::Clock* clock) {
  base::Time now(clock->Now());
  base::Time::Exploded now_exploded{};
  local_time ? now.LocalExplode(&now_exploded) : now.UTCExplode(&now_exploded);
  int now_minute = reduced_resolution
                       ? now_exploded.minute / kFetchTimeHistogramResolution *
                             kFetchTimeHistogramResolution
                       : now_exploded.minute;
  return now_exploded.hour * 60 + now_minute;
}

// The response from the backend might include suggestions from multiple
// categories. If only a single category was requested, this function filters
// all other categories out.
void FilterCategories(FetchedCategoriesVector* categories,
                      base::Optional<Category> exclusive_category) {
  if (!exclusive_category.has_value()) {
    return;
  }
  Category exclusive = exclusive_category.value();
  auto category_it =
      std::find_if(categories->begin(), categories->end(),
                   [&exclusive](const FetchedCategory& c) -> bool {
                     return c.category == exclusive;
                   });
  if (category_it == categories->end()) {
    categories->clear();
    return;
  }
  FetchedCategory category = std::move(*category_it);
  categories->clear();
  categories->push_back(std::move(category));
}

}  // namespace

GURL GetFetchEndpoint(version_info::Channel channel) {
  std::string endpoint = variations::GetVariationParamValueByFeature(
      ntp_snippets::kArticleSuggestionsFeature, kContentSuggestionsBackend);
  if (!endpoint.empty()) {
    return GURL{endpoint};
  }

  switch (channel) {
    case version_info::Channel::STABLE:
    case version_info::Channel::BETA:
      return GURL{kContentSuggestionsServer};

    case version_info::Channel::DEV:
    case version_info::Channel::CANARY:
    case version_info::Channel::UNKNOWN:
      return GURL{kContentSuggestionsStagingServer};
  }
  NOTREACHED();
  return GURL{kContentSuggestionsStagingServer};
}

RemoteSuggestionsFetcher::RemoteSuggestionsFetcher(
    SigninManagerBase* signin_manager,
    OAuth2TokenService* token_service,
    scoped_refptr<URLRequestContextGetter> url_request_context_getter,
    PrefService* pref_service,
    LanguageModel* language_model,
    const ParseJSONCallback& parse_json_callback,
    const GURL& api_endpoint,
    const std::string& api_key,
    const UserClassifier* user_classifier)
    : signin_manager_(signin_manager),
      token_service_(token_service),
      url_request_context_getter_(std::move(url_request_context_getter)),
      language_model_(language_model),
      parse_json_callback_(parse_json_callback),
      fetch_url_(api_endpoint),
      api_key_(api_key),
      clock_(new base::DefaultClock()),
      user_classifier_(user_classifier) {}

RemoteSuggestionsFetcher::~RemoteSuggestionsFetcher() = default;

void RemoteSuggestionsFetcher::FetchSnippets(
    const RequestParams& params,
    SnippetsAvailableCallback callback) {
  if (!params.interactive_request) {
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "NewTabPage.Snippets.FetchTimeLocal",
        GetMinuteOfTheDay(/*local_time=*/true,
                          /*reduced_resolution=*/true, clock_.get()));
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "NewTabPage.Snippets.FetchTimeUTC",
        GetMinuteOfTheDay(/*local_time=*/false,
                          /*reduced_resolution=*/true, clock_.get()));
  }

  JsonRequest::Builder builder;
  builder.SetLanguageModel(language_model_)
      .SetParams(params)
      .SetParseJsonCallback(parse_json_callback_)
      .SetClock(clock_.get())
      .SetUrlRequestContextGetter(url_request_context_getter_)
      .SetUserClassifier(*user_classifier_);

  if (signin_manager_->IsAuthenticated() || signin_manager_->AuthInProgress()) {
    // Signed-in: get OAuth token --> fetch suggestions.
    pending_requests_.emplace(std::move(builder), std::move(callback));
    StartTokenRequest();
  } else {
    // Not signed in: fetch suggestions (without authentication).
    FetchSnippetsNonAuthenticated(std::move(builder), std::move(callback));
  }
}

void RemoteSuggestionsFetcher::FetchSnippetsNonAuthenticated(
    JsonRequest::Builder builder,
    SnippetsAvailableCallback callback) {
  if (api_key_.empty()) {
    // If we don't have an API key, don't even try.
    FetchFinished(OptionalFetchedCategories(), std::move(callback),
                  FetchResult::MISSING_API_KEY, std::string());
    return;
  }
  // When not providing OAuth token, we need to pass the Google API key.
  builder.SetUrl(
      GURL(base::StringPrintf(kSnippetsServerNonAuthorizedFormat,
                              fetch_url_.spec().c_str(), api_key_.c_str())));
  StartRequest(std::move(builder), std::move(callback));
}

void RemoteSuggestionsFetcher::FetchSnippetsAuthenticated(
    JsonRequest::Builder builder,
    SnippetsAvailableCallback callback,
    const std::string& oauth_access_token) {
  // TODO(jkrcal, treib): Add unit-tests for authenticated fetches.
  builder.SetUrl(fetch_url_)
      .SetAuthentication(signin_manager_->GetAuthenticatedAccountId(),
                         base::StringPrintf(kAuthorizationRequestHeaderFormat,
                                            oauth_access_token.c_str()));
  StartRequest(std::move(builder), std::move(callback));
}

void RemoteSuggestionsFetcher::StartRequest(
    JsonRequest::Builder builder,
    SnippetsAvailableCallback callback) {
  std::unique_ptr<JsonRequest> request = builder.Build();
  JsonRequest* raw_request = request.get();
  raw_request->Start(base::BindOnce(&RemoteSuggestionsFetcher::JsonRequestDone,
                                    base::Unretained(this), std::move(request),
                                    std::move(callback)));
}

void RemoteSuggestionsFetcher::StartTokenRequest() {
  // If there is already an ongoing token request, just wait for that.
  if (token_fetcher_) {
    return;
  }

  OAuth2TokenService::ScopeSet scopes{kContentSuggestionsApiScope};
  token_fetcher_ = base::MakeUnique<AccessTokenFetcher>(
      "ntp_snippets", signin_manager_, token_service_, scopes,
      base::BindOnce(&RemoteSuggestionsFetcher::AccessTokenFetchFinished,
                     base::Unretained(this)));
}

void RemoteSuggestionsFetcher::AccessTokenFetchFinished(
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
    std::pair<JsonRequest::Builder, SnippetsAvailableCallback>
        builder_and_callback = std::move(pending_requests_.front());
    pending_requests_.pop();
    FetchSnippetsAuthenticated(std::move(builder_and_callback.first),
                               std::move(builder_and_callback.second),
                               access_token);
  }
}

void RemoteSuggestionsFetcher::AccessTokenError(
    const GoogleServiceAuthError& error) {
  DCHECK_NE(error.state(), GoogleServiceAuthError::NONE);

  DLOG(ERROR) << "Unable to get token: " << error.ToString();

  while (!pending_requests_.empty()) {
    std::pair<JsonRequest::Builder, SnippetsAvailableCallback>
        builder_and_callback = std::move(pending_requests_.front());

    FetchFinished(OptionalFetchedCategories(),
                  std::move(builder_and_callback.second),
                  FetchResult::OAUTH_TOKEN_ERROR,
                  /*error_details=*/
                  base::StringPrintf(" (%s)", error.ToString().c_str()));
    pending_requests_.pop();
  }
}

void RemoteSuggestionsFetcher::JsonRequestDone(
    std::unique_ptr<JsonRequest> request,
    SnippetsAvailableCallback callback,
    std::unique_ptr<base::Value> result,
    FetchResult status_code,
    const std::string& error_details) {
  DCHECK(request);
  // Record the time when request for fetching remote content snippets finished.
  const base::Time fetch_time = clock_->Now();

  last_fetch_json_ = request->GetResponseString();

  UMA_HISTOGRAM_TIMES("NewTabPage.Snippets.FetchTime",
                      request->GetFetchDuration());

  if (!result) {
    FetchFinished(OptionalFetchedCategories(), std::move(callback), status_code,
                  error_details);
    return;
  }

  FetchedCategoriesVector categories;
  if (!JsonToCategories(*result, &categories, fetch_time)) {
    LOG(WARNING) << "Received invalid snippets: " << last_fetch_json_;
    FetchFinished(OptionalFetchedCategories(), std::move(callback),
                  FetchResult::INVALID_SNIPPET_CONTENT_ERROR, std::string());
    return;
  }
  // Filter out unwanted categories if necessary.
  // TODO(fhorschig): As soon as the server supports filtering by category,
  // adjust the request instead of over-fetching and filtering here.
  FilterCategories(&categories, request->exclusive_category());

  FetchFinished(std::move(categories), std::move(callback),
                FetchResult::SUCCESS, std::string());
}

void RemoteSuggestionsFetcher::FetchFinished(
    OptionalFetchedCategories categories,
    SnippetsAvailableCallback callback,
    FetchResult fetch_result,
    const std::string& error_details) {
  DCHECK(fetch_result == FetchResult::SUCCESS || !categories.has_value());

  last_status_ = FetchResultToString(fetch_result) + error_details;

  UMA_HISTOGRAM_ENUMERATION("NewTabPage.Snippets.FetchResult",
                            static_cast<int>(fetch_result),
                            static_cast<int>(FetchResult::RESULT_MAX));

  DVLOG(1) << "Fetch finished: " << last_status_;

  std::move(callback).Run(FetchResultToStatus(fetch_result),
                          std::move(categories));
}

}  // namespace ntp_snippets
