// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/service/cloud_print/cloud_print_url_fetcher.h"
#include "chrome/service/service_process.h"
#include "googleurl/src/gurl.h"
#include "net/test/test_server.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "net/url_request/url_request_throttler_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

namespace {

const FilePath::CharType kDocRoot[] = FILE_PATH_LITERAL("chrome/test/data");

int g_request_context_getter_instances = 0;
class TrackingTestURLRequestContextGetter
    : public TestURLRequestContextGetter {
 public:
  explicit TrackingTestURLRequestContextGetter(
      base::MessageLoopProxy* io_message_loop_proxy,
      net::URLRequestThrottlerManager* throttler_manager)
      : TestURLRequestContextGetter(io_message_loop_proxy),
        throttler_manager_(throttler_manager),
        context_(NULL) {
    g_request_context_getter_instances++;
  }

  virtual TestURLRequestContext* GetURLRequestContext() OVERRIDE {
    if (!context_.get()) {
      context_.reset(new TestURLRequestContext(true));
      context_->set_throttler_manager(throttler_manager_);
      context_->Init();
    }
    return context_.get();
  }

 protected:
  virtual ~TrackingTestURLRequestContextGetter() {
    g_request_context_getter_instances--;
  }

 private:
  // Not owned here.
  net::URLRequestThrottlerManager* throttler_manager_;
  scoped_ptr<TestURLRequestContext> context_;
};

class TestCloudPrintURLFetcher : public CloudPrintURLFetcher {
 public:
  explicit TestCloudPrintURLFetcher(
      base::MessageLoopProxy* io_message_loop_proxy)
      : io_message_loop_proxy_(io_message_loop_proxy) {
  }

  virtual net::URLRequestContextGetter* GetRequestContextGetter() {
    return new TrackingTestURLRequestContextGetter(
        io_message_loop_proxy_.get(), throttler_manager());
  }

  net::URLRequestThrottlerManager* throttler_manager() {
    return &throttler_manager_;
  }

 private:
  virtual ~TestCloudPrintURLFetcher() {}

  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;

  // We set this as the throttler manager for the
  // TestURLRequestContext we create.
  net::URLRequestThrottlerManager throttler_manager_;
};

class CloudPrintURLFetcherTest : public testing::Test,
                                 public CloudPrintURLFetcherDelegate {
 public:
  CloudPrintURLFetcherTest() : max_retries_(0), fetcher_(NULL) { }

  // Creates a URLFetcher, using the program's main thread to do IO.
  virtual void CreateFetcher(const GURL& url, int max_retries);

  // CloudPrintURLFetcher::Delegate
  virtual CloudPrintURLFetcher::ResponseAction HandleRawResponse(
      const net::URLFetcher* source,
      const GURL& url,
      const net::URLRequestStatus& status,
      int response_code,
      const net::ResponseCookies& cookies,
      const std::string& data);

  virtual CloudPrintURLFetcher::ResponseAction OnRequestAuthError() {
    ADD_FAILURE();
    return CloudPrintURLFetcher::STOP_PROCESSING;
  }

  virtual std::string GetAuthHeader() {
    return std::string();
  }

  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy() {
    return io_message_loop_proxy_;
  }

 protected:
  virtual void SetUp() {
    testing::Test::SetUp();

    io_message_loop_proxy_ = base::MessageLoopProxy::current();
  }

  virtual void TearDown() {
    fetcher_ = NULL;
    // Deleting the fetcher causes a task to be posted to the IO thread to
    // release references to the URLRequestContextGetter. We need to run all
    // pending tasks to execute that (this is the IO thread).
    MessageLoop::current()->RunAllPending();
    EXPECT_EQ(0, g_request_context_getter_instances);
  }

  // URLFetcher is designed to run on the main UI thread, but in our tests
  // we assume that the current thread is the IO thread where the URLFetcher
  // dispatches its requests to.  When we wish to simulate being used from
  // a UI thread, we dispatch a worker thread to do so.
  MessageLoopForIO io_loop_;
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;
  int max_retries_;
  Time start_time_;
  scoped_refptr<TestCloudPrintURLFetcher> fetcher_;
};

class CloudPrintURLFetcherBasicTest : public CloudPrintURLFetcherTest {
 public:
  CloudPrintURLFetcherBasicTest()
      : handle_raw_response_(false), handle_raw_data_(false) { }
  // CloudPrintURLFetcher::Delegate
  virtual CloudPrintURLFetcher::ResponseAction HandleRawResponse(
      const net::URLFetcher* source,
      const GURL& url,
      const net::URLRequestStatus& status,
      int response_code,
      const net::ResponseCookies& cookies,
      const std::string& data);

