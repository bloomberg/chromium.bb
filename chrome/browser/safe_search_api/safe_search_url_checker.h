// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_SEARCH_API_SAFE_SEARCH_URL_CHECKER_H_
#define CHROME_BROWSER_SAFE_SEARCH_API_SAFE_SEARCH_URL_CHECKER_H_

#include <stddef.h>

#include "base/callback_forward.h"
#include "base/containers/mru_cache.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

// This class uses the SafeSearch API to check the SafeSearch classification
// of the content on a given URL and returns the result asynchronously
// via a callback.
class SafeSearchURLChecker : net::URLFetcherDelegate {
 public:
  enum class Classification { SAFE, UNSAFE };

  // Returns whether |url| should be blocked. Called from CheckURL.
  using CheckCallback = base::Callback<
      void(const GURL&, Classification classification, bool /* uncertain */)>;

  explicit SafeSearchURLChecker(net::URLRequestContextGetter* context);
  SafeSearchURLChecker(net::URLRequestContextGetter* context,
                       size_t cache_size);
  ~SafeSearchURLChecker() override;

  // Returns whether |callback| was run synchronously.
  bool CheckURL(const GURL& url, const CheckCallback& callback);

  void SetCacheTimeoutForTesting(const base::TimeDelta& timeout) {
    cache_timeout_ = timeout;
  }

 private:
  struct Check;
  struct CheckResult {
    CheckResult(Classification classification, bool uncertain);
    Classification classification;
    bool uncertain;
    base::TimeTicks timestamp;
  };

  // net::URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  net::URLRequestContextGetter* context_;

  ScopedVector<Check> checks_in_progress_;

  base::MRUCache<GURL, CheckResult> cache_;
  base::TimeDelta cache_timeout_;

  DISALLOW_COPY_AND_ASSIGN(SafeSearchURLChecker);
};

#endif  // CHROME_BROWSER_SAFE_SEARCH_API_SAFE_SEARCH_URL_CHECKER_H_
