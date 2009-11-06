// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/thread.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/sync/glue/http_bridge.h"
#include "net/url_request/url_request_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::HttpBridge;

namespace {
// TODO(timsteele): Should use PathService here. See Chromium Issue 3113.
const wchar_t kDocRoot[] = L"chrome/test/data";
}

// Lazy getter for TestURLRequestContext instances.
class TestURLRequestContextGetter : public URLRequestContextGetter {
 public:
  virtual URLRequestContext* GetURLRequestContext() {
    if (!context_)
      context_ = new TestURLRequestContext;
    return context_;
  }
 private:
  ~TestURLRequestContextGetter() {}

  scoped_refptr<URLRequestContext> context_;
};

class HttpBridgeTest : public testing::Test {
 public:
  HttpBridgeTest()
      : fake_default_request_context_getter_(NULL),
        io_thread_(ChromeThread::IO) {
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

  MessageLoop* io_thread_loop() { return io_thread_.message_loop(); }

  // Note this is lazy created, so don't call this before your bridge.
  TestURLRequestContextGetter* GetTestRequestContextGetter() {
    return fake_default_request_context_getter_;
  }

 private:
  // A make-believe "default" request context, as would be returned by
  // Profile::GetDefaultRequestContext().  Created lazily by BuildBridge.
  TestURLRequestContextGetter* fake_default_request_context_getter_;

  // Separate thread for IO used by the HttpBridge.
  ChromeThread io_thread_;

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
  void CallOnURLFetchComplete() {
    ASSERT_TRUE(MessageLoop::current() == test_->io_thread_loop());
    // We return no cookies and a dummy content response.
    ResponseCookies cookies;

    std::string response_content = "success!";
    OnURLFetchComplete(NULL, GURL("www.google.com"), URLRequestStatus(),
                       200, cookies, response_content);
  }
  HttpBridgeTest* test_;
};

TEST_F(HttpBridgeTest, TestUsesSameHttpNetworkSession) {
  scoped_refptr<HttpBridge> http_bridge(this->BuildBridge());
  EXPECT_TRUE(GetTestRequestContextGetter());
  net::HttpNetworkSession* test_session =
      GetTestRequestContextGetter()->GetURLRequestContext()->
      http_transaction_factory()->GetSession();
  EXPECT_EQ(test_session,
            http_bridge->GetRequestContextGetter()->
                GetURLRequestContext()->
                http_transaction_factory()->GetSession());
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
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  scoped_refptr<HttpBridge> http_bridge(BuildBridge());

  std::string payload = "this should be echoed back";
  GURL echo = server->TestServerPage("echo");
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
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  scoped_refptr<HttpBridge> http_bridge(BuildBridge());

  GURL echo_header = server->TestServerPage("echoall");
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
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  scoped_refptr<HttpBridge> http_bridge(BuildBridge());

  GURL echo_header = server->TestServerPage("echoall");

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
