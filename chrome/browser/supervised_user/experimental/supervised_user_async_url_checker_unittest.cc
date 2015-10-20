// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/supervised_user/experimental/supervised_user_async_url_checker.h"
#include "net/base/net_errors.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;

namespace {

const size_t kCacheSize = 2;

const int kSupervisedUserAsyncURLCheckerURLFetcherID = 0;

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
  base::DictionaryValue* classification_dict = new base::DictionaryValue;
  if (is_porn)
    classification_dict->SetBoolean("pornography", is_porn);
  base::ListValue* classifications_list = new base::ListValue;
  classifications_list->Append(classification_dict);
  dict.SetWithoutPathExpansion("classifications", classifications_list);
  std::string result;
  base::JSONWriter::Write(dict, &result);
  return result;
}

}  // namespace

class SupervisedUserAsyncURLCheckerTest : public testing::Test {
 public:
  SupervisedUserAsyncURLCheckerTest()
      : next_url_(0),
        request_context_(new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get())),
        checker_(request_context_.get(), kCacheSize) {}

  MOCK_METHOD3(OnCheckDone,
               void(const GURL& url,
                    SupervisedUserURLFilter::FilteringBehavior behavior,
                    bool uncertain));

 protected:
  GURL GetNewURL() {
    CHECK(next_url_ < arraysize(kURLs));
    return GURL(kURLs[next_url_++]);
  }

  // Returns true if the result was returned synchronously (cache hit).
  bool CheckURL(const GURL& url) {
    return checker_.CheckURL(
        url,
        base::Bind(&SupervisedUserAsyncURLCheckerTest::OnCheckDone,
                   base::Unretained(this)));
  }

  net::TestURLFetcher* GetURLFetcher() {
    net::TestURLFetcher* url_fetcher = url_fetcher_factory_.GetFetcherByID(
        kSupervisedUserAsyncURLCheckerURLFetcherID);
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

  void SendFailedResponse() {
    SendResponse(net::ERR_ABORTED, std::string());
  }

  size_t next_url_;
  base::MessageLoop message_loop_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  SupervisedUserAsyncURLChecker checker_;
};

TEST_F(SupervisedUserAsyncURLCheckerTest, Simple) {
  {
    GURL url(GetNewURL());
    ASSERT_FALSE(CheckURL(url));
    EXPECT_CALL(*this, OnCheckDone(url, SupervisedUserURLFilter::ALLOW, false));
    SendValidResponse(false);
  }
  {
    GURL url(GetNewURL());
    ASSERT_FALSE(CheckURL(url));
    EXPECT_CALL(*this, OnCheckDone(url, SupervisedUserURLFilter::BLOCK, false));
    SendValidResponse(true);
  }
  {
    GURL url(GetNewURL());
    ASSERT_FALSE(CheckURL(url));
    EXPECT_CALL(*this, OnCheckDone(url, SupervisedUserURLFilter::ALLOW, true));
    SendFailedResponse();
  }
}

TEST_F(SupervisedUserAsyncURLCheckerTest, Equivalence) {
  // Leading "www." in the response should be ignored.
  {
    GURL url("http://example.com");
    GURL url_response("http://www.example.com");
    ASSERT_FALSE(CheckURL(url));
    EXPECT_CALL(*this, OnCheckDone(url, SupervisedUserURLFilter::ALLOW, false));
    SendValidResponse(false);
  }
  // Scheme should be ignored.
  {
    GURL url("http://www.example2.com");
    GURL url_response("https://www.example2.com");
    ASSERT_FALSE(CheckURL(url));
    EXPECT_CALL(*this, OnCheckDone(url, SupervisedUserURLFilter::ALLOW, false));
    SendValidResponse(false);
  }
  // Both at the same time should work as well.
  {
    GURL url("http://example3.com");
    GURL url_response("https://www.example3.com");
    ASSERT_FALSE(CheckURL(url));
    EXPECT_CALL(*this, OnCheckDone(url, SupervisedUserURLFilter::ALLOW, false));
    SendValidResponse(false);
  }
}

TEST_F(SupervisedUserAsyncURLCheckerTest, Cache) {
  // One more URL than fit in the cache.
  ASSERT_EQ(2u, kCacheSize);
  GURL url1(GetNewURL());
  GURL url2(GetNewURL());
  GURL url3(GetNewURL());

  // Populate the cache.
  ASSERT_FALSE(CheckURL(url1));
  EXPECT_CALL(*this, OnCheckDone(url1, SupervisedUserURLFilter::ALLOW, false));
  SendValidResponse(false);
  ASSERT_FALSE(CheckURL(url2));
  EXPECT_CALL(*this, OnCheckDone(url2, SupervisedUserURLFilter::ALLOW, false));
  SendValidResponse(false);

  // Now we should get results synchronously.
  EXPECT_CALL(*this, OnCheckDone(url2, SupervisedUserURLFilter::ALLOW, false));
  ASSERT_TRUE(CheckURL(url2));
  EXPECT_CALL(*this, OnCheckDone(url1, SupervisedUserURLFilter::ALLOW, false));
  ASSERT_TRUE(CheckURL(url1));

  // Now |url2| is the LRU and should be evicted on the next check.
  ASSERT_FALSE(CheckURL(url3));
  EXPECT_CALL(*this, OnCheckDone(url3, SupervisedUserURLFilter::ALLOW, false));
  SendValidResponse(false);

  ASSERT_FALSE(CheckURL(url2));
  EXPECT_CALL(*this, OnCheckDone(url2, SupervisedUserURLFilter::ALLOW, false));
  SendValidResponse(false);
}

TEST_F(SupervisedUserAsyncURLCheckerTest, CoalesceRequestsToSameURL) {
  GURL url(GetNewURL());
  // Start two checks for the same URL.
  ASSERT_FALSE(CheckURL(url));
  ASSERT_FALSE(CheckURL(url));
  // A single response should answer both checks.
  EXPECT_CALL(*this, OnCheckDone(url, SupervisedUserURLFilter::ALLOW, false))
      .Times(2);
  SendValidResponse(false);
}

TEST_F(SupervisedUserAsyncURLCheckerTest, CacheTimeout) {
  GURL url(GetNewURL());

  checker_.SetCacheTimeoutForTesting(base::TimeDelta::FromSeconds(0));

  ASSERT_FALSE(CheckURL(url));
  EXPECT_CALL(*this, OnCheckDone(url, SupervisedUserURLFilter::ALLOW, false));
  SendValidResponse(false);

  // Since the cache timeout is zero, the cache entry should be invalidated
  // immediately.
  ASSERT_FALSE(CheckURL(url));
  EXPECT_CALL(*this, OnCheckDone(url, SupervisedUserURLFilter::BLOCK, false));
  SendValidResponse(true);
}
