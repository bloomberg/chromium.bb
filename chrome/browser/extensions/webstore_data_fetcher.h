// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_DATA_FETCHER_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_DATA_FETCHER_H_

#include <memory>
#include <string>

#include "base/macros.h"
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
  ~WebstoreDataFetcher() override;

  // Makes this request use a POST instead of GET, and sends |json| in the
  // body of the request. If |json| is empty, this is a no-op.
  void SetJsonPostData(const std::string& json);

  void Start();

  void set_max_auto_retries(int max_retries) {
    max_auto_retries_ = max_retries;
  }

 private:
  void OnJsonParseSuccess(std::unique_ptr<base::Value> parsed_json);
  void OnJsonParseFailure(const std::string& error);

  // net::URLFetcherDelegate overrides:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  WebstoreDataFetcherDelegate* delegate_;
  net::URLRequestContextGetter* request_context_;
  GURL referrer_url_;
  std::string id_;
  std::string json_post_data_;

  // For fetching webstore JSON data.
  std::unique_ptr<net::URLFetcher> webstore_data_url_fetcher_;

  // Maximum auto retry times on server 5xx error or ERR_NETWORK_CHANGED.
  // Default is 0 which means to use the URLFetcher default behavior.
  int max_auto_retries_;

  DISALLOW_COPY_AND_ASSIGN(WebstoreDataFetcher);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WEBSTORE_DATA_FETCHER_H_
