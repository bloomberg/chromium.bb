// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/values.h"
#include "chrome/browser/supervised_user/experimental/supervised_user_async_url_checker.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;

namespace {

const char kCx[] = "somecsecx";
const size_t kCacheSize = 2;

const int kSupervisedUserAsyncURLCheckerSafeURLFetcherID = 0;
const int kSupervisedUserAsyncURLCheckerUnsafeURLFetcherID = 1;

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

std::string BuildResponse(const GURL& url) {
  base::DictionaryValue dict;
  base::DictionaryValue* search_info_dict = new base::DictionaryValue;
  std::string result_count = url.is_valid() ? "1" : "0";
  search_info_dict->SetStringWithoutPathExpansion("totalResults",
                                                  result_count);
  dict.SetWithoutPathExpansion("searchInformation", search_info_dict);
  if (result_count != "0") {
    base::ListValue* results_list = new base::ListValue;
    base::DictionaryValue* result_dict = new base::DictionaryValue;
    result_dict->SetStringWithoutPathExpansion("link", url.spec());
    results_list->Append(result_dict);
    dict.SetWithoutPathExpansion("items", results_list);
  }
  std::string result;
  base::JSONWriter::Write(&dict, &result);
  return result;
}

}  // namespace

class SupervisedUserAsyncURLCheckerTest : public testing::Test {
 public:
  SupervisedUserAsyncURLCheckerTest()
      : next_url_(0),
        request_context_(new net::TestURLRequestContextGetter(
            base::MessageLoopProxy::current())),
        checker_(request_context_.get(), kCx, kCacheSize) {
  }

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

  net::TestURLFetcher* GetURLFetcher(bool safe) {
    int id = safe ? kSupervisedUserAsyncURLCheckerSafeURLFetcherID
                  : kSupervisedUserAsyncURLCheckerUnsafeURLFetcherID;
    net::TestURLFetcher* url_fetcher = url_fetcher_factory_.GetFetcherByID(id);
    EXPECT_TRUE(url_fetcher);
    return url_fetcher;
  }

  void SendResponse(bool safe,
                    net::URLRequestStatus::Status status,
                    const std::string& response) {
    net::TestURLFetcher* url_fetcher = GetURLFetcher(safe);
    url_fetcher->set_status(net::URLRequestStatus(status, 0));
    url_fetcher->set_response_code(net::HTTP_OK);
    url_fetcher->SetResponseString(response);
    url_fetcher->delegate()->OnURLFetchComplete(url_fetcher);
  }

  void SendValidResponse(bool safe, const GURL& url) {
    SendResponse(safe, net::URLRequestStatus::SUCCESS, BuildResponse(url));
  }

  void SendFailedResponse(bool safe) {
    SendResponse(safe, net::URLRequestStatus::CANCELED, std::string());
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
    EXPECT_FALSE(CheckURL(url));
    // "URL found" response from safe fetcher should immediately give a
    // "not blocked" result.
    EXPECT_CALL(*this, OnCheckDone(url, SupervisedUserURLFilter::ALLOW, false));
    SendValidResponse(true, url);
  }
  {
    GURL url(GetNewURL());
    EXPECT_FALSE(CheckURL(url));
    // "URL not found" response from safe fetcher should not immediately give a
    // result.
    EXPECT_CALL(*this, OnCheckDone(_, _, _)).Times(0);
    SendValidResponse(true, GURL());
    // "URL found" response from unsafe fetcher should give a "blocked" result.
    EXPECT_CALL(*this, OnCheckDone(url, SupervisedUserURLFilter::BLOCK, false));
    SendValidResponse(false, url);
  }
  {
    GURL url(GetNewURL());
    EXPECT_FALSE(CheckURL(url));
    // "URL found" response from unsafe fetcher should not immediately give a
    // result.
    EXPECT_CALL(*this, OnCheckDone(_, _, _)).Times(0);
    SendValidResponse(false, url);
    // "URL not found" response from safe fetcher should give a "blocked"
    // result.
    EXPECT_CALL(*this, OnCheckDone(url, SupervisedUserURLFilter::BLOCK, false));
    SendValidResponse(true, GURL());
  }
  {
    GURL url(GetNewURL());
    EXPECT_FALSE(CheckURL(url));
    // "URL not found" response from unsafe fetcher should immediately give a
    // "not blocked (but uncertain)" result.
    EXPECT_CALL(*this, OnCheckDone(url, SupervisedUserURLFilter::ALLOW, true));
    SendValidResponse(false, GURL());
  }
}

