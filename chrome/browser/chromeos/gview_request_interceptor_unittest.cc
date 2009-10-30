// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include "base/message_loop.h"
#include "chrome/browser/chromeos/gview_request_interceptor.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class GViewURLRequestTestJob : public URLRequestTestJob {
 public:
  explicit GViewURLRequestTestJob(URLRequest* request)
      : URLRequestTestJob(request, true) {
  }

  virtual bool GetMimeType(std::string* mime_type) const {
    // During the course of a single test, two URLRequestJobs are
    // created -- the first is for the viewable document URL, and the
    // second is for the rediected URL.  In order to test the
    // interceptor, the mime type of the first request must be one of
    // the supported viewable mime types.  So when the URLRequestJob
    // is a request for one of the test URLs that point to viewable
    // content, return an appropraite mime type.  Otherwise, return
    // "text/html".
    if (request_->url() == GURL("http://foo.com/file.pdf")) {
      *mime_type = "application/pdf";
    } else if (request_->url() == GURL("http://foo.com/file.ppt")) {
      *mime_type = "application/vnd.ms-powerpoint";
    } else {
      *mime_type = "text/html";
    }
    return true;
  }
};

class GViewRequestInterceptorTest : public testing::Test {
 public:
  virtual void SetUp() {
    URLRequest::RegisterProtocolFactory("http",
                                        &GViewRequestInterceptorTest::Factory);
    interceptor_ = GViewRequestInterceptor::GetGViewRequestInterceptor();
  }

  virtual void TearDown() {
    URLRequest::RegisterProtocolFactory("http", NULL);
    message_loop_.RunAllPending();
  }

  static URLRequestJob* Factory(URLRequest* request,
                                const std::string& scheme) {
    return new GViewURLRequestTestJob(request);
  }

 protected:
  MessageLoopForIO message_loop_;
  TestDelegate test_delegate_;
  URLRequest::Interceptor* interceptor_;
};

TEST_F(GViewRequestInterceptorTest, DoNotInterceptHtml) {
  URLRequest request(GURL("http://foo.com/index.html"), &test_delegate_);
  request.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(0, test_delegate_.received_redirect_count());
  EXPECT_EQ(GURL("http://foo.com/index.html"), request.url());
}

TEST_F(GViewRequestInterceptorTest, DoNotInterceptDownload) {
  URLRequest request(GURL("http://foo.com/file.pdf"), &test_delegate_);
  request.set_load_flags(net::LOAD_IS_DOWNLOAD);
  request.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(0, test_delegate_.received_redirect_count());
  EXPECT_EQ(GURL("http://foo.com/file.pdf"), request.url());
}

TEST_F(GViewRequestInterceptorTest, InterceptPdf) {
  URLRequest request(GURL("http://foo.com/file.pdf"), &test_delegate_);
  request.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(1, test_delegate_.received_redirect_count());
  EXPECT_EQ(GURL("http://docs.google.com/gview?url=http%3A//foo.com/file.pdf"),
                 request.url());
}

TEST_F(GViewRequestInterceptorTest, InterceptPowerpoint) {
  URLRequest request(GURL("http://foo.com/file.ppt"), &test_delegate_);
  request.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(1, test_delegate_.received_redirect_count());
  EXPECT_EQ(GURL("http://docs.google.com/gview?url=http%3A//foo.com/file.ppt"),
                 request.url());
}

}  // namespace chromeos
