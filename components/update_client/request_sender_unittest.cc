// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "components/update_client/request_sender.h"
#include "components/update_client/test_configurator.h"
#include "components/update_client/url_request_post_interceptor.h"
#include "net/url_request/url_fetcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {

namespace {

const char kUrl1[] = "https://localhost2/path1";
const char kUrl2[] = "https://localhost2/path2";
const char kUrlPath1[] = "path1";
const char kUrlPath2[] = "path2";

}  // namespace

class RequestSenderTest : public testing::Test {
 public:
  RequestSenderTest();
  ~RequestSenderTest() override;

  // Overrides from testing::Test.
  void SetUp() override;
  void TearDown() override;

  void RequestSenderComplete(const net::URLFetcher* source);

 protected:
  void Quit();
  void RunThreads();
  void RunThreadsUntilIdle();

  scoped_refptr<TestConfigurator> config_;
  scoped_ptr<RequestSender> request_sender_;
  scoped_ptr<InterceptorFactory> interceptor_factory_;

  URLRequestPostInterceptor* post_interceptor_1_;  // Owned by the factory.
  URLRequestPostInterceptor* post_interceptor_2_;  // Owned by the factory.

  const net::URLFetcher* url_fetcher_source_;

 private:
  base::MessageLoopForIO loop_;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(RequestSenderTest);
};

RequestSenderTest::RequestSenderTest()
    : post_interceptor_1_(nullptr),
      post_interceptor_2_(nullptr),
      url_fetcher_source_(nullptr) {
}

RequestSenderTest::~RequestSenderTest() {
}

void RequestSenderTest::SetUp() {
  config_ = new TestConfigurator(base::ThreadTaskRunnerHandle::Get(),
                                 base::ThreadTaskRunnerHandle::Get());
  interceptor_factory_.reset(
      new InterceptorFactory(base::ThreadTaskRunnerHandle::Get()));
  post_interceptor_1_ =
      interceptor_factory_->CreateInterceptorForPath(kUrlPath1);
  post_interceptor_2_ =
      interceptor_factory_->CreateInterceptorForPath(kUrlPath2);
  EXPECT_TRUE(post_interceptor_1_);
  EXPECT_TRUE(post_interceptor_2_);

  request_sender_.reset();
}

void RequestSenderTest::TearDown() {
  request_sender_.reset();

  post_interceptor_1_ = nullptr;
  post_interceptor_2_ = nullptr;

  interceptor_factory_.reset();

  config_ = nullptr;

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
  EXPECT_TRUE(post_interceptor_1_->ExpectRequest(new PartialMatch("test")));

  std::vector<GURL> urls;
  urls.push_back(GURL(kUrl1));
  urls.push_back(GURL(kUrl2));
  request_sender_.reset(new RequestSender(config_));
  request_sender_->Send("test", urls,
                        base::Bind(&RequestSenderTest::RequestSenderComplete,
                                   base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(1, post_interceptor_1_->GetHitCount())
      << post_interceptor_1_->GetRequestsAsString();
  EXPECT_EQ(1, post_interceptor_1_->GetCount())
      << post_interceptor_1_->GetRequestsAsString();

  EXPECT_STREQ("test", post_interceptor_1_->GetRequests()[0].c_str());
  EXPECT_EQ(GURL(kUrl1), url_fetcher_source_->GetOriginalURL());
  EXPECT_EQ(200, url_fetcher_source_->GetResponseCode());
}

// Tests that the request succeeds using the second url after the first url
// has failed.
TEST_F(RequestSenderTest, RequestSendSuccessWithFallback) {
  EXPECT_TRUE(
      post_interceptor_1_->ExpectRequest(new PartialMatch("test"), 403));
  EXPECT_TRUE(post_interceptor_2_->ExpectRequest(new PartialMatch("test")));

  std::vector<GURL> urls;
  urls.push_back(GURL(kUrl1));
  urls.push_back(GURL(kUrl2));
  request_sender_.reset(new RequestSender(config_));
  request_sender_->Send("test", urls,
                        base::Bind(&RequestSenderTest::RequestSenderComplete,
                                   base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(1, post_interceptor_1_->GetHitCount())
      << post_interceptor_1_->GetRequestsAsString();
  EXPECT_EQ(1, post_interceptor_1_->GetCount())
      << post_interceptor_1_->GetRequestsAsString();
  EXPECT_EQ(1, post_interceptor_2_->GetHitCount())
      << post_interceptor_2_->GetRequestsAsString();
  EXPECT_EQ(1, post_interceptor_2_->GetCount())
      << post_interceptor_2_->GetRequestsAsString();

  EXPECT_STREQ("test", post_interceptor_1_->GetRequests()[0].c_str());
  EXPECT_STREQ("test", post_interceptor_2_->GetRequests()[0].c_str());
  EXPECT_EQ(GURL(kUrl2), url_fetcher_source_->GetOriginalURL());
  EXPECT_EQ(200, url_fetcher_source_->GetResponseCode());
}

// Tests that the request fails when both urls have failed.
TEST_F(RequestSenderTest, RequestSendFailed) {
  EXPECT_TRUE(
      post_interceptor_1_->ExpectRequest(new PartialMatch("test"), 403));
  EXPECT_TRUE(
      post_interceptor_2_->ExpectRequest(new PartialMatch("test"), 403));

  std::vector<GURL> urls;
  urls.push_back(GURL(kUrl1));
  urls.push_back(GURL(kUrl2));
  request_sender_.reset(new RequestSender(config_));
  request_sender_->Send("test", urls,
                        base::Bind(&RequestSenderTest::RequestSenderComplete,
                                   base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(1, post_interceptor_1_->GetHitCount())
      << post_interceptor_1_->GetRequestsAsString();
  EXPECT_EQ(1, post_interceptor_1_->GetCount())
      << post_interceptor_1_->GetRequestsAsString();
  EXPECT_EQ(1, post_interceptor_2_->GetHitCount())
      << post_interceptor_2_->GetRequestsAsString();
  EXPECT_EQ(1, post_interceptor_2_->GetCount())
      << post_interceptor_2_->GetRequestsAsString();

  EXPECT_STREQ("test", post_interceptor_1_->GetRequests()[0].c_str());
  EXPECT_STREQ("test", post_interceptor_2_->GetRequests()[0].c_str());
  EXPECT_EQ(GURL(kUrl2), url_fetcher_source_->GetOriginalURL());
  EXPECT_EQ(403, url_fetcher_source_->GetResponseCode());
}

// Tests that the request fails when no urls are provided.
TEST_F(RequestSenderTest, RequestSendFailedNoUrls) {
  std::vector<GURL> urls;
  request_sender_.reset(new RequestSender(config_));
  request_sender_->Send("test", urls,
                        base::Bind(&RequestSenderTest::RequestSenderComplete,
                                   base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(nullptr, url_fetcher_source_);
}

}  // namespace update_client
