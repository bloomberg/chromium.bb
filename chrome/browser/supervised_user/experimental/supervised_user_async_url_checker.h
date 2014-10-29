// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_EXPERIMENTAL_SUPERVISED_USER_ASYNC_URL_CHECKER_H_
#define CHROME_BROWSER_SUPERVISED_USER_EXPERIMENTAL_SUPERVISED_USER_ASYNC_URL_CHECKER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/containers/mru_cache.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

// This class checks against an online service (currently a Custom Search
// Engine using SafeSearch) whether a given URL is safe to visit for a
// supervised user, and returns the result asynchronously via a callback.
class SupervisedUserAsyncURLChecker : net::URLFetcherDelegate {
 public:
  // Returns whether |url| should be blocked. Called from CheckURL.
  typedef base::Callback<void(const GURL&,
                              SupervisedUserURLFilter::FilteringBehavior,
                              bool /* uncertain */)>
      CheckCallback;

  SupervisedUserAsyncURLChecker(net::URLRequestContextGetter* context,
                                const std::string& cx,
                                const std::string& api_key);
  SupervisedUserAsyncURLChecker(net::URLRequestContextGetter* context,
                                const std::string& cx,
                                const std::string& api_key,
                                size_t cache_size);
  virtual ~SupervisedUserAsyncURLChecker();

  // Returns whether |callback| was run synchronously.
  bool CheckURL(const GURL& url, const CheckCallback& callback);

 private:
  struct Check;
  struct CheckResult {
    CheckResult(SupervisedUserURLFilter::FilteringBehavior behavior,
                bool uncertain);
    SupervisedUserURLFilter::FilteringBehavior behavior;
    bool uncertain;
  };

  void SetApiKey(const std::string& api_key);

  // net::URLFetcherDelegate implementation.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) override;

  net::URLRequestContextGetter* context_;
  std::string cx_;
  std::string api_key_;

  ScopedVector<Check> checks_in_progress_;

  base::MRUCache<GURL, CheckResult> cache_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserAsyncURLChecker);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_EXPERIMENTAL_SUPERVISED_USER_ASYNC_URL_CHECKER_H_
