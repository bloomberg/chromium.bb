// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_resource_handler.h"
#include "chrome/common/resource_response.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace prerender {

namespace {

class MockResourceHandler : public ResourceHandler {
 public:
  MockResourceHandler() {}

  virtual bool OnUploadProgress(int request_id,
                                uint64 position,
                                uint64 size) {
    return true;
  }

  virtual bool OnRequestRedirected(int request_id, const GURL& url,
                                   ResourceResponse* response,
                                   bool* defer) {
    *defer = false;
    return true;
  }

  virtual bool OnResponseStarted(int request_id,
                                 ResourceResponse* response) {
    return true;
  }

  virtual bool OnWillStart(int request_id, const GURL& url, bool* defer) {
    *defer = false;
    return true;
  }

  virtual bool OnWillRead(int request_id,
                          net::IOBuffer** buf,
                          int* buf_size,
                          int min_size) {
    return true;
  }

  virtual bool OnReadCompleted(int request_id, int* bytes_read) {
    return true;
  }

  virtual bool OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& security_info) {
    return true;
  }

  virtual void OnRequestClosed() {
  }

  virtual void OnDataDownloaded(int request_id, int bytes_downloaded) {}
};

// HttpResponseHeaders expects the raw input for it's constructor
// to be a NUL ('\0') separated string for each line. This is a little
// difficult to do for string literals, so this helper function accepts
// newline-separated string literals and does the substitution. The
// returned object is expected to be deleted by the caller.
net::HttpResponseHeaders* CreateResponseHeaders(
    const char* newline_separated_headers) {
  std::string headers(newline_separated_headers);
  std::string::iterator i = headers.begin();
  std::string::iterator end = headers.end();
  while (i != end) {
    if (*i == '\n')
      *i = '\0';
    ++i;
  }
  return new net::HttpResponseHeaders(headers);
}

}  // namespace

class PrerenderResourceHandlerTest : public testing::Test {
 protected:
  PrerenderResourceHandlerTest()
      : loop_(MessageLoop::TYPE_IO),
        ui_thread_(BrowserThread::UI, &loop_),
        io_thread_(BrowserThread::IO, &loop_),
        test_url_request_(GURL("http://www.referrer.com"),
                          &test_url_request_delegate_),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            pre_handler_(new PrerenderResourceHandler(
                test_url_request_,
                new MockResourceHandler(),
                NewCallback(
                    this,
                    &PrerenderResourceHandlerTest::SetLastHandledURL)))),
        default_url_("http://www.prerender.com") {
  }

  virtual ~PrerenderResourceHandlerTest() {
    // When a ResourceHandler's reference count drops to 0, it is not
    // deleted immediately. Instead, a task is posted to the IO thread's
    // message loop to delete it.
    // So, drop the reference count to 0 and run the message loop once
    // to ensure that all resources are cleaned up before the test exits.
    pre_handler_ = NULL;
    loop_.RunAllPending();
  }

  void SetLastHandledURL(const GURL& url, const std::vector<GURL>& alias_urls,
                         const GURL& referrer) {
    last_handled_url_ = url;
    alias_urls_ = alias_urls;
    referrer_ = referrer;
  }

  // Common logic shared by many of the tests
  void StartPrerendering(const std::string& mime_type,
                         const char* headers) {
    int request_id = 1;
    bool defer = false;
    EXPECT_TRUE(pre_handler_->OnWillStart(request_id, default_url_, &defer));
    EXPECT_FALSE(defer);
    scoped_refptr<ResourceResponse> response(new ResourceResponse);
    response->response_head.mime_type = mime_type;
    response->response_head.headers = CreateResponseHeaders(headers);
    EXPECT_TRUE(last_handled_url_.is_empty());

    // Start the response. If it is able to prerender, a task will
    // be posted to the UI thread and |SetLastHandledURL| will be called.
    EXPECT_TRUE(pre_handler_->OnResponseStarted(request_id, response));
    loop_.RunAllPending();
  }

  // Test whether a given URL is part of alias_urls_.
  bool ContainsAliasURL(const GURL& url) {
    return std::find(alias_urls_.begin(), alias_urls_.end(), url)
        != alias_urls_.end();
  }

  // Must be initialized before |test_url_request_|.
  MessageLoop loop_;
  BrowserThread ui_thread_;
  BrowserThread io_thread_;

  TestDelegate test_url_request_delegate_;
  TestURLRequest test_url_request_;

  scoped_refptr<PrerenderResourceHandler> pre_handler_;
  GURL last_handled_url_;
  GURL default_url_;
  std::vector<GURL> alias_urls_;
  GURL referrer_;
};

namespace {

TEST_F(PrerenderResourceHandlerTest, NoOp) {
}

// Tests that a valid HTML resource will correctly get diverted
// to the PrerenderManager.
TEST_F(PrerenderResourceHandlerTest, Prerender) {
  StartPrerendering("text/html",
                    "HTTP/1.1 200 OK\n");
  EXPECT_EQ(default_url_, last_handled_url_);
}

static const int kRequestId = 1;

// Tests that the final request in a redirect chain will
// get diverted to the PrerenderManager.
TEST_F(PrerenderResourceHandlerTest, PrerenderRedirect) {
  GURL url_redirect("http://www.redirect.com");
  bool defer = false;
  EXPECT_TRUE(pre_handler_->OnWillStart(kRequestId, default_url_, &defer));
  EXPECT_FALSE(defer);
  EXPECT_TRUE(pre_handler_->OnRequestRedirected(kRequestId,
                                                url_redirect,
                                                NULL,
                                                &defer));
  EXPECT_FALSE(defer);
  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  response->response_head.mime_type = "text/html";
  response->response_head.headers = CreateResponseHeaders(
      "HTTP/1.1 200 OK\n");
  EXPECT_TRUE(pre_handler_->OnResponseStarted(kRequestId, response));
  EXPECT_TRUE(last_handled_url_.is_empty());
  loop_.RunAllPending();
  EXPECT_EQ(url_redirect, last_handled_url_);
  EXPECT_EQ(true, ContainsAliasURL(url_redirect));
  EXPECT_EQ(true, ContainsAliasURL(default_url_));
  EXPECT_EQ(2, static_cast<int>(alias_urls_.size()));
}

// Tests that https requests will not be prerendered.
TEST_F(PrerenderResourceHandlerTest, PrerenderHttps) {
  GURL url_https("https://www.google.com");
  bool defer = false;
  EXPECT_FALSE(pre_handler_->OnWillStart(kRequestId, url_https, &defer));
  EXPECT_FALSE(defer);
}

TEST_F(PrerenderResourceHandlerTest, PrerenderRedirectToHttps) {
  bool defer = false;
  EXPECT_TRUE(pre_handler_->OnWillStart(kRequestId, default_url_, &defer));
  EXPECT_FALSE(defer);
  GURL url_https("https://www.google.com");
  EXPECT_FALSE(pre_handler_->OnRequestRedirected(kRequestId,
                                                 url_https,
                                                 NULL,
                                                 &defer));
  EXPECT_FALSE(defer);
}

}  // namespace

}  // namespace prerender
