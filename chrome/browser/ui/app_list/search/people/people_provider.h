// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_PEOPLE_PEOPLE_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_PEOPLE_PEOPLE_PROVIDER_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/app_list/search/common/webservice_search_provider.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "url/gurl.h"

class AppListControllerDelegate;

namespace base {
class DictionaryValue;
}

namespace app_list {

namespace test {
class PeopleProviderTest;
}

class ChromeSearchResult;
class JSONResponseFetcher;

// PeopleProvider fetches search results from the web store server.
// A "Search in web store" result will be returned if the server does not
// return any results.
class PeopleProvider : public WebserviceSearchProvider,
                       public OAuth2TokenService::Consumer {
 public:
  explicit PeopleProvider(Profile* profile);
  virtual ~PeopleProvider();

  // SearchProvider overrides:
  virtual void Start(const base::string16& query) OVERRIDE;
  virtual void Stop() OVERRIDE;

  // OAuth2TokenService::Consumer overrides:
  virtual void OnGetTokenSuccess(
      const OAuth2TokenService::Request* request,
      const std::string& access_token,
      const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(
      const OAuth2TokenService::Request* request,
      const GoogleServiceAuthError& error) OVERRIDE;

 private:
  friend class app_list::test::PeopleProviderTest;

  // Start a request for getting the access token for people search.
  void RequestAccessToken();
  // Invalidate our existing token so a new one can be fetched.
  void InvalidateToken();

  // Get the full people search query URL. This URL includes the OAuth refresh
  // token for authenticating the current user.
  GURL GetQueryUrl(const std::string& query);

  // Start the search request with |query_|.
  void StartQuery();

  // Callback for people search results being fetched.
  void OnPeopleSearchFetched(scoped_ptr<base::DictionaryValue> json);
  void ProcessPeopleSearchResults(const base::DictionaryValue* json);
  scoped_ptr<ChromeSearchResult> CreateResult(
      const base::DictionaryValue& dict);

  // Setup the various variables that we override for testing.
  void SetupForTest(const base::Closure& people_search_fetched_callback,
                    const GURL& people_search_url);

  scoped_ptr<JSONResponseFetcher> people_search_;
  base::Closure people_search_fetched_callback_;

  std::string access_token_;
  scoped_ptr<OAuth2TokenService::Request> access_token_request_;
  OAuth2TokenService::ScopeSet oauth2_scope_;

  // The current query.
  std::string query_;
  GURL people_search_url_;

  bool skip_request_token_for_test_;

  DISALLOW_COPY_AND_ASSIGN(PeopleProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_PEOPLE_PEOPLE_PROVIDER_H_
