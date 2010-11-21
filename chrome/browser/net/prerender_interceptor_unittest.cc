// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/prerender_interceptor.h"

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/test/test_server.h"
#include "net/url_request/url_request_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_browser_net {

class PrerenderInterceptorTest : public testing::Test {
 protected:
  PrerenderInterceptorTest();

  void MakeTestUrl(const std::string& base);
  virtual void SetUp();

  net::TestServer test_server_;
  GURL gurl_;
  GURL last_intercepted_gurl_;
  scoped_ptr<URLRequest> req_;

 private:
  void SetLastInterceptedGurl(const GURL& url);

  PrerenderInterceptor prerender_interceptor_;
  MessageLoopForIO io_loop_;
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;
  BrowserThread ui_thread_;
  TestDelegate delegate_;
};

PrerenderInterceptorTest::PrerenderInterceptorTest()
    : test_server_(net::TestServer::TYPE_HTTP,
                   FilePath(FILE_PATH_LITERAL("chrome/test/data"))),
      last_intercepted_gurl_("http://not.initialized/"),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          prerender_interceptor_(
              NewCallback(this,
                          &PrerenderInterceptorTest::SetLastInterceptedGurl))),
      ui_thread_(BrowserThread::UI, &io_loop_) {
}

void PrerenderInterceptorTest::SetUp() {
  testing::Test::SetUp();
  last_intercepted_gurl_ = GURL("http://nothing.intercepted/");

  io_message_loop_proxy_ = base::MessageLoopProxy::CreateForCurrentThread();
  ASSERT_TRUE(test_server_.Start());
}

void PrerenderInterceptorTest::MakeTestUrl(const std::string& base) {
  gurl_ = test_server_.GetURL(base);
  req_.reset(new TestURLRequest(gurl_, &delegate_));
}

void PrerenderInterceptorTest::SetLastInterceptedGurl(const GURL& url) {
  last_intercepted_gurl_ = url;
}

namespace {

TEST_F(PrerenderInterceptorTest, Interception) {
  MakeTestUrl("files/prerender/doc1.html");
  req_->set_load_flags(req_->load_flags() | net::LOAD_PREFETCH);
  req_->Start();

  MessageLoop::current()->Run();
  EXPECT_EQ(URLRequestStatus::SUCCESS, req_->status().status());
  EXPECT_EQ(gurl_, last_intercepted_gurl_);
}

TEST_F(PrerenderInterceptorTest, NotAPrefetch) {
  MakeTestUrl("files/prerender/doc2.html");
  req_->set_load_flags(req_->load_flags() & ~net::LOAD_PREFETCH);
  req_->Start();

  MessageLoop::current()->Run();
  EXPECT_EQ(URLRequestStatus::SUCCESS, req_->status().status());
  EXPECT_NE(gurl_, last_intercepted_gurl_);
}

TEST_F(PrerenderInterceptorTest, WrongMimeType) {
  MakeTestUrl("files/prerender/image.jpeg");
  req_->set_load_flags(req_->load_flags() | net::LOAD_PREFETCH);
  req_->Start();

  MessageLoop::current()->Run();
  EXPECT_EQ(URLRequestStatus::SUCCESS, req_->status().status());
  EXPECT_NE(gurl_, last_intercepted_gurl_);
}

}  // namespace

}  // namespace chrome_browser_net
