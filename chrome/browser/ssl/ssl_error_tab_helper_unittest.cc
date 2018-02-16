// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_error_tab_helper.h"

#include <memory>

#include "base/bind.h"
#include "base/time/time.h"
#include "chrome/browser/interstitials/chrome_metrics_helper.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "net/base/net_errors.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "url/gurl.h"

namespace {

const char kTestSslMetricsName[] = "test_ssl_blocking_page";

std::unique_ptr<ChromeMetricsHelper> CreateTestSslMetricsHelper(
    content::WebContents* web_contents) {
  security_interstitials::MetricsHelper::ReportDetails report_details;
  report_details.metric_prefix = kTestSslMetricsName;
  return std::make_unique<ChromeMetricsHelper>(web_contents, GURL(),
                                               report_details);
}

class TestSSLBlockingPage : public SSLBlockingPage {
 public:
  // |*destroyed_tracker| is set to true in the destructor.
  TestSSLBlockingPage(content::WebContents* web_contents,
                      GURL request_url,
                      bool* destroyed_tracker)
      : SSLBlockingPage(
            web_contents,
            net::ERR_CERT_CONTAINS_ERRORS,
            net::SSLInfo(),
            request_url,
            0,
            base::Time::NowFromSystemTime(),
            GURL(),
            nullptr /* ssl_cert_reporter */,
            true /* overridable */,
            CreateTestSslMetricsHelper(web_contents),
            false /* is_superfish */,
            base::Callback<void(content::CertificateRequestResultType)>()),
        destroyed_tracker_(destroyed_tracker) {}

  ~TestSSLBlockingPage() override { *destroyed_tracker_ = true; }

 private:
  bool* destroyed_tracker_;
};

class SSLErrorTabHelperTest : public ChromeRenderViewHostTestHarness {
 protected:
  std::unique_ptr<content::NavigationHandle> CreateHandle(
      bool committed,
      bool is_same_document) {
    return content::NavigationHandle::CreateNavigationHandleForTesting(
        GURL(), main_rfh(), committed, net::OK, is_same_document);
  }

