// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_WEBSTORE_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_WEBSTORE_PROVIDER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/app_list/search/common/webservice_search_provider.h"
#include "chrome/browser/ui/app_list/search/webstore_cache.h"

class AppListControllerDelegate;
class Profile;

namespace base {
class DictionaryValue;
}

namespace app_list {

namespace test {
class WebstoreProviderTest;
}

class ChromeSearchResult;
class JSONResponseFetcher;

// WebstoreProvider fetches search results from the web store server.
// A "Search in web store" result will be returned if the server does not
// return any results.
class WebstoreProvider : public WebserviceSearchProvider{
 public:
  WebstoreProvider(Profile* profile, AppListControllerDelegate* controller);
  virtual ~WebstoreProvider();

  // SearchProvider overrides:
  virtual void Start(const base::string16& query) OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  friend class app_list::test::WebstoreProviderTest;

  // Start the search request with |query_|.
  void StartQuery();

  void OnWebstoreSearchFetched(scoped_ptr<base::DictionaryValue> json);
  void ProcessWebstoreSearchResults(const base::DictionaryValue* json);
  scoped_ptr<ChromeSearchResult> CreateResult(
      const base::DictionaryValue& dict);

  void set_webstore_search_fetched_callback(const base::Closure& callback) {
    webstore_search_fetched_callback_ = callback;
  }

  Profile* profile_;
  AppListControllerDelegate* controller_;
  scoped_ptr<JSONResponseFetcher> webstore_search_;
  base::Closure webstore_search_fetched_callback_;

  // The cache of the search result which will be valid only in a single
  // input session.
  WebstoreCache cache_;

  // The timestamp when the last key event happened.
  base::Time last_keytyped_;

  // The timer to throttle QPS to the webstore search .
  base::OneShotTimer<WebstoreProvider> query_throttler_;

  // The current query.
  std::string query_;

  DISALLOW_COPY_AND_ASSIGN(WebstoreProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_WEBSTORE_PROVIDER_H_
