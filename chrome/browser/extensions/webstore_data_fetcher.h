// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_DATA_FETCHER_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_DATA_FETCHER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace base {
class Value;
}

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

namespace extensions {

class WebstoreDataFetcherDelegate;

// WebstoreDataFetcher fetches web store data and parses it into a
// DictionaryValue.
class WebstoreDataFetcher : public base::SupportsWeakPtr<WebstoreDataFetcher>,
                            public net::URLFetcherDelegate {
 public:
  WebstoreDataFetcher(WebstoreDataFetcherDelegate* delegate,
                      net::URLRequestContextGetter* request_context,
                      const GURL& referrer_url,
                      const std::string webstore_item_id);
  virtual ~WebstoreDataFetcher();

  void Start();

  void set_max_auto_retries(int max_retries) {
    max_auto_retries_ = max_retries;
  }

 private:
  void OnJsonParseSuccess(scoped_ptr<base::Value> parsed_json);
  void OnJsonParseFailure(const std::string& error);

  // net::URLFetcherDelegate overrides:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  WebstoreDataFetcherDelegate* delegate_;
  net::URLRequestContextGetter* request_context_;
  GURL referrer_url_;
  std::string id_;

  // For fetching webstore JSON data.
  scoped_ptr<net::URLFetcher> webstore_data_url_fetcher_;

  // Maximum auto retry times on server 5xx error or ERR_NETWORK_CHANGED.
  // Default is 0 which means to use the URLFetcher default behavior.
  int max_auto_retries_;

  DISALLOW_COPY_AND_ASSIGN(WebstoreDataFetcher);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WEBSTORE_DATA_FETCHER_H_
