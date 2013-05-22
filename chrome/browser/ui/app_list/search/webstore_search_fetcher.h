// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_WEBSTORE_SEARCH_FETCHER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_WEBSTORE_SEARCH_FETCHER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace base {
class DictionaryValue;
class Value;
}

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

namespace app_list {

// A class to fetch web store search result.
class WebstoreSearchFetcher : public net::URLFetcherDelegate {
 public:
  // Callback to pass back the parsed json dictionary returned from the server.
  // Invoked with NULL if there is an error.
  typedef base::Callback<void(scoped_ptr<base::DictionaryValue>)> Callback;

  WebstoreSearchFetcher(const Callback& callback,
                        net::URLRequestContextGetter* context_getter);
  virtual ~WebstoreSearchFetcher();

  // Starts to fetch results for the given |query| and the language code |hl|.
  void Start(const std::string& query, const std::string& hl);
  void Stop();

 private:
  // Callbacks for SafeJsonParser.
  void OnJsonParseSuccess(scoped_ptr<base::Value> parsed_json);
  void OnJsonParseError(const std::string& error);

  // net::URLFetcherDelegate overrides:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  Callback callback_;
  net::URLRequestContextGetter* context_getter_;

  scoped_ptr<net::URLFetcher> fetcher_;
  base::WeakPtrFactory<WebstoreSearchFetcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebstoreSearchFetcher);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_WEBSTORE_SEARCH_FETCHER_H_
