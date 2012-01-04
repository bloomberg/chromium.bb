// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/connect_interceptor.h"

#include "base/threading/platform_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_browser_net {

// These tests are all focused ConnectInterceptor::TimedCache.
TEST(ConnectInterceptorTest, TimedCacheRecall) {
  // Creat a cache that has a long expiration so that we can test basic recall.
  ConnectInterceptor::TimedCache cache(base::TimeDelta::FromHours(1));

  GURL url("http://google.com/anypath");
  GURL ssl_url("https://ssl_google.com/anypath");
  EXPECT_FALSE(cache.WasRecentlySeen(url));
  EXPECT_FALSE(cache.WasRecentlySeen(ssl_url));

  cache.SetRecentlySeen(url);

  EXPECT_TRUE(cache.WasRecentlySeen(url));
  EXPECT_FALSE(cache.WasRecentlySeen(ssl_url));

  cache.SetRecentlySeen(ssl_url);

  EXPECT_TRUE(cache.WasRecentlySeen(url));
  EXPECT_TRUE(cache.WasRecentlySeen(ssl_url));

  // Check that port defaults correctly in canonicalization.
  GURL url_with_port("http://google.com:80/anypath");
  GURL ssl_url_with_port("https://ssl_google.com:443/anypath");
  EXPECT_TRUE(cache.WasRecentlySeen(url_with_port));
  EXPECT_TRUE(cache.WasRecentlySeen(ssl_url_with_port));

  // Check for similar urls, to verify canonicalization isn't too generous.
  GURL ssl_url_wrong_host("https://google.com/otherpath");
  GURL ssl_url_wrong_path("https://ssl_google.com/otherpath");
  GURL ssl_url_wrong_port("https://ssl_google.com:666/anypath");
  GURL url_wrong_scheme("ftp://google.com/anypath");
  GURL url_wrong_host("http://DOODLE.com/otherpath");
  GURL url_wrong_path("http://google.com/otherpath");
  GURL url_wrong_port("http://google.com:81/anypath");

  EXPECT_FALSE(cache.WasRecentlySeen(ssl_url_wrong_host));
  EXPECT_FALSE(cache.WasRecentlySeen(ssl_url_wrong_path));
  EXPECT_FALSE(cache.WasRecentlySeen(ssl_url_wrong_port));

  EXPECT_FALSE(cache.WasRecentlySeen(url_wrong_scheme));

  EXPECT_FALSE(cache.WasRecentlySeen(url_wrong_host));
  EXPECT_FALSE(cache.WasRecentlySeen(url_wrong_path));
  EXPECT_FALSE(cache.WasRecentlySeen(url_wrong_port));
}

TEST(ConnectInterceptorTest, TimedCacheEviction) {
  // Creat a cache that has a short expiration so that we can force evictions.
  ConnectInterceptor::TimedCache cache(base::TimeDelta::FromMilliseconds(1));

  GURL url("http://google.com/anypath");
  EXPECT_FALSE(cache.WasRecentlySeen(url));

  cache.SetRecentlySeen(url);

  // Sleep at least long enough to cause an eviction.
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(30));

  EXPECT_FALSE(cache.WasRecentlySeen(url));
}

}  // namespace chrome_browser_net.
