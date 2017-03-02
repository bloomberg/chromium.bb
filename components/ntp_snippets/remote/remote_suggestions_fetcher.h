// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_FETCHER_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_FETCHER_H_

#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_info.h"
#include "components/ntp_snippets/remote/json_request.h"
#include "components/ntp_snippets/remote/remote_suggestion.h"
#include "components/ntp_snippets/remote/request_params.h"
#include "components/ntp_snippets/status.h"
#include "components/translate/core/browser/language_model.h"
#include "components/version_info/version_info.h"
#include "net/url_request/url_request_context_getter.h"

class PrefService;
class SigninManagerBase;

namespace base {
class Value;
}  // namespace base

namespace ntp_snippets {

class UserClassifier;

// Returns the appropriate API endpoint for the fetcher, in consideration of
// the channel and variation parameters.
GURL GetFetchEndpoint(version_info::Channel channel);

// TODO(tschumann): BuildArticleCategoryInfo() and BuildRemoteCategoryInfo()
// don't really belong into this library. However, as the fetcher is
// providing this data for server-defined remote sections it's a good starting
// point. Candiates to add to such a library would be persisting categories
// (have all category managment in one place) or turning parsed JSON into
// FetchedCategory objects (all domain-specific logic in one place).

// Provides the CategoryInfo data for article suggestions. If |title| is
// nullopt, then the default, hard-coded title will be used.
CategoryInfo BuildArticleCategoryInfo(
    const base::Optional<base::string16>& title);

// Provides the CategoryInfo data for other remote suggestions.
CategoryInfo BuildRemoteCategoryInfo(const base::string16& title,
                                     bool allow_fetching_more_results);

// Fetches suggestion data for the NTP from the server.
// TODO(fhorschig): Untangle cyclic dependencies by introducing a
// RemoteSuggestionsFetcherInterface. (Would be good for mock implementations,
// too!)
class RemoteSuggestionsFetcher : public OAuth2TokenService::Consumer,
                                 public OAuth2TokenService::Observer {
 public:
  struct FetchedCategory {
    Category category;
    CategoryInfo info;
    RemoteSuggestion::PtrVector suggestions;

    FetchedCategory(Category c, CategoryInfo&& info);
    FetchedCategory(FetchedCategory&&);             // = default, in .cc
    ~FetchedCategory();                             // = default, in .cc
    FetchedCategory& operator=(FetchedCategory&&);  // = default, in .cc
  };
  using FetchedCategoriesVector = std::vector<FetchedCategory>;
  using OptionalFetchedCategories = base::Optional<FetchedCategoriesVector>;

  using SnippetsAvailableCallback =
      base::OnceCallback<void(Status status,
                              OptionalFetchedCategories fetched_categories)>;

  RemoteSuggestionsFetcher(
      SigninManagerBase* signin_manager,
      OAuth2TokenService* token_service,
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
      PrefService* pref_service,
      translate::LanguageModel* language_model,
      const ParseJSONCallback& parse_json_callback,
      const GURL& api_endpoint,
      const std::string& api_key,
      const UserClassifier* user_classifier);
  ~RemoteSuggestionsFetcher() override;

  // Initiates a fetch from the server. When done (successfully or not), calls
  // the callback.
  //
  // If an ongoing fetch exists, both fetches won't influence each other (i.e.
  // every callback will be called exactly once).
  void FetchSnippets(const RequestParams& params,
                     SnippetsAvailableCallback callback);

  // Debug string representing the status/result of the last fetch attempt.
  const std::string& last_status() const { return last_status_; }

  // Returns the last JSON fetched from the server.
  const std::string& last_json() const { return last_fetch_json_; }

  // Returns the URL endpoint used by the fetcher.
  const GURL& fetch_url() const { return fetch_url_; }

  // Overrides internal clock for testing purposes.
  void SetClockForTesting(std::unique_ptr<base::Clock> clock) {
    clock_ = std::move(clock);
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeReaderSnippetsFetcherTest,
                           BuildRequestAuthenticated);
  FRIEND_TEST_ALL_PREFIXES(ChromeReaderSnippetsFetcherTest,
                           BuildRequestUnauthenticated);
  FRIEND_TEST_ALL_PREFIXES(ChromeReaderSnippetsFetcherTest,
                           BuildRequestExcludedIds);
  FRIEND_TEST_ALL_PREFIXES(ChromeReaderSnippetsFetcherTest,
                           BuildRequestNoUserClass);
  FRIEND_TEST_ALL_PREFIXES(ChromeReaderSnippetsFetcherTest,
                           BuildRequestWithTwoLanguages);
  FRIEND_TEST_ALL_PREFIXES(ChromeReaderSnippetsFetcherTest,
                           BuildRequestWithUILanguageOnly);
  FRIEND_TEST_ALL_PREFIXES(ChromeReaderSnippetsFetcherTest,
                           BuildRequestWithOtherLanguageOnly);
  friend class ChromeReaderSnippetsFetcherTest;

  void FetchSnippetsNonAuthenticated(internal::JsonRequest::Builder builder,
                                     SnippetsAvailableCallback callback);
  void FetchSnippetsAuthenticated(internal::JsonRequest::Builder builder,
                                  SnippetsAvailableCallback callback,
                                  const std::string& account_id,
                                  const std::string& oauth_access_token);
  void StartRequest(internal::JsonRequest::Builder builder,
                    SnippetsAvailableCallback callback);

  void StartTokenRequest();

  // OAuth2TokenService::Consumer overrides:
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  // OAuth2TokenService::Observer overrides:
  void OnRefreshTokenAvailable(const std::string& account_id) override;

  void JsonRequestDone(std::unique_ptr<internal::JsonRequest> request,
                       SnippetsAvailableCallback callback,
                       std::unique_ptr<base::Value> result,
                       internal::FetchResult status_code,
                       const std::string& error_details);
  void FetchFinished(OptionalFetchedCategories categories,
                     SnippetsAvailableCallback callback,
                     internal::FetchResult status_code,
                     const std::string& error_details);

  bool JsonToSnippets(const base::Value& parsed,
                      FetchedCategoriesVector* categories,
                      const base::Time& fetch_time);

  // Authentication for signed-in users.
  SigninManagerBase* signin_manager_;
  OAuth2TokenService* token_service_;
  std::unique_ptr<OAuth2TokenService::Request> oauth_request_;
  bool waiting_for_refresh_token_ = false;

  // When a token request gets canceled, we want to retry once.
  bool oauth_token_retried_ = false;

  // Holds the URL request context.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  // Stores requests that wait for an access token.
  std::queue<
      std::pair<internal::JsonRequest::Builder, SnippetsAvailableCallback>>
      pending_requests_;

  // Weak reference, not owned.
  translate::LanguageModel* const language_model_;

  const ParseJSONCallback parse_json_callback_;

  // API endpoint for fetching suggestions.
  const GURL fetch_url_;
  // Which API to use
  const internal::FetchAPI fetch_api_;

  // API key to use for non-authenticated requests.
  const std::string api_key_;

  // Allow for an injectable clock for testing.
  std::unique_ptr<base::Clock> clock_;

  // Classifier that tells us how active the user is. Not owned.
  const UserClassifier* user_classifier_;

  // Info on the last finished fetch.
  std::string last_status_;
  std::string last_fetch_json_;

  DISALLOW_COPY_AND_ASSIGN(RemoteSuggestionsFetcher);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_FETCHER_H_