  virtual CloudPrintURLFetcher::ResponseAction HandleRawData(
      const net::URLFetcher* source,
      const GURL& url,
      const std::string& data);

  virtual CloudPrintURLFetcher::ResponseAction HandleJSONData(
      const net::URLFetcher* source,
      const GURL& url,
      DictionaryValue* json_data,
      bool succeeded);

  void SetHandleRawResponse(bool handle_raw_response) {
    handle_raw_response_ = handle_raw_response;
  }
  void SetHandleRawData(bool handle_raw_data) {
    handle_raw_data_ = handle_raw_data;
  }
 private:
  bool handle_raw_response_;
  bool handle_raw_data_;
};

// Version of CloudPrintURLFetcherTest that tests overload protection.
class CloudPrintURLFetcherOverloadTest : public CloudPrintURLFetcherTest {
 public:
  CloudPrintURLFetcherOverloadTest() : response_count_(0) {
  }

  // CloudPrintURLFetcher::Delegate
  virtual CloudPrintURLFetcher::ResponseAction HandleRawData(
      const net::URLFetcher* source,
      const GURL& url,
      const std::string& data);

 private:
  int response_count_;
};

// Version of CloudPrintURLFetcherTest that tests backoff protection.
class CloudPrintURLFetcherRetryBackoffTest : public CloudPrintURLFetcherTest {
 public:
  CloudPrintURLFetcherRetryBackoffTest() : response_count_(0) {
  }

  // CloudPrintURLFetcher::Delegate
  virtual CloudPrintURLFetcher::ResponseAction HandleRawData(
      const net::URLFetcher* source,
      const GURL& url,
      const std::string& data);

  virtual void OnRequestGiveUp();

