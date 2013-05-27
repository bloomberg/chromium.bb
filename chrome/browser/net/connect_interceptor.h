// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CONNECT_INTERCEPTOR_H_
#define CHROME_BROWSER_NET_CONNECT_INTERCEPTOR_H_

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/net/timed_cache.h"

class GURL;

namespace net {
class URLRequest;
}

namespace chrome_browser_net {

class Predictor;

//------------------------------------------------------------------------------
// An interceptor to monitor URLRequests so that we can do speculative DNS
// resolution and/or speculative TCP preconnections.
class ConnectInterceptor {
 public:
  // Construction includes registration as an URL.
  explicit ConnectInterceptor(Predictor* predictor);
  // Destruction includes unregistering.
  virtual ~ConnectInterceptor();

  // Learn about referrers, and optionally preconnect based on history.
  void WitnessURLRequest(net::URLRequest* request);

 private:
  // Provide access to local class TimedCache for testing.
  FRIEND_TEST_ALL_PREFIXES(ConnectInterceptorTest, TimedCacheRecall);
  FRIEND_TEST_ALL_PREFIXES(ConnectInterceptorTest, TimedCacheEviction);

  TimedCache timed_cache_;
  Predictor* const predictor_;

  DISALLOW_COPY_AND_ASSIGN(ConnectInterceptor);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_CONNECT_INTERCEPTOR_H_
