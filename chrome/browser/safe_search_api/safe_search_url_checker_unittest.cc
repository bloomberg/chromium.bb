// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_search_api/safe_search_url_checker.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using Classification = SafeSearchURLChecker::Classification;
using testing::_;

namespace {

const size_t kCacheSize = 2;

const int kSafeSearchURLCheckerURLFetcherID = 0;

const char* kURLs[] = {
    "http://www.randomsite1.com",
    "http://www.randomsite2.com",
    "http://www.randomsite3.com",
    "http://www.randomsite4.com",
    "http://www.randomsite5.com",
    "http://www.randomsite6.com",
    "http://www.randomsite7.com",
    "http://www.randomsite8.com",
    "http://www.randomsite9.com",
};

std::string BuildResponse(bool is_porn) {
  base::DictionaryValue dict;
  std::unique_ptr<base::DictionaryValue> classification_dict(
      new base::DictionaryValue);
  if (is_porn)
    classification_dict->SetBoolean("pornography", is_porn);
  base::ListValue* classifications_list = new base::ListValue;
  classifications_list->Append(std::move(classification_dict));
  dict.SetWithoutPathExpansion("classifications", classifications_list);
  std::string result;
  base::JSONWriter::Write(dict, &result);
  return result;
}

}  // namespace

class SafeSearchURLCheckerTest : public testing::Test {
 public:
  SafeSearchURLCheckerTest()
      : next_url_(0),
        request_context_(new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get())),
        checker_(request_context_.get(), kCacheSize) {}

  MOCK_METHOD3(OnCheckDone,
               void(const GURL& url,
                    Classification classification,
                    bool uncertain));

 protected:
  GURL GetNewURL() {
    CHECK(next_url_ < arraysize(kURLs));
    return GURL(kURLs[next_url_++]);
  }

  // Returns true if the result was returned synchronously (cache hit).
  bool CheckURL(const GURL& url) {
    return checker_.CheckURL(url,
                             base::Bind(&SafeSearchURLCheckerTest::OnCheckDone,
                                        base::Unretained(this)));
  }

  net::TestURLFetcher* GetURLFetcher() {
    net::TestURLFetcher* url_fetcher =
        url_fetcher_factory_.GetFetcherByID(kSafeSearchURLCheckerURLFetcherID);
    EXPECT_TRUE(url_fetcher);
    return url_fetcher;
  }

  void SendResponse(net::Error error, const std::string& response) {
    net::TestURLFetcher* url_fetcher = GetURLFetcher();
    url_fetcher->set_status(net::URLRequestStatus::FromError(error));
    url_fetcher->set_response_code(net::HTTP_OK);
    url_fetcher->SetResponseString(response);
    url_fetcher->delegate()->OnURLFetchComplete(url_fetcher);
  }

  void SendValidResponse(bool is_porn) {
    SendResponse(net::OK, BuildResponse(is_porn));
  }

  void SendFailedResponse() { SendResponse(net::ERR_ABORTED, std::string()); }

  size_t next_url_;
  base::MessageLoop message_loop_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  SafeSearchURLChecker checker_;
};

TEST_F(SafeSearchURLCheckerTest, Simple) {
  {
    GURL url(GetNewURL());
    ASSERT_FALSE(CheckURL(url));
    EXPECT_CALL(*this, OnCheckDone(url, Classification::SAFE, false));
    SendValidResponse(false);
  }
  {
    GURL url(GetNewURL());
    ASSERT_FALSE(CheckURL(url));
    EXPECT_CALL(*this, OnCheckDone(url, Classification::UNSAFE, false));
    SendValidResponse(true);
  }
  {
    GURL url(GetNewURL());
    ASSERT_FALSE(CheckURL(url));
    EXPECT_CALL(*this, OnCheckDone(url, Classification::SAFE, true));
    SendFailedResponse();
  }
}

TEST_F(SafeSearchURLCheckerTest, Cache) {
  // One more URL than fit in the cache.
  ASSERT_EQ(2u, kCacheSize);
  GURL url1(GetNewURL());
  GURL url2(GetNewURL());
  GURL url3(GetNewURL());

  // Populate the cache.
  ASSERT_FALSE(CheckURL(url1));
  EXPECT_CALL(*this, OnCheckDone(url1, Classification::SAFE, false));
  SendValidResponse(false);
  ASSERT_FALSE(CheckURL(url2));
  EXPECT_CALL(*this, OnCheckDone(url2, Classification::SAFE, false));
  SendValidResponse(false);

  // Now we should get results synchronously.
  EXPECT_CALL(*this, OnCheckDone(url2, Classification::SAFE, false));
  ASSERT_TRUE(CheckURL(url2));
  EXPECT_CALL(*this, OnCheckDone(url1, Classification::SAFE, false));
  ASSERT_TRUE(CheckURL(url1));

  // Now |url2| is the LRU and should be evicted on the next check.
  ASSERT_FALSE(CheckURL(url3));
  EXPECT_CALL(*this, OnCheckDone(url3, Classification::SAFE, false));
  SendValidResponse(false);

  ASSERT_FALSE(CheckURL(url2));
  EXPECT_CALL(*this, OnCheckDone(url2, Classification::SAFE, false));
  SendValidResponse(false);
}

TEST_F(SafeSearchURLCheckerTest, CoalesceRequestsToSameURL) {
  GURL url(GetNewURL());
  // Start two checks for the same URL.
  ASSERT_FALSE(CheckURL(url));
  ASSERT_FALSE(CheckURL(url));
  // A single response should answer both checks.
  EXPECT_CALL(*this, OnCheckDone(url, Classification::SAFE, false)).Times(2);
  SendValidResponse(false);
}

TEST_F(SafeSearchURLCheckerTest, CacheTimeout) {
  GURL url(GetNewURL());

  checker_.SetCacheTimeoutForTesting(base::TimeDelta::FromSeconds(0));

  ASSERT_FALSE(CheckURL(url));
  EXPECT_CALL(*this, OnCheckDone(url, Classification::SAFE, false));
  SendValidResponse(false);

  // Since the cache timeout is zero, the cache entry should be invalidated
  // immediately.
  ASSERT_FALSE(CheckURL(url));
  EXPECT_CALL(*this, OnCheckDone(url, Classification::UNSAFE, false));
  SendValidResponse(true);
}