 private:
  int response_count_;
};


void CloudPrintURLFetcherTest::CreateFetcher(const GURL& url, int max_retries) {
  fetcher_ = new TestCloudPrintURLFetcher(io_message_loop_proxy());

  // Registers an entry for test url. It only allows 3 requests to be sent
  // in 200 milliseconds.
  scoped_refptr<net::URLRequestThrottlerEntry> entry(
      new net::URLRequestThrottlerEntry(
          fetcher_->throttler_manager(), "", 200, 3, 1, 2.0, 0.0, 256));
  fetcher_->throttler_manager()->OverrideEntryForTests(url, entry);

  max_retries_ = max_retries;
  start_time_ = Time::Now();
  fetcher_->StartGetRequest(url, this, max_retries_, std::string());
}

CloudPrintURLFetcher::ResponseAction
CloudPrintURLFetcherTest::HandleRawResponse(
    const net::URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const net::ResponseCookies& cookies,
    const std::string& data) {
  EXPECT_TRUE(status.is_success());
  EXPECT_EQ(200, response_code);  // HTTP OK
  EXPECT_FALSE(data.empty());
  return CloudPrintURLFetcher::CONTINUE_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
CloudPrintURLFetcherBasicTest::HandleRawResponse(
    const net::URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const net::ResponseCookies& cookies,
    const std::string& data) {
  EXPECT_TRUE(status.is_success());
  EXPECT_EQ(200, response_code);  // HTTP OK
  EXPECT_FALSE(data.empty());

  if (handle_raw_response_) {
    // If the current message loop is not the IO loop, it will be shut down when
    // the main loop returns and this thread subsequently goes out of scope.
    io_message_loop_proxy()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    return CloudPrintURLFetcher::STOP_PROCESSING;
  }
  return CloudPrintURLFetcher::CONTINUE_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
CloudPrintURLFetcherBasicTest::HandleRawData(
    const net::URLFetcher* source,
    const GURL& url,
    const std::string& data) {
  // We should never get here if we returned true in HandleRawResponse
  EXPECT_FALSE(handle_raw_response_);
  if (handle_raw_data_) {
    io_message_loop_proxy()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    return CloudPrintURLFetcher::STOP_PROCESSING;
  }
  return CloudPrintURLFetcher::CONTINUE_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
CloudPrintURLFetcherBasicTest::HandleJSONData(
    const net::URLFetcher* source,
    const GURL& url,
    DictionaryValue* json_data,
    bool succeeded) {
  // We should never get here if we returned true in one of the above methods.
  EXPECT_FALSE(handle_raw_response_);
  EXPECT_FALSE(handle_raw_data_);
  io_message_loop_proxy()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
CloudPrintURLFetcherOverloadTest::HandleRawData(
    const net::URLFetcher* source,
    const GURL& url,
    const std::string& data) {
  const TimeDelta one_second = TimeDelta::FromMilliseconds(1000);
  response_count_++;
  if (response_count_ < 20) {
    fetcher_->StartGetRequest(url,
                              this,
                              max_retries_,
                              std::string());
  } else {
    // We have already sent 20 requests continuously. And we expect that
    // it takes more than 1 second due to the overload protection settings.
    EXPECT_TRUE(Time::Now() - start_time_ >= one_second);
    io_message_loop_proxy()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
CloudPrintURLFetcherRetryBackoffTest::HandleRawData(
    const net::URLFetcher* source,
    const GURL& url,
    const std::string& data) {
  response_count_++;
  // First attempt + 11 retries = 12 total responses.
  EXPECT_LE(response_count_, 12);
  return CloudPrintURLFetcher::RETRY_REQUEST;
}

void CloudPrintURLFetcherRetryBackoffTest::OnRequestGiveUp() {
  // It takes more than 200 ms to finish all 11 requests.
  EXPECT_TRUE(Time::Now() - start_time_ >= TimeDelta::FromMilliseconds(200));
  io_message_loop_proxy()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

// http://code.google.com/p/chromium/issues/detail?id=60426
TEST_F(CloudPrintURLFetcherBasicTest, DISABLED_HandleRawResponse) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP,
                              net::TestServer::kLocalhost,
                              FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());
  SetHandleRawResponse(true);

  CreateFetcher(test_server.GetURL("echo"), 0);
  MessageLoop::current()->Run();
}

// http://code.google.com/p/chromium/issues/detail?id=60426
TEST_F(CloudPrintURLFetcherBasicTest, DISABLED_HandleRawData) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP,
                              net::TestServer::kLocalhost,
                              FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  SetHandleRawData(true);
  CreateFetcher(test_server.GetURL("echo"), 0);
  MessageLoop::current()->Run();
}

TEST_F(CloudPrintURLFetcherOverloadTest, Protect) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP,
                              net::TestServer::kLocalhost,
                              FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  GURL url(test_server.GetURL("defaultresponse"));
  CreateFetcher(url, 11);

  MessageLoop::current()->Run();
}

// http://code.google.com/p/chromium/issues/detail?id=60426
TEST_F(CloudPrintURLFetcherRetryBackoffTest, DISABLED_GiveUp) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP,
                              net::TestServer::kLocalhost,
                              FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  GURL url(test_server.GetURL("defaultresponse"));
  CreateFetcher(url, 11);

  MessageLoop::current()->Run();
}

}  // namespace.
