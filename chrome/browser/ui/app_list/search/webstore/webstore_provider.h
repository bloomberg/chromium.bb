// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_WEBSTORE_WEBSTORE_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_WEBSTORE_WEBSTORE_PROVIDER_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/app_list/search/common/webservice_search_provider.h"

class AppListControllerDelegate;

namespace base {
class DictionaryValue;
}

namespace app_list {

namespace test {
class WebstoreProviderTest;
}

class JSONResponseFetcher;
class SearchResult;

// WebstoreProvider fetches search results from the web store server.
// A "Search in web store" result will be returned if the server does not
// return any results.
class WebstoreProvider : public WebserviceSearchProvider{
 public:
  WebstoreProvider(Profile* profile, AppListControllerDelegate* controller);
  ~WebstoreProvider() override;

  // SearchProvider overrides:
  void Start(bool is_voice_query, const base::string16& query) override;
  void Stop() override;

 private:
  friend class app_list::test::WebstoreProviderTest;

  // Start the search request with |query_|.
  void StartQuery();

  void OnWebstoreSearchFetched(scoped_ptr<base::DictionaryValue> json);
  void ProcessWebstoreSearchResults(const base::DictionaryValue* json);
  scoped_ptr<SearchResult> CreateResult(const base::DictionaryValue& dict);

  void set_webstore_search_fetched_callback(const base::Closure& callback) {
    webstore_search_fetched_callback_ = callback;
  }

  AppListControllerDelegate* controller_;
  scoped_ptr<JSONResponseFetcher> webstore_search_;
  base::Closure webstore_search_fetched_callback_;

  // The current query.
  std::string query_;

  DISALLOW_COPY_AND_ASSIGN(WebstoreProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_WEBSTORE_WEBSTORE_PROVIDER_H_
