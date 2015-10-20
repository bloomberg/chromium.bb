// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_EXPERIMENTAL_SUPERVISED_USER_ASYNC_URL_CHECKER_H_
#define CHROME_BROWSER_SUPERVISED_USER_EXPERIMENTAL_SUPERVISED_USER_ASYNC_URL_CHECKER_H_

#include "base/callback_forward.h"
#include "base/containers/mru_cache.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

// This class checks against an online service (the SafeSearch API) whether a
// given URL is safe to visit for a supervised user, and returns the result
// asynchronously via a callback.
class SupervisedUserAsyncURLChecker : net::URLFetcherDelegate {
 public:
  // Returns whether |url| should be blocked. Called from CheckURL.
  using CheckCallback =
      base::Callback<void(const GURL&,
                          SupervisedUserURLFilter::FilteringBehavior,
                          bool /* uncertain */)>;

  SupervisedUserAsyncURLChecker(net::URLRequestContextGetter* context);
  SupervisedUserAsyncURLChecker(net::URLRequestContextGetter* context,
                                size_t cache_size);
  ~SupervisedUserAsyncURLChecker() override;

  // Returns whether |callback| was run synchronously.
  bool CheckURL(const GURL& url, const CheckCallback& callback);

  void SetCacheTimeoutForTesting(const base::TimeDelta& timeout) {
    cache_timeout_ = timeout;
  }

 private:
  struct Check;
  struct CheckResult {
    CheckResult(SupervisedUserURLFilter::FilteringBehavior behavior,
                bool uncertain);
    SupervisedUserURLFilter::FilteringBehavior behavior;
    bool uncertain;
    base::TimeTicks timestamp;
  };

  // net::URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  net::URLRequestContextGetter* context_;

  ScopedVector<Check> checks_in_progress_;

  base::MRUCache<GURL, CheckResult> cache_;
  base::TimeDelta cache_timeout_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserAsyncURLChecker);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_EXPERIMENTAL_SUPERVISED_USER_ASYNC_URL_CHECKER_H_
