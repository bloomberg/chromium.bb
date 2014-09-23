// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/component_updater/request_sender.h"
#include "components/component_updater/test/test_configurator.h"
#include "components/component_updater/test/url_request_post_interceptor.h"
#include "net/url_request/url_fetcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {

const char kUrl1[] = "https://localhost2/path1";
const char kUrl2[] = "https://localhost2/path2";
const char kUrlPath1[] = "path1";
const char kUrlPath2[] = "path2";

}  // namespace

class RequestSenderTest : public testing::Test {
 public:
  RequestSenderTest();
  virtual ~RequestSenderTest();

  // Overrides from testing::Test.
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  void RequestSenderComplete(const net::URLFetcher* source);

 protected:
  void Quit();
  void RunThreads();
  void RunThreadsUntilIdle();

  scoped_ptr<TestConfigurator> config_;
  scoped_ptr<RequestSender> request_sender_;
  scoped_ptr<InterceptorFactory> interceptor_factory_;

  URLRequestPostInterceptor* post_interceptor_1;  // Owned by the factory.
  URLRequestPostInterceptor* post_interceptor_2;  // Owned by the factory.

  const net::URLFetcher* url_fetcher_source_;

 private:
  base::MessageLoopForIO loop_;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(RequestSenderTest);
};

RequestSenderTest::RequestSenderTest()
    : post_interceptor_1(NULL),
      post_interceptor_2(NULL),
      url_fetcher_source_(NULL) {
  net::URLFetcher::SetEnableInterceptionForTests(true);
}

RequestSenderTest::~RequestSenderTest() {
  net::URLFetcher::SetEnableInterceptionForTests(false);
}

void RequestSenderTest::SetUp() {
  config_.reset(new TestConfigurator(base::MessageLoopProxy::current(),
                                     base::MessageLoopProxy::current()));
  interceptor_factory_.reset(
      new InterceptorFactory(base::MessageLoopProxy::current()));
  post_interceptor_1 =
      interceptor_factory_->CreateInterceptorForPath(kUrlPath1);
  post_interceptor_2 =
      interceptor_factory_->CreateInterceptorForPath(kUrlPath2);
  EXPECT_TRUE(post_interceptor_1);
  EXPECT_TRUE(post_interceptor_2);

  request_sender_.reset();
}

void RequestSenderTest::TearDown() {
  request_sender_.reset();

  post_interceptor_1 = NULL;
  post_interceptor_2 = NULL;

  interceptor_factory_.reset();

  config_.reset();

  RunThreadsUntilIdle();
}

void RequestSenderTest::RunThreads() {
  base::RunLoop runloop;
  quit_closure_ = runloop.QuitClosure();
  runloop.Run();

  // Since some tests need to drain currently enqueued tasks such as network
  // intercepts on the IO thread, run the threads until they are
  // idle. The component updater service won't loop again until the loop count
  // is set and the service is started.
  RunThreadsUntilIdle();
}

void RequestSenderTest::RunThreadsUntilIdle() {
  base::RunLoop().RunUntilIdle();
}

void RequestSenderTest::Quit() {
  if (!quit_closure_.is_null())
    quit_closure_.Run();
}

void RequestSenderTest::RequestSenderComplete(const net::URLFetcher* source) {
  url_fetcher_source_ = source;
  Quit();
}

// Tests that when a request to the first url succeeds, the subsequent urls are
// not tried.
TEST_F(RequestSenderTest, RequestSendSuccess) {
  EXPECT_TRUE(post_interceptor_1->ExpectRequest(new PartialMatch("test")));

  std::vector<GURL> urls;
  urls.push_back(GURL(kUrl1));
  urls.push_back(GURL(kUrl2));
  request_sender_.reset(new RequestSender(*config_));
  request_sender_->Send("test",
                        urls,
                        base::Bind(&RequestSenderTest::RequestSenderComplete,
                                   base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(1, post_interceptor_1->GetHitCount())
      << post_interceptor_1->GetRequestsAsString();
  EXPECT_EQ(1, post_interceptor_1->GetCount())
      << post_interceptor_1->GetRequestsAsString();

  EXPECT_STREQ("test", post_interceptor_1->GetRequests()[0].c_str());
  EXPECT_EQ(GURL(kUrl1), url_fetcher_source_->GetOriginalURL());
  EXPECT_EQ(200, url_fetcher_source_->GetResponseCode());
}

// Tests that the request succeeds using the second url after the first url
// has failed.
TEST_F(RequestSenderTest, RequestSendSuccessWithFallback) {
  EXPECT_TRUE(post_interceptor_1->ExpectRequest(new PartialMatch("test"), 403));
  EXPECT_TRUE(post_interceptor_2->ExpectRequest(new PartialMatch("test")));

  std::vector<GURL> urls;
  urls.push_back(GURL(kUrl1));
  urls.push_back(GURL(kUrl2));
  request_sender_.reset(new RequestSender(*config_));
  request_sender_->Send("test",
                        urls,
                        base::Bind(&RequestSenderTest::RequestSenderComplete,
                                   base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(1, post_interceptor_1->GetHitCount())
      << post_interceptor_1->GetRequestsAsString();
  EXPECT_EQ(1, post_interceptor_1->GetCount())
      << post_interceptor_1->GetRequestsAsString();
  EXPECT_EQ(1, post_interceptor_2->GetHitCount())
      << post_interceptor_2->GetRequestsAsString();
  EXPECT_EQ(1, post_interceptor_2->GetCount())
      << post_interceptor_2->GetRequestsAsString();

  EXPECT_STREQ("test", post_interceptor_1->GetRequests()[0].c_str());
  EXPECT_STREQ("test", post_interceptor_2->GetRequests()[0].c_str());
  EXPECT_EQ(GURL(kUrl2), url_fetcher_source_->GetOriginalURL());
  EXPECT_EQ(200, url_fetcher_source_->GetResponseCode());
}

// Tests that the request fails when both urls have failed.
TEST_F(RequestSenderTest, RequestSendFailed) {
  EXPECT_TRUE(post_interceptor_1->ExpectRequest(new PartialMatch("test"), 403));
  EXPECT_TRUE(post_interceptor_2->ExpectRequest(new PartialMatch("test"), 403));

  std::vector<GURL> urls;
  urls.push_back(GURL(kUrl1));
  urls.push_back(GURL(kUrl2));
  request_sender_.reset(new RequestSender(*config_));
  request_sender_->Send("test",
                        urls,
                        base::Bind(&RequestSenderTest::RequestSenderComplete,
                                   base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(1, post_interceptor_1->GetHitCount())
      << post_interceptor_1->GetRequestsAsString();
  EXPECT_EQ(1, post_interceptor_1->GetCount())
      << post_interceptor_1->GetRequestsAsString();
  EXPECT_EQ(1, post_interceptor_2->GetHitCount())
      << post_interceptor_2->GetRequestsAsString();
  EXPECT_EQ(1, post_interceptor_2->GetCount())
      << post_interceptor_2->GetRequestsAsString();

  EXPECT_STREQ("test", post_interceptor_1->GetRequests()[0].c_str());
  EXPECT_STREQ("test", post_interceptor_2->GetRequests()[0].c_str());
  EXPECT_EQ(GURL(kUrl2), url_fetcher_source_->GetOriginalURL());
  EXPECT_EQ(403, url_fetcher_source_->GetResponseCode());
}

}  // namespace component_updater
