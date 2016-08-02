// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/chrome_data_use_group.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/test/test_utils.h"
#include "net/base/network_delegate.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job.h"
#include "testing/gtest/include/gtest/gtest.h"

// Intercepter to confirm that a render process ID and a render frame ID are
// set on URL requests.
class ConfirmFrameIdInterceptor : public net::URLRequestInterceptor {
 public:
  ConfirmFrameIdInterceptor() : did_intercept_request_(false) {
  }

  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    did_intercept_request_ = true;

    int render_process_id;
    int render_frame_id;
    EXPECT_TRUE(content::ResourceRequestInfo::GetRenderFrameForRequest(
        request, &render_process_id, &render_frame_id));
    return nullptr;
  }

  bool did_intercept_request() {
    return did_intercept_request_;
  }

 private:
  mutable bool did_intercept_request_;
};

class ChromeDataUseGroupBrowserTest : public InProcessBrowserTest {
 public:
  ChromeDataUseGroupBrowserTest() : interceptor_(nullptr) {
  }

  void SetUpOnMainThread() override {
    url_ = net::URLRequestMockHTTPJob::GetMockUrl("index.html");

    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&ChromeDataUseGroupBrowserTest::SetUpOnIOThread,
             base::Unretained(this)));
    RunAllPendingInMessageLoop(content::BrowserThread::IO);
  }

  void TearDownOnMainThread() override {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&ChromeDataUseGroupBrowserTest::TearDownOnIOThread,
             base::Unretained(this)));
    RunAllPendingInMessageLoop(content::BrowserThread::IO);
  }

  void SetUpOnIOThread() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    interceptor_ = new ConfirmFrameIdInterceptor();
    net::URLRequestFilter::GetInstance()->AddUrlInterceptor(url_,
        base::WrapUnique(interceptor_));
  }

  void TearDownOnIOThread() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    interceptor_ = nullptr;
    net::URLRequestFilter::GetInstance()->RemoveUrlHandler(url_);
  }

 protected:
  GURL& url() {
    return url_;
  }

  bool did_intercept_request() {
    return interceptor_->did_intercept_request();
  }

 private:
  ConfirmFrameIdInterceptor* interceptor_;
  GURL url_;
};

// Confirms that URL requests have a valid |render_process_id| and
// |render_frame_id| set on them before they begin. This protects against
// any regressions that may be caused as we move to PlzNavigate.
IN_PROC_BROWSER_TEST_F(ChromeDataUseGroupBrowserTest, ExistsFrameId) {
  EXPECT_FALSE(did_intercept_request());
  ui_test_utils::NavigateToURL(browser(), url());
  EXPECT_TRUE(did_intercept_request());
}
