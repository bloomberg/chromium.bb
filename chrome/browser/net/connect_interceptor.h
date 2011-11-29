// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CONNECT_INTERCEPTOR_H_
#define CHROME_BROWSER_NET_CONNECT_INTERCEPTOR_H_
#pragma once

#include "base/gtest_prod_util.h"
#include "base/memory/mru_cache.h"
#include "base/time.h"
#include "net/url_request/url_request_job_factory.h"

namespace chrome_browser_net {

class Predictor;

//------------------------------------------------------------------------------
// An interceptor to monitor URLRequests so that we can do speculative DNS
// resolution and/or speculative TCP preconnections.
class ConnectInterceptor : public net::URLRequestJobFactory::Interceptor {
 public:
  // Construction includes registration as an URL.
  explicit ConnectInterceptor(Predictor* predictor);
  // Destruction includes unregistering.
  virtual ~ConnectInterceptor();

 protected:
  // Overridden from net::URLRequest::Interceptor:
  // Learn about referrers, and optionally preconnect based on history.
  virtual net::URLRequestJob* MaybeIntercept(
      net::URLRequest* request) const OVERRIDE;
  virtual net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request) const OVERRIDE;
  virtual net::URLRequestJob* MaybeInterceptRedirect(
      const GURL& location, net::URLRequest* request) const OVERRIDE;

 private:
  // Provide access to local class TimedCache for testing.
  FRIEND_TEST_ALL_PREFIXES(ConnectInterceptorTest, TimedCacheRecall);
  FRIEND_TEST_ALL_PREFIXES(ConnectInterceptorTest, TimedCacheEviction);

  // Define a LRU cache that recalls all navigations within the last N seconds.
  // When we learn about subresources to possibly preconnect to, it would be a
  // waste to preconnect when the original navigation was too long ago.  Any
  // connected, but unused TCP/IP connection, will generally be reset by the
  // server if it is not used quickly (i.e., GET or POST is sent).
  class TimedCache {
   public:
    explicit TimedCache(const base::TimeDelta& max_duration);
    ~TimedCache();

    // Evicts any entries that have been in the FIFO "too long," and then checks
    // to see if the given url is (still) in the FIFO cache.
    bool WasRecentlySeen(const GURL& url) const;

    // Adds the given url to the cache, where it will remain for max_duration_.
    void SetRecentlySeen(const GURL& url) const;

   private:
    // Our cache will be keyed on a URL (actually, just a scheme/host/port).
    // We will always track the time it was last added to the FIFO cache by
    // remembering a TimeTicks value.
    typedef base::MRUCache<GURL, base::TimeTicks> UrlMruTimedCache;
    // mru_cache_ has to be mutable in order to be accessed from the overriden
    // URLRequestJob functions. It is mutable because it tracks the urls and
    // caches them.
    mutable UrlMruTimedCache mru_cache_;

    // The longest time an entry can persist in the cache, and still be found.
    const base::TimeDelta max_duration_;

    DISALLOW_COPY_AND_ASSIGN(TimedCache);
  };
  TimedCache timed_cache_;
  Predictor* const predictor_;

  DISALLOW_COPY_AND_ASSIGN(ConnectInterceptor);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_CONNECT_INTERCEPTOR_H_
