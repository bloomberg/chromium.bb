// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop_proxy.h"
#include "base/threading/thread.h"
#include "chrome/browser/sync/glue/http_bridge.h"
#include "chrome/common/net/test_url_fetcher_factory.h"
#include "chrome/test/test_url_request_context_getter.h"
#include "content/browser/browser_thread.h"
#include "net/url_request/url_request_test_util.h"
#include "net/test/test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::HttpBridge;

namespace {
// TODO(timsteele): Should use PathService here. See Chromium Issue 3113.
const FilePath::CharType kDocRoot[] = FILE_PATH_LITERAL("chrome/test/data");
}

class HttpBridgeTest : public testing::Test {
 public:
  HttpBridgeTest()
      : test_server_(net::TestServer::TYPE_HTTP, FilePath(kDocRoot)),
        fake_default_request_context_getter_(NULL),
        io_thread_(BrowserThread::IO) {
  }

  virtual void SetUp() {
    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    io_thread_.StartWithOptions(options);
  }

  virtual void TearDown() {
    io_thread_loop()->ReleaseSoon(FROM_HERE,
        fake_default_request_context_getter_);
    io_thread_.Stop();
    fake_default_request_context_getter_ = NULL;
  }

  HttpBridge* BuildBridge() {
    if (!fake_default_request_context_getter_) {
      fake_default_request_context_getter_ = new TestURLRequestContextGetter();
      fake_default_request_context_getter_->AddRef();
    }
    HttpBridge* bridge = new HttpBridge(
        new HttpBridge::RequestContextGetter(
            fake_default_request_context_getter_));
    return bridge;
  }

  static void TestSameHttpNetworkSession(MessageLoop* main_message_loop,
                                         HttpBridgeTest* test) {
    scoped_refptr<HttpBridge> http_bridge(test->BuildBridge());
    EXPECT_TRUE(test->GetTestRequestContextGetter());
    net::HttpNetworkSession* test_session =
        test->GetTestRequestContextGetter()->GetURLRequestContext()->
        http_transaction_factory()->GetSession();
    EXPECT_EQ(test_session,
              http_bridge->GetRequestContextGetter()->
                  GetURLRequestContext()->
                  http_transaction_factory()->GetSession());
    main_message_loop->PostTask(FROM_HERE, new MessageLoop::QuitTask);
  }

  MessageLoop* io_thread_loop() { return io_thread_.message_loop(); }

  // Note this is lazy created, so don't call this before your bridge.
  TestURLRequestContextGetter* GetTestRequestContextGetter() {
    return fake_default_request_context_getter_;
  }

  net::TestServer test_server_;

 private:
  // A make-believe "default" request context, as would be returned by
  // Profile::GetDefaultRequestContext().  Created lazily by BuildBridge.
  TestURLRequestContextGetter* fake_default_request_context_getter_;

  // Separate thread for IO used by the HttpBridge.
  BrowserThread io_thread_;
  MessageLoop loop_;
};

class DummyURLFetcher : public TestURLFetcher {
 public:
  DummyURLFetcher() : TestURLFetcher(0, GURL(), POST, NULL) {}

  net::HttpResponseHeaders* response_headers() const {
    return NULL;
  }
};

// An HttpBridge that doesn't actually make network requests and just calls
// back with dummy response info.
class ShuntedHttpBridge : public HttpBridge {
 public:
  ShuntedHttpBridge(URLRequestContextGetter* baseline_context_getter,
                    HttpBridgeTest* test)
      : HttpBridge(new HttpBridge::RequestContextGetter(
                       baseline_context_getter)),
                   test_(test) { }
 protected:
  virtual void MakeAsynchronousPost() {
    ASSERT_TRUE(MessageLoop::current() == test_->io_thread_loop());
    // We don't actually want to make a request for this test, so just callback
    // as if it completed.
    test_->io_thread_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &ShuntedHttpBridge::CallOnURLFetchComplete));
  }
 private:
  ~ShuntedHttpBridge() {}

  void CallOnURLFetchComplete() {
    ASSERT_TRUE(MessageLoop::current() == test_->io_thread_loop());
    // We return no cookies and a dummy content response.
    ResponseCookies cookies;

    std::string response_content = "success!";
    DummyURLFetcher fetcher;
    OnURLFetchComplete(&fetcher, GURL("www.google.com"),
                       net::URLRequestStatus(),
                       200, cookies, response_content);
  }
  HttpBridgeTest* test_;
};

TEST_F(HttpBridgeTest, TestUsesSameHttpNetworkSession) {
  // Run this test on the IO thread because we can only call
  // URLRequestContextGetter::GetURLRequestContext on the IO thread.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableFunction(&HttpBridgeTest::TestSameHttpNetworkSession,
                          MessageLoop::current(), this));
  MessageLoop::current()->Run();
}

