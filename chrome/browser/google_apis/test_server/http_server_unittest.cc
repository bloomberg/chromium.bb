// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/test_server/http_server.h"

#include "base/threading/thread.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace test_server {

namespace {

// Helper function to receive content of a response returned after invoking
// |fetcher|.
std::string GetFetcherResponseContent(const net::URLFetcher* fetcher) {
  std::string result;
  const bool success = fetcher->GetResponseAsString(&result);
  EXPECT_TRUE(success);
  return result;
}

}  // namespace

class HttpServerTest : public testing::Test,
                       public net::URLFetcherDelegate {
 public:
  HttpServerTest()
    : ui_thread_(content::BrowserThread::UI, &message_loop_),
      io_thread_(content::BrowserThread::IO) {
  }

  virtual void SetUp() OVERRIDE {
    io_thread_.StartIOThread();

    request_context_getter_ = new net::TestURLRequestContextGetter(
        content::BrowserThread::GetMessageLoopProxyForThread(
            content::BrowserThread::IO));

    server_.reset(new HttpServer());
    ASSERT_TRUE(server_->InitializeAndWaitUntilReady());
  }

  virtual void TearDown() OVERRIDE {
    server_->ShutdownAndWaitUntilComplete();
  }

  // net::URLFetcherDelegate override.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE {
    MessageLoop::current()->Quit();
  }

 protected:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
  scoped_ptr<HttpServer> server_;
};

TEST_F(HttpServerTest, TextRequest) {
  // The simplest text response with an auto generated url.
  GURL url1 = server_->RegisterTextResponse("test1",
                                            "Raspberry chocolate",
                                            "text/html",
                                            SUCCESS);
  ASSERT_NE("", url1.spec());

  GURL url2 = server_->RegisterTextResponse("test2",
                                            "Vanilla chocolate",
                                            "text/html",
                                            SUCCESS);
  ASSERT_NE("", url2.spec());

  // Response with a specified url and response code.
  GURL url3 = server_->RegisterTextResponse(
      "chocolate/bar.html",  // URL
      "No chocolates",  // Dummy response text.
      "text/plain",  // Content type.
      NOT_FOUND);  // Response code (404 here).
  ASSERT_NE("", url3.spec());

  // Set up fetchers.
  scoped_ptr<net::URLFetcher> fetcher1 = scoped_ptr<net::URLFetcher>(
      net::URLFetcher::Create(url1,
                              net::URLFetcher::GET,
                              this));
  fetcher1->SetRequestContext(request_context_getter_.get());
  scoped_ptr<net::URLFetcher> fetcher2 = scoped_ptr<net::URLFetcher>(
      net::URLFetcher::Create(url2,
                              net::URLFetcher::GET,
                              this));
  fetcher2->SetRequestContext(request_context_getter_.get());
  scoped_ptr<net::URLFetcher> fetcher3 = scoped_ptr<net::URLFetcher>(
      net::URLFetcher::Create(url3,
                              net::URLFetcher::GET,
                              this));
  fetcher3->SetRequestContext(request_context_getter_.get());

  // Test.
  fetcher1->Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(net::URLRequestStatus::SUCCESS, fetcher1->GetStatus().status());
  EXPECT_EQ(200, fetcher1->GetResponseCode());
  EXPECT_EQ("Raspberry chocolate", GetFetcherResponseContent(fetcher1.get()));

  fetcher2->Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(net::URLRequestStatus::SUCCESS, fetcher1->GetStatus().status());
  EXPECT_EQ(200, fetcher2->GetResponseCode());
  EXPECT_EQ("Vanilla chocolate", GetFetcherResponseContent(fetcher2.get()));

  fetcher3->Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(net::URLRequestStatus::SUCCESS, fetcher1->GetStatus().status());
  EXPECT_EQ(404, fetcher3->GetResponseCode());
  EXPECT_EQ("No chocolates", GetFetcherResponseContent(fetcher3.get()));
}

TEST_F(HttpServerTest, DefaultNotFoundResponse) {
  ASSERT_NE("", server_->GetBaseURL().spec());

  scoped_ptr<net::URLFetcher> fetcher = scoped_ptr<net::URLFetcher>(
      net::URLFetcher::Create(server_->GetBaseURL(),
                              net::URLFetcher::GET,
                              this));
  fetcher->SetRequestContext(request_context_getter_.get());

  fetcher->Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(net::URLRequestStatus::SUCCESS, fetcher->GetStatus().status());
  EXPECT_EQ(404, fetcher->GetResponseCode());
}

// TODO(mtomasz): Write a test for a file response.

}  // namespace test_server
}  // namespace drive