  // The lifetime of the blocking page is managed by the SSLErrorTabHelper for
  // the test's web_contents. |destroyed_tracker| will be set to true when the
  // corresponding blocking page is destroyed.
  void CreateAssociatedBlockingPage(content::NavigationHandle* handle,
                                    bool* destroyed_tracker) {
    SSLErrorTabHelper::AssociateBlockingPage(
        web_contents(), handle->GetNavigationId(),
        std::make_unique<TestSSLBlockingPage>(web_contents(), GURL(),
                                              destroyed_tracker));
  }
};

// Tests that the helper properly handles the lifetime of a single blocking
// page, interleaved with other navigations.
TEST_F(SSLErrorTabHelperTest, SingleBlockingPage) {
  std::unique_ptr<content::NavigationHandle> blocking_page_handle =
      CreateHandle(true, false);
  bool blocking_page_destroyed = false;
  CreateAssociatedBlockingPage(blocking_page_handle.get(),
                               &blocking_page_destroyed);
  SSLErrorTabHelper* helper =
      SSLErrorTabHelper::FromWebContents(web_contents());

  // Test that a same-document navigation doesn't destroy the blocking page if
  // its navigation hasn't committed yet.
  std::unique_ptr<content::NavigationHandle> same_document_handle =
      CreateHandle(true, true);
  helper->DidFinishNavigation(same_document_handle.get());
  EXPECT_FALSE(blocking_page_destroyed);

  // Test that a committed (non-same-document) navigation doesn't destroy the
  // blocking page if its navigation hasn't committed yet.
  std::unique_ptr<content::NavigationHandle> committed_handle1 =
      CreateHandle(true, false);
  helper->DidFinishNavigation(committed_handle1.get());
  EXPECT_FALSE(blocking_page_destroyed);

  // Simulate comitting the interstitial.
  helper->DidFinishNavigation(blocking_page_handle.get());
  EXPECT_FALSE(blocking_page_destroyed);

  // Test that a subsequent committed navigation releases the blocking page
  // stored for the currently committed navigation.
  std::unique_ptr<content::NavigationHandle> committed_handle2 =
      CreateHandle(true, false);
  helper->DidFinishNavigation(committed_handle2.get());
  EXPECT_TRUE(blocking_page_destroyed);
}

// Tests that the helper properly handles the lifetime of multiple blocking
// pages, committed in a different order than they are created.
TEST_F(SSLErrorTabHelperTest, MultipleBlockingPages) {
  // Simulate associating the first interstitial.
  std::unique_ptr<content::NavigationHandle> handle1 =
      CreateHandle(true, false);
  bool blocking_page1_destroyed = false;
  CreateAssociatedBlockingPage(handle1.get(), &blocking_page1_destroyed);

  // We can directly retrieve the helper for testing once
  // CreateAssociatedBlockingPage() was called.
  SSLErrorTabHelper* helper =
      SSLErrorTabHelper::FromWebContents(web_contents());

  // Simulate commiting the first interstitial.
  helper->DidFinishNavigation(handle1.get());
  EXPECT_FALSE(blocking_page1_destroyed);

  // Associate the second interstitial.
  std::unique_ptr<content::NavigationHandle> handle2 =
      CreateHandle(true, false);
  bool blocking_page2_destroyed = false;
  CreateAssociatedBlockingPage(handle2.get(), &blocking_page2_destroyed);
  EXPECT_FALSE(blocking_page1_destroyed);
  EXPECT_FALSE(blocking_page2_destroyed);

  // Associate the third interstitial.
  std::unique_ptr<content::NavigationHandle> handle3 =
      CreateHandle(true, false);
  bool blocking_page3_destroyed = false;
  CreateAssociatedBlockingPage(handle3.get(), &blocking_page3_destroyed);
  EXPECT_FALSE(blocking_page1_destroyed);
  EXPECT_FALSE(blocking_page2_destroyed);
  EXPECT_FALSE(blocking_page3_destroyed);

  // Simulate commiting the third interstitial.
  helper->DidFinishNavigation(handle3.get());
  EXPECT_TRUE(blocking_page1_destroyed);
  EXPECT_FALSE(blocking_page2_destroyed);
  EXPECT_FALSE(blocking_page3_destroyed);

  // Simulate commiting the second interstitial.
  helper->DidFinishNavigation(handle2.get());
  EXPECT_TRUE(blocking_page1_destroyed);
  EXPECT_FALSE(blocking_page2_destroyed);
  EXPECT_TRUE(blocking_page3_destroyed);
}

// Tests that the helper properly handles a navigation that finishes without
// committing.
TEST_F(SSLErrorTabHelperTest, NavigationDoesNotCommit) {
  std::unique_ptr<content::NavigationHandle> committed_handle =
      CreateHandle(true, false);
  bool committed_blocking_page_destroyed = false;
  CreateAssociatedBlockingPage(committed_handle.get(),
                               &committed_blocking_page_destroyed);
  SSLErrorTabHelper* helper =
      SSLErrorTabHelper::FromWebContents(web_contents());
  helper->DidFinishNavigation(committed_handle.get());
  EXPECT_FALSE(committed_blocking_page_destroyed);

  // Simulate a navigation that does not commit.
  std::unique_ptr<content::NavigationHandle> non_committed_handle =
      CreateHandle(false, false);
  bool non_committed_blocking_page_destroyed = false;
  CreateAssociatedBlockingPage(non_committed_handle.get(),
                               &non_committed_blocking_page_destroyed);
  helper->DidFinishNavigation(non_committed_handle.get());

  // The blocking page for the non-committed navigation should have been cleaned
  // up, but the one for the previous committed navigation should still be
  // around.
  EXPECT_TRUE(non_committed_blocking_page_destroyed);
  EXPECT_FALSE(committed_blocking_page_destroyed);

  // When a navigation does commit, the previous one should be cleaned up.
  std::unique_ptr<content::NavigationHandle> next_committed_handle =
      CreateHandle(true, false);
  helper->DidFinishNavigation(next_committed_handle.get());
  EXPECT_TRUE(committed_blocking_page_destroyed);
}

}  // namespace
