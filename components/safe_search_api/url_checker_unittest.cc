// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_search_api/url_checker.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/safe_search_api/stub_url_checker.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;

namespace safe_search_api {

namespace {

constexpr size_t kCacheSize = 2;

const char* kURLs[] = {
    "http://www.randomsite1.com", "http://www.randomsite2.com",
    "http://www.randomsite3.com", "http://www.randomsite4.com",
    "http://www.randomsite5.com", "http://www.randomsite6.com",
    "http://www.randomsite7.com", "http://www.randomsite8.com",
    "http://www.randomsite9.com",
};

}  // namespace

class SafeSearchURLCheckerTest : public testing::Test {
 public:
  SafeSearchURLCheckerTest()
      : next_url_(0), checker_(stub_url_checker_.BuildURLChecker(kCacheSize)) {}

  MOCK_METHOD3(OnCheckDone,
               void(const GURL& url,
                    Classification classification,
                    bool uncertain));

 protected:
  GURL GetNewURL() {
    CHECK(next_url_ < base::size(kURLs));
    return GURL(kURLs[next_url_++]);
  }

  // Returns true if the result was returned synchronously (cache hit).
  bool CheckURL(const GURL& url) {
    bool cached = checker_->CheckURL(
        url, base::BindOnce(&SafeSearchURLCheckerTest::OnCheckDone,
                            base::Unretained(this)));
    return cached;
  }

  void WaitForResponse() { base::RunLoop().RunUntilIdle(); }

  bool SendValidResponse(const GURL& url, bool is_porn) {
    stub_url_checker_.SetUpValidResponse(is_porn);
    bool result = CheckURL(url);
    WaitForResponse();
    return result;
  }

  bool SendFailedResponse(const GURL& url) {
    stub_url_checker_.SetUpFailedResponse();
    bool result = CheckURL(url);
    WaitForResponse();
    return result;
  }

  size_t next_url_;
  base::MessageLoop message_loop_;
  StubURLChecker stub_url_checker_;
  std::unique_ptr<URLChecker> checker_;
};

TEST_F(SafeSearchURLCheckerTest, Simple) {
  {
    GURL url(GetNewURL());
    EXPECT_CALL(*this, OnCheckDone(url, Classification::SAFE, false));
    ASSERT_FALSE(SendValidResponse(url, false));
  }
  {
    GURL url(GetNewURL());
    EXPECT_CALL(*this, OnCheckDone(url, Classification::UNSAFE, false));
    ASSERT_FALSE(SendValidResponse(url, true));
  }
  {
    GURL url(GetNewURL());
    EXPECT_CALL(*this, OnCheckDone(url, Classification::SAFE, true));
    ASSERT_FALSE(SendFailedResponse(url));
  }
}

TEST_F(SafeSearchURLCheckerTest, Cache) {
  // One more URL than fit in the cache.
  ASSERT_EQ(2u, kCacheSize);
  GURL url1(GetNewURL());
  GURL url2(GetNewURL());
  GURL url3(GetNewURL());

  // Populate the cache.
  EXPECT_CALL(*this, OnCheckDone(url1, Classification::SAFE, false));
  ASSERT_FALSE(SendValidResponse(url1, false));
  EXPECT_CALL(*this, OnCheckDone(url2, Classification::SAFE, false));
  ASSERT_FALSE(SendValidResponse(url2, false));

  // Now we should get results synchronously, without a network request.
  stub_url_checker_.ClearResponses();
  EXPECT_CALL(*this, OnCheckDone(url2, Classification::SAFE, false));
  ASSERT_TRUE(CheckURL(url2));
  EXPECT_CALL(*this, OnCheckDone(url1, Classification::SAFE, false));
  ASSERT_TRUE(CheckURL(url1));

  // Now |url2| is the LRU and should be evicted on the next check.
  EXPECT_CALL(*this, OnCheckDone(url3, Classification::SAFE, false));
  ASSERT_FALSE(SendValidResponse(url3, false));

  EXPECT_CALL(*this, OnCheckDone(url2, Classification::SAFE, false));
  ASSERT_FALSE(SendValidResponse(url2, false));
}

TEST_F(SafeSearchURLCheckerTest, CoalesceRequestsToSameURL) {
  GURL url(GetNewURL());
  // Start two checks for the same URL.
  stub_url_checker_.SetUpValidResponse(false);
  ASSERT_FALSE(CheckURL(url));
  ASSERT_FALSE(CheckURL(url));
  // A single response should answer both of those checks
  EXPECT_CALL(*this, OnCheckDone(url, Classification::SAFE, false)).Times(2);
  WaitForResponse();
}

TEST_F(SafeSearchURLCheckerTest, CacheTimeout) {
  GURL url(GetNewURL());

  checker_->SetCacheTimeoutForTesting(base::TimeDelta::FromSeconds(0));

  EXPECT_CALL(*this, OnCheckDone(url, Classification::SAFE, false));
  ASSERT_FALSE(SendValidResponse(url, false));

  // Since the cache timeout is zero, the cache entry should be invalidated
  // immediately.
  EXPECT_CALL(*this, OnCheckDone(url, Classification::UNSAFE, false));
  ASSERT_FALSE(SendValidResponse(url, true));
}

TEST_F(SafeSearchURLCheckerTest, AllowAllGoogleURLs) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kAllowAllGoogleUrls);
  {
    GURL url("https://sites.google.com/porn");
    EXPECT_CALL(*this, OnCheckDone(url, Classification::SAFE, _));
    // No server interaction.
    bool cache_hit = CheckURL(url);
    ASSERT_TRUE(cache_hit);
  }
  {
    GURL url("https://youtube.com/porn");
    EXPECT_CALL(*this, OnCheckDone(url, Classification::SAFE, _));
    // No server interaction
    bool cache_hit = CheckURL(url);
    ASSERT_TRUE(cache_hit);
  }
}

TEST_F(SafeSearchURLCheckerTest, NoAllowAllGoogleURLs) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kAllowAllGoogleUrls);
  {
    GURL url("https://sites.google.com/porn");
    EXPECT_CALL(*this, OnCheckDone(url, Classification::UNSAFE, _));
    stub_url_checker_.SetUpValidResponse(true);
    bool cache_hit = CheckURL(url);
    ASSERT_FALSE(cache_hit);
    WaitForResponse();
  }
  {
    GURL url("https://youtube.com/porn");
    EXPECT_CALL(*this, OnCheckDone(url, Classification::UNSAFE, _));
    stub_url_checker_.SetUpValidResponse(true);
    bool cache_hit = CheckURL(url);
    ASSERT_FALSE(cache_hit);
    WaitForResponse();
  }
}

}  // namespace safe_search_api
