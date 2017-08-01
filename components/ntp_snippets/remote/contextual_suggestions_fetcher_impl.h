// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_CONTEXTUAL_SUGGESTIONS_FETCHER_IMPL_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_CONTEXTUAL_SUGGESTIONS_FETCHER_IMPL_H_

#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"
#include "base/time/clock.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_info.h"
#include "components/ntp_snippets/remote/contextual_json_request.h"
#include "components/ntp_snippets/remote/contextual_suggestions_fetcher.h"
#include "components/ntp_snippets/remote/remote_suggestion.h"
#include "components/ntp_snippets/status.h"
#include "net/url_request/url_request_context_getter.h"

class AccessTokenFetcher;
class OAuth2TokenService;
class PrefService;
class SigninManagerBase;

namespace ntp_snippets {

// TODO(gaschler): Move authentication that is in common with
// RemoteSuggestionsFetcher to a helper class.
class ContextualSuggestionsFetcherImpl : public ContextualSuggestionsFetcher {
 public:
  ContextualSuggestionsFetcherImpl(
      SigninManagerBase* signin_manager,
      OAuth2TokenService* token_service,
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
      PrefService* pref_service,
      const ParseJSONCallback& parse_json_callback);
  ~ContextualSuggestionsFetcherImpl() override;

  // ContextualSuggestionsFetcher implementation.
  void FetchContextualSuggestions(
      const GURL& url,
      SuggestionsAvailableCallback callback) override;

  const std::string& GetLastStatusForTesting() const override;
  const std::string& GetLastJsonForTesting() const override;
  const GURL& GetFetchUrlForTesting() const override;

 private:
  void StartRequest(internal::ContextualJsonRequest::Builder builder,
                    SuggestionsAvailableCallback callback,
                    const std::string& oauth_access_token);

  void StartTokenRequest();

  void AccessTokenFetchFinished(const GoogleServiceAuthError& error,
                                const std::string& access_token);
  void AccessTokenError(const GoogleServiceAuthError& error);

  void JsonRequestDone(std::unique_ptr<internal::ContextualJsonRequest> request,
                       SuggestionsAvailableCallback callback,
                       std::unique_ptr<base::Value> result,
                       internal::FetchResult status_code,
                       const std::string& error_details);
  void FetchFinished(OptionalSuggestions optional_suggestions,
                     SuggestionsAvailableCallback callback,
                     internal::FetchResult status_code,
                     const std::string& error_details);

  bool JsonToSuggestions(const base::Value& parsed,
                         RemoteSuggestion::PtrVector* suggestions);

  // Authentication for signed-in users.
  SigninManagerBase* signin_manager_;
  OAuth2TokenService* token_service_;

  std::unique_ptr<AccessTokenFetcher> token_fetcher_;

  // Holds the URL request context.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  // Stores requests that wait for an access token.
  std::queue<std::pair<internal::ContextualJsonRequest::Builder,
                       SuggestionsAvailableCallback>>
      pending_requests_;

  const ParseJSONCallback parse_json_callback_;

  // API endpoint for fetching contextual suggestions.
  const GURL fetch_url_;

  // Info on the last finished fetch.
  std::string last_status_;
  std::string last_fetch_json_;

  DISALLOW_COPY_AND_ASSIGN(ContextualSuggestionsFetcherImpl);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_CONTEXTUAL_SUGGESTIONS_FETCHER_IMPL_H_
