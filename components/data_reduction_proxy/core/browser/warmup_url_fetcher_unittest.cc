// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/warmup_url_fetcher.h"

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

namespace {

class WarmupURLFetcherTest : public WarmupURLFetcher {
 public:
  WarmupURLFetcherTest(const scoped_refptr<net::URLRequestContextGetter>&
                           url_request_context_getter)
      : WarmupURLFetcher(url_request_context_getter) {}

  ~WarmupURLFetcherTest() override {}

  using WarmupURLFetcher::GetWarmupURLWithQueryParam;

 private:
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(WarmupURLFetcherTest);
};

// Test that query param for the warmup URL is randomly set.
TEST(WarmupURLFetcherTest, TestGetWarmupURLWithQueryParam) {
  base::MessageLoopForIO message_loop;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter =
      new net::TestURLRequestContextGetter(message_loop.task_runner());
  WarmupURLFetcherTest warmup_url_fetcher(request_context_getter);

  GURL gurl_original;
  warmup_url_fetcher.GetWarmupURLWithQueryParam(&gurl_original);
  EXPECT_FALSE(gurl_original.query().empty());

  bool query_param_different = false;

  // Generate 5 more GURLs. At least one of them should have a different query
  // param than that of |gurl_original|. Multiple GURLs are generated to
  // probability of test failing due to query params of two GURLs being equal
  // due to chance.
  for (size_t i = 0; i < 5; ++i) {
    GURL gurl;
    warmup_url_fetcher.GetWarmupURLWithQueryParam(&gurl);
    EXPECT_EQ(gurl_original.host(), gurl.host());
    EXPECT_EQ(gurl_original.port(), gurl.port());
    EXPECT_EQ(gurl_original.path(), gurl.path());

    EXPECT_FALSE(gurl.query().empty());

    if (gurl_original.query() != gurl.query())
      query_param_different = true;
  }
  EXPECT_TRUE(query_param_different);
}

}  // namespace

}  // namespace data_reduction_proxy