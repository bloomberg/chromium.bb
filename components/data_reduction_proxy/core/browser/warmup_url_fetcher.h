// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_WARMUP_URL_FETCHER_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_WARMUP_URL_FETCHER_H_

#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/url_request/url_fetcher_delegate.h"

class GURL;

namespace net {

class URLFetcher;
class URLRequestContextGetter;

}  // namespace net

namespace data_reduction_proxy {

// URLFetcherDelegate for fetching the warmup URL.
class WarmupURLFetcher : public net::URLFetcherDelegate {
 public:
  explicit WarmupURLFetcher(const scoped_refptr<net::URLRequestContextGetter>&
                                url_request_context_getter);

  ~WarmupURLFetcher() override;

  // Creates and starts a URLFetcher that fetches the warmup URL.
  void FetchWarmupURL();

 protected:
  // Sets |warmup_url_with_query_params| to the warmup URL. Attaches random
  // query params to the warmup URL.
  void GetWarmupURLWithQueryParam(GURL* warmup_url_with_query_params) const;

 private:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  // The URLFetcher being used for fetching the warmup URL.
  std::unique_ptr<net::URLFetcher> fetcher_;

  DISALLOW_COPY_AND_ASSIGN(WarmupURLFetcher);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_WARMUP_URL_FETCHER_H_