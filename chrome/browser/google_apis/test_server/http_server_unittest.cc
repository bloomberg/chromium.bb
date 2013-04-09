// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/test_server/http_server.h"

#include "base/stringprintf.h"
#include "base/threading/thread.h"
#include "chrome/browser/google_apis/test_server/http_request.h"
#include "chrome/browser/google_apis/test_server/http_response.h"
#include "chrome/browser/google_apis/test_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {
namespace test_server {

namespace {

// Gets the content from the given URLFetcher.
std::string GetContentFromFetcher(const net::URLFetcher& fetcher) {
  std::string result;
  const bool success = fetcher.GetResponseAsString(&result);
  EXPECT_TRUE(success);
  return result;
}

// Gets the content type from the given URLFetcher.
std::string GetContentTypeFromFetcher(const net::URLFetcher& fetcher) {
  const net::HttpResponseHeaders* headers = fetcher.GetResponseHeaders();
  if (headers) {
    std::string content_type;
    if (headers->GetMimeType(&content_type))
      return content_type;
  }
  return "";
}

}  // namespace

class HttpServerTest : public testing::Test,
                       public net::URLFetcherDelegate {
 public:
  HttpServerTest()
      : num_responses_received_(0),
        num_responses_expected_(0),
        ui_thread_(content::BrowserThread::UI, &message_loop_),
        io_thread_(content::BrowserThread::IO) {
  }

  virtual void SetUp() OVERRIDE {
    io_thread_.StartIOThread();

    request_context_getter_ = new net::TestURLRequestContextGetter(
        content::BrowserThread::GetMessageLoopProxyForThread(
            content::BrowserThread::IO));

    ASSERT_TRUE(server_.InitializeAndWaitUntilReady());
  }

  virtual void TearDown() OVERRIDE {
    server_.ShutdownAndWaitUntilComplete();
  }

  // net::URLFetcherDelegate override.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE {
    ++num_responses_received_;
    if (num_responses_received_ == num_responses_expected_)
      message_loop_.Quit();
  }

  // Waits until the specified number of responses are received.
  void WaitForResponses(int num_responses) {
    num_responses_received_ = 0;
    num_responses_expected_ = num_responses;
    message_loop_.Run();  // Will be terminated in OnURLFetchComplete().
  }

  // Handles |request| sent to |path| and returns the response per |content|,
  // |content type|, and |code|. Saves the request URL for verification.
  scoped_ptr<HttpResponse> HandleRequest(const std::string& path,
                                         const std::string& content,
                                         const std::string& content_type,
                                         ResponseCode code,
                                         const HttpRequest& request) {
    request_relative_url_ = request.relative_url;

    GURL absolute_url = server_.GetURL(request.relative_url);
    if (absolute_url.path() == path) {
      scoped_ptr<HttpResponse> http_response(new HttpResponse);
      http_response->set_code(code);
      http_response->set_content(content);
      http_response->set_content_type(content_type);
      return http_response.Pass();
    }

    return scoped_ptr<HttpResponse>();
  }

 protected:
  int num_responses_received_;
  int num_responses_expected_;
  std::string request_relative_url_;
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
  HttpServer server_;
};

TEST_F(HttpServerTest, GetBaseURL) {
  EXPECT_EQ(base::StringPrintf("http://127.0.0.1:%d/", server_.port()),
                               server_.base_url().spec());
}

TEST_F(HttpServerTest, GetURL) {
  EXPECT_EQ(base::StringPrintf("http://127.0.0.1:%d/path?query=foo",
                               server_.port()),
            server_.GetURL("/path?query=foo").spec());
}

