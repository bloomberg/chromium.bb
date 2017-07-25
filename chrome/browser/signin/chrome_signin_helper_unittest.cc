// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_helper.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "components/signin/core/browser/scoped_account_consistency.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/signin/core/common/signin_features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
const GURL kGaiaUrl("https://accounts.google.com");
const char kDiceResponseHeader[] = "X-Chrome-ID-Consistency-Response";

// URLRequestInterceptor adding a Dice response header to Gaia responses.
class TestRequestInterceptor : public net::URLRequestInterceptor {
 public:
  ~TestRequestInterceptor() override {}

 private:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    std::string response_headers =
        base::StringPrintf("HTTP/1.1 200 OK\n\n%s: Foo\n", kDiceResponseHeader);
    return new net::URLRequestTestJob(request, network_delegate,
                                      response_headers, "", true);
  }
};

// Helper class managing the thread hopping between UI and IO.
class TestResponseHelper {
 public:
  TestResponseHelper() : success_(false) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  // Creates a URL request with the Dice response headers.
  void CreateRequestWithResponseHeaders() {
    success_ = false;
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(
            &TestResponseHelper::CreateRequestWithDiceResponseHeaderOnIO,
            base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(success_);

    success_ = false;
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(&TestResponseHelper::CheckHeaderSetOnIO,
                       base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(success_);
  }

  // Processes the Dice header and check that it was removed.
  void CheckDiceHeaderRemoved() {
    success_ = false;
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(&TestResponseHelper::CheckDiceHeaderRemovedOnIO,
                       base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(success_);
  }

 private:
  void CreateRequestWithDiceResponseHeaderOnIO() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    test_request_delegate_ = base::MakeUnique<net::TestDelegate>();
    request_ = url_request_context_.CreateRequest(
        kGaiaUrl, net::DEFAULT_PRIORITY, test_request_delegate_.get(),
        TRAFFIC_ANNOTATION_FOR_TESTS);
    content::ResourceRequestInfo::AllocateForTesting(
        request_.get(), content::RESOURCE_TYPE_MAIN_FRAME, nullptr, -1, -1, -1,
        true, false, false, true, content::PREVIEWS_OFF);
    net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
        kGaiaUrl, base::MakeUnique<TestRequestInterceptor>());
    request_->Start();
    success_ = true;
  }

  void CheckHeaderSetOnIO() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    net::HttpResponseHeaders* response_headers = request_->response_headers();
    ASSERT_TRUE(response_headers);
    ASSERT_TRUE(request_->response_headers()->HasHeader(kDiceResponseHeader));
    net::URLRequestFilter::GetInstance()->RemoveUrlHandler(kGaiaUrl);
    success_ = true;
  }

  void CheckDiceHeaderRemovedOnIO() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    // Process the header.
    signin::ProcessAccountConsistencyResponseHeaders(request_.get(), GURL(),
                                                     false /* is_incognito */);
    // Check that the header has been removed.
    EXPECT_FALSE(request_->response_headers()->HasHeader(kDiceResponseHeader));
    success_ = true;
  }

  net::TestURLRequestContext url_request_context_;
  std::unique_ptr<net::TestDelegate> test_request_delegate_;
  std::unique_ptr<net::URLRequest> request_;
  bool success_;
};
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace

class ChromeSigninHelperTest : public testing::Test {
 protected:
  ChromeSigninHelperTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}

  ~ChromeSigninHelperTest() override {}

  content::TestBrowserThreadBundle thread_bundle_;
};

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Tests that Dice response headers are removed after being processed.
TEST_F(ChromeSigninHelperTest, RemoveDiceSigninHeader) {
  signin::ScopedAccountConsistencyDice scoped_dice;

  TestResponseHelper test_response_helper;
  test_response_helper.CreateRequestWithResponseHeaders();
  test_response_helper.CheckDiceHeaderRemoved();
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