TEST_F(SupervisedUserAsyncURLCheckerTest, Equivalence) {
  // Leading "www." in the response should be ignored.
  {
    GURL url("http://example.com");
    GURL url_response("http://www.example.com");
    EXPECT_FALSE(CheckURL(url));
    EXPECT_CALL(*this, OnCheckDone(url, SupervisedUserURLFilter::ALLOW, false));
    SendValidResponse(true, url_response);
  }
  // Scheme should be ignored.
  {
    GURL url("http://www.example2.com");
    GURL url_response("https://www.example2.com");
    EXPECT_FALSE(CheckURL(url));
    EXPECT_CALL(*this, OnCheckDone(url, SupervisedUserURLFilter::ALLOW, false));
    SendValidResponse(true, url_response);
  }
  // Both at the same time should work as well.
  {
    GURL url("http://example3.com");
    GURL url_response("https://www.example3.com");
    EXPECT_FALSE(CheckURL(url));
    EXPECT_CALL(*this, OnCheckDone(url, SupervisedUserURLFilter::ALLOW, false));
    SendValidResponse(true, url_response);
  }
}

TEST_F(SupervisedUserAsyncURLCheckerTest, Cache) {
  // One more URL than fit in the cache.
  ASSERT_EQ(2u, kCacheSize);
  GURL url1(GetNewURL());
  GURL url2(GetNewURL());
  GURL url3(GetNewURL());

  // Populate the cache.
  EXPECT_FALSE(CheckURL(url1));
  EXPECT_CALL(*this, OnCheckDone(url1, SupervisedUserURLFilter::ALLOW, false));
  SendValidResponse(true, url1);
  EXPECT_FALSE(CheckURL(url2));
  EXPECT_CALL(*this, OnCheckDone(url2, SupervisedUserURLFilter::ALLOW, false));
  SendValidResponse(true, url2);

  // Now we should get results synchronously.
  EXPECT_CALL(*this, OnCheckDone(url2, SupervisedUserURLFilter::ALLOW, false));
  EXPECT_TRUE(CheckURL(url2));
  EXPECT_CALL(*this, OnCheckDone(url1, SupervisedUserURLFilter::ALLOW, false));
  EXPECT_TRUE(CheckURL(url1));

  // Now |url2| is the LRU and should be evicted on the next check.
  EXPECT_FALSE(CheckURL(url3));
  EXPECT_CALL(*this, OnCheckDone(url3, SupervisedUserURLFilter::ALLOW, false));
  SendValidResponse(true, url3);

  EXPECT_FALSE(CheckURL(url2));
  EXPECT_CALL(*this, OnCheckDone(url2, SupervisedUserURLFilter::ALLOW, false));
  SendValidResponse(true, url2);
}

TEST_F(SupervisedUserAsyncURLCheckerTest, CoalesceRequestsToSameURL) {
  GURL url(GetNewURL());
  // Start two checks for the same URL.
  EXPECT_FALSE(CheckURL(url));
  EXPECT_FALSE(CheckURL(url));
  // A single response should answer both checks.
  EXPECT_CALL(*this, OnCheckDone(url, SupervisedUserURLFilter::ALLOW, false))
      .Times(2);
  SendValidResponse(true, url);
}