TEST_F(HttpServerTest, RegisterRequestHandler) {
  server_.RegisterRequestHandler(base::Bind(&HttpServerTest::HandleRequest,
                                            base::Unretained(this),
                                            "/test",
                                            "<b>Worked!</b>",
                                            "text/html",
                                            SUCCESS));

  scoped_ptr<net::URLFetcher> fetcher(
      net::URLFetcher::Create(server_.GetURL("/test?q=foo"),
                              net::URLFetcher::GET,
                              this));
  fetcher->SetRequestContext(request_context_getter_.get());
  fetcher->Start();
  WaitForResponses(1);

  EXPECT_EQ(net::URLRequestStatus::SUCCESS, fetcher->GetStatus().status());
  EXPECT_EQ(SUCCESS, fetcher->GetResponseCode());
  EXPECT_EQ("<b>Worked!</b>", GetContentFromFetcher(*fetcher));
  EXPECT_EQ("text/html", GetContentTypeFromFetcher(*fetcher));

  EXPECT_EQ("/test?q=foo", request_relative_url_);
}

TEST_F(HttpServerTest, DefaultNotFoundResponse) {
  scoped_ptr<net::URLFetcher> fetcher(
      net::URLFetcher::Create(server_.GetURL("/non-existent"),
                              net::URLFetcher::GET,
                              this));
  fetcher->SetRequestContext(request_context_getter_.get());

  fetcher->Start();
  WaitForResponses(1);
  EXPECT_EQ(net::URLRequestStatus::SUCCESS, fetcher->GetStatus().status());
  EXPECT_EQ(NOT_FOUND, fetcher->GetResponseCode());
}

TEST_F(HttpServerTest, ConcurrentFetches) {
  server_.RegisterRequestHandler(
      base::Bind(&HttpServerTest::HandleRequest,
                 base::Unretained(this),
                 "/test1",
                 "Raspberry chocolate",
                 "text/html",
                 SUCCESS));
  server_.RegisterRequestHandler(
      base::Bind(&HttpServerTest::HandleRequest,
                 base::Unretained(this),
                 "/test2",
                 "Vanilla chocolate",
                 "text/html",
                 SUCCESS));
  server_.RegisterRequestHandler(
      base::Bind(&HttpServerTest::HandleRequest,
                 base::Unretained(this),
                 "/test3",
                 "No chocolates",
                 "text/plain",
                 NOT_FOUND));

  scoped_ptr<net::URLFetcher> fetcher1 = scoped_ptr<net::URLFetcher>(
      net::URLFetcher::Create(server_.GetURL("/test1"),
                              net::URLFetcher::GET,
                              this));
  fetcher1->SetRequestContext(request_context_getter_.get());
  scoped_ptr<net::URLFetcher> fetcher2 = scoped_ptr<net::URLFetcher>(
      net::URLFetcher::Create(server_.GetURL("/test2"),
                              net::URLFetcher::GET,
                              this));
  fetcher2->SetRequestContext(request_context_getter_.get());
  scoped_ptr<net::URLFetcher> fetcher3 = scoped_ptr<net::URLFetcher>(
      net::URLFetcher::Create(server_.GetURL("/test3"),
                              net::URLFetcher::GET,
                              this));
  fetcher3->SetRequestContext(request_context_getter_.get());

  // Fetch the three URLs concurrently.
  fetcher1->Start();
  fetcher2->Start();
  fetcher3->Start();
  WaitForResponses(3);

  EXPECT_EQ(net::URLRequestStatus::SUCCESS, fetcher1->GetStatus().status());
  EXPECT_EQ(SUCCESS, fetcher1->GetResponseCode());
  EXPECT_EQ("Raspberry chocolate", GetContentFromFetcher(*fetcher1));
  EXPECT_EQ("text/html", GetContentTypeFromFetcher(*fetcher1));

  EXPECT_EQ(net::URLRequestStatus::SUCCESS, fetcher2->GetStatus().status());
  EXPECT_EQ(SUCCESS, fetcher2->GetResponseCode());
  EXPECT_EQ("Vanilla chocolate", GetContentFromFetcher(*fetcher2));
  EXPECT_EQ("text/html", GetContentTypeFromFetcher(*fetcher2));

  EXPECT_EQ(net::URLRequestStatus::SUCCESS, fetcher3->GetStatus().status());
  EXPECT_EQ(NOT_FOUND, fetcher3->GetResponseCode());
  EXPECT_EQ("No chocolates", GetContentFromFetcher(*fetcher3));
  EXPECT_EQ("text/plain", GetContentTypeFromFetcher(*fetcher3));
}

}  // namespace test_server
}  // namespace google_apis