// Test the HttpBridge without actually making any network requests.
TEST_F(HttpBridgeTest, TestMakeSynchronousPostShunted) {
  scoped_refptr<URLRequestContextGetter> ctx_getter(
      new TestURLRequestContextGetter());
  scoped_refptr<HttpBridge> http_bridge(new ShuntedHttpBridge(
      ctx_getter, this));
  http_bridge->SetUserAgent("bob");
  http_bridge->SetURL("http://www.google.com", 9999);
  http_bridge->SetPostPayload("text/plain", 2, " ");

  int os_error = 0;
  int response_code = 0;
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_TRUE(success);
  EXPECT_EQ(200, response_code);
  EXPECT_EQ(0, os_error);

  EXPECT_EQ(8, http_bridge->GetResponseContentLength());
  EXPECT_EQ(std::string("success!"),
            std::string(http_bridge->GetResponseContent()));
}

// Full round-trip test of the HttpBridge, using default UA string and
// no request cookies.
TEST_F(HttpBridgeTest, TestMakeSynchronousPostLiveWithPayload) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<HttpBridge> http_bridge(BuildBridge());

  std::string payload = "this should be echoed back";
  GURL echo = test_server_.GetURL("echo");
  http_bridge->SetURL(echo.spec().c_str(), echo.IntPort());
  http_bridge->SetPostPayload("application/x-www-form-urlencoded",
                              payload.length() + 1, payload.c_str());
  int os_error = 0;
  int response_code = 0;
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_TRUE(success);
  EXPECT_EQ(200, response_code);
  EXPECT_EQ(0, os_error);

  EXPECT_EQ(payload.length() + 1,
            static_cast<size_t>(http_bridge->GetResponseContentLength()));
  EXPECT_EQ(payload, std::string(http_bridge->GetResponseContent()));
}

// Full round-trip test of the HttpBridge, using custom UA string
TEST_F(HttpBridgeTest, TestMakeSynchronousPostLiveComprehensive) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<HttpBridge> http_bridge(BuildBridge());

  GURL echo_header = test_server_.GetURL("echoall");
  http_bridge->SetUserAgent("bob");
  http_bridge->SetURL(echo_header.spec().c_str(), echo_header.IntPort());

  std::string test_payload = "###TEST PAYLOAD###";
  http_bridge->SetPostPayload("text/html", test_payload.length() + 1,
                              test_payload.c_str());

  int os_error = 0;
  int response_code = 0;
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_TRUE(success);
  EXPECT_EQ(200, response_code);
  EXPECT_EQ(0, os_error);

  std::string response(http_bridge->GetResponseContent(),
                       http_bridge->GetResponseContentLength());
  EXPECT_EQ(std::string::npos, response.find("Cookie:"));
  EXPECT_NE(std::string::npos, response.find("User-Agent: bob"));
  EXPECT_NE(std::string::npos, response.find(test_payload.c_str()));
}

TEST_F(HttpBridgeTest, TestExtraRequestHeaders) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<HttpBridge> http_bridge(BuildBridge());

  GURL echo_header = test_server_.GetURL("echoall");

  http_bridge->SetURL(echo_header.spec().c_str(), echo_header.IntPort());
  http_bridge->SetExtraRequestHeaders("test:fnord");

  std::string test_payload = "###TEST PAYLOAD###";
  http_bridge->SetPostPayload("text/html", test_payload.length() + 1,
                              test_payload.c_str());

  int os_error = 0;
  int response_code = 0;
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_TRUE(success);
  EXPECT_EQ(200, response_code);
  EXPECT_EQ(0, os_error);

  std::string response(http_bridge->GetResponseContent(),
                       http_bridge->GetResponseContentLength());

  EXPECT_NE(std::string::npos, response.find("fnord"));
  EXPECT_NE(std::string::npos, response.find(test_payload.c_str()));
}

TEST_F(HttpBridgeTest, TestResponseHeader) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<HttpBridge> http_bridge(BuildBridge());

  GURL echo_header = test_server_.GetURL("echoall");
  http_bridge->SetURL(echo_header.spec().c_str(), echo_header.IntPort());

  std::string test_payload = "###TEST PAYLOAD###";
  http_bridge->SetPostPayload("text/html", test_payload.length() + 1,
                              test_payload.c_str());

  int os_error = 0;
  int response_code = 0;
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_TRUE(success);
  EXPECT_EQ(200, response_code);
  EXPECT_EQ(0, os_error);

  EXPECT_EQ(http_bridge->GetResponseHeaderValue("Content-type"), "text/html");
  EXPECT_TRUE(http_bridge->GetResponseHeaderValue("invalid-header").empty());
}
