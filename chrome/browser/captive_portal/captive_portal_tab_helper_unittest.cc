// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/captive_portal/captive_portal_tab_helper.h"

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/captive_portal/captive_portal_service.h"
#include "chrome/browser/captive_portal/captive_portal_tab_reloader.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using captive_portal::CaptivePortalResult;

namespace {

const char* const kStartUrl = "http://whatever.com/index.html";
const char* const kHttpUrl = "http://whatever.com/";
const char* const kHttpsUrl = "https://whatever.com/";

// Used for cross-process navigations.  Shouldn't actually matter whether this
// is different from kHttpsUrl, but best to keep things consistent.
const char* const kHttpsUrl2 = "https://cross_process.com/";

// Some navigations behave differently depending on if they're cross-process
// or not.
enum NavigationType {
  kSameProcess,
  kCrossProcess,
};

}  // namespace

class MockCaptivePortalTabReloader : public CaptivePortalTabReloader {
 public:
  MockCaptivePortalTabReloader()
      : CaptivePortalTabReloader(nullptr, nullptr, base::Callback<void()>()) {
  }

  MOCK_METHOD1(OnLoadStart, void(bool));
  MOCK_METHOD1(OnLoadCommitted, void(int));
  MOCK_METHOD0(OnAbort, void());
  MOCK_METHOD1(OnRedirect, void(bool));
  MOCK_METHOD2(OnCaptivePortalResults,
               void(CaptivePortalResult, CaptivePortalResult));
};

// Inherits from the ChromeRenderViewHostTestHarness to gain access to
// CreateTestWebContents.  Since the tests need to micromanage order of
// WebContentsObserver function calls, does not actually make sure of
// the harness in any other way.
class CaptivePortalTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  CaptivePortalTabHelperTest()
      : mock_reloader_(new testing::StrictMock<MockCaptivePortalTabReloader>) {}
  ~CaptivePortalTabHelperTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Load kStartUrl. This ensures that any subsequent navigation to kHttpsUrl2
    // will be properly registered as cross-process. It should be different than
    // the rest of the URLs used, otherwise unit tests will clasify navigations
    // as same document ones, which would be incorrect.
    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(web_contents());
    web_contents_tester->NavigateAndCommit(GURL(kStartUrl));
    content::RenderFrameHostTester* rfh_tester =
        content::RenderFrameHostTester::For(main_rfh());
    rfh_tester->SimulateNavigationStop();

    tab_helper_.reset(new CaptivePortalTabHelper(web_contents()));
    tab_helper_->profile_ = nullptr;
    tab_helper_->SetTabReloaderForTest(mock_reloader_);
  }

  void TearDown() override {
    tab_helper_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Simulates a successful load of |url|.
  void SimulateSuccess(const GURL& url) {
    EXPECT_CALL(mock_reloader(), OnLoadStart(url.SchemeIsCryptographic()))
        .Times(1);
    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(web_contents());
    web_contents_tester->StartNavigation(url);

    EXPECT_CALL(mock_reloader(), OnLoadCommitted(net::OK)).Times(1);
    content::RenderFrameHost* rfh =
        pending_main_rfh() ? pending_main_rfh() : main_rfh();
    content::RenderFrameHostTester::For(rfh)->SimulateNavigationCommit(url);
  }

  // Simulates a connection timeout while requesting |url|.
  void SimulateTimeout(const GURL& url) {
    EXPECT_CALL(mock_reloader(), OnLoadStart(url.SchemeIsCryptographic()))
        .Times(1);
    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(web_contents());
    web_contents_tester->StartNavigation(url);
    content::RenderFrameHost* rfh =
        pending_main_rfh() ? pending_main_rfh() : main_rfh();
    content::RenderFrameHostTester* rfh_tester =
        content::RenderFrameHostTester::For(rfh);

    rfh_tester->SimulateNavigationError(url, net::ERR_TIMED_OUT);

    EXPECT_CALL(mock_reloader(), OnLoadCommitted(net::ERR_TIMED_OUT)).Times(1);
    rfh_tester->SimulateNavigationErrorPageCommit();
  }

  // Simulates an abort while requesting |url|.
  void SimulateAbort(const GURL& url,
                     NavigationType navigation_type) {
    EXPECT_CALL(mock_reloader(), OnLoadStart(url.SchemeIsCryptographic()))
        .Times(1);
    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(web_contents());
    web_contents_tester->StartNavigation(url);
    DCHECK(navigation_type != kSameProcess || !pending_main_rfh());

    EXPECT_CALL(mock_reloader(), OnAbort()).Times(1);
    content::RenderFrameHost* rfh =
        navigation_type == kSameProcess ? main_rfh() : pending_main_rfh();
    content::RenderFrameHostTester* rfh_tester =
        content::RenderFrameHostTester::For(rfh);
    rfh_tester->SimulateNavigationError(url, net::ERR_ABORTED);
    // PlzNavigate: on abort, no renderer will have been associated with the
    // navigation. Therefore do not simulate a DidStopLoading IPC coming from a
    // renderer.
    if (!content::IsBrowserSideNavigationEnabled())
      rfh_tester->SimulateNavigationStop();

    // Make sure that above call resulted in abort, for tests that continue
    // after the abort.
    EXPECT_CALL(mock_reloader(), OnAbort()).Times(0);
  }

  // Simulates an abort while loading an error page.
  void SimulateAbortTimeout(const GURL& url,
                            NavigationType navigation_type) {
    EXPECT_CALL(mock_reloader(), OnLoadStart(url.SchemeIsCryptographic()))
        .Times(1);
    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(web_contents());
    web_contents_tester->StartNavigation(url);
    DCHECK(navigation_type != kSameProcess || !pending_main_rfh());

    EXPECT_CALL(mock_reloader(), OnAbort()).Times(1);
    content::RenderFrameHost* rfh =
        navigation_type == kSameProcess ? main_rfh() : pending_main_rfh();
    content::RenderFrameHostTester* rfh_tester =
        content::RenderFrameHostTester::For(rfh);
    rfh_tester->SimulateNavigationError(url, net::ERR_TIMED_OUT);
    rfh_tester->SimulateNavigationStop();

    // Make sure that above call resulted in abort, for tests that continue
    // after the abort.
    EXPECT_CALL(mock_reloader(), OnAbort()).Times(0);
  }

  CaptivePortalTabHelper* tab_helper() { return tab_helper_.get(); }

  // Simulates a captive portal redirect by calling the Observe method.
  void ObservePortalResult(CaptivePortalResult previous_result,
                           CaptivePortalResult result) {
    content::Source<Profile> source_profile(nullptr);

    CaptivePortalService::Results results;
    results.previous_result = previous_result;
    results.result = result;
    content::Details<CaptivePortalService::Results> details_results(&results);

    EXPECT_CALL(mock_reloader(), OnCaptivePortalResults(previous_result,
                                                        result)).Times(1);
    tab_helper()->Observe(chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT,
                          source_profile, details_results);
  }

  MockCaptivePortalTabReloader& mock_reloader() { return *mock_reloader_; }

  void SetIsLoginTab() { tab_helper()->SetIsLoginTab(); }

 private:
  std::unique_ptr<CaptivePortalTabHelper> tab_helper_;

  // Owned by |tab_helper_|.
  testing::StrictMock<MockCaptivePortalTabReloader>* mock_reloader_;

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalTabHelperTest);
};

TEST_F(CaptivePortalTabHelperTest, HttpSuccess) {
  SimulateSuccess(GURL(kHttpUrl));
  content::RenderFrameHostTester::For(main_rfh())->SimulateNavigationStop();
}

TEST_F(CaptivePortalTabHelperTest, HttpTimeout) {
  SimulateTimeout(GURL(kHttpUrl));
  content::RenderFrameHostTester::For(main_rfh())->SimulateNavigationStop();
}

TEST_F(CaptivePortalTabHelperTest, HttpsSuccess) {
  SimulateSuccess(GURL(kHttpsUrl));
  content::RenderFrameHostTester::For(main_rfh())->SimulateNavigationStop();
  EXPECT_FALSE(tab_helper()->IsLoginTab());
}

TEST_F(CaptivePortalTabHelperTest, HttpsTimeout) {
  SimulateTimeout(GURL(kHttpsUrl));
  // Make sure no state was carried over from the timeout.
  SimulateSuccess(GURL(kHttpsUrl));
  EXPECT_FALSE(tab_helper()->IsLoginTab());
}

TEST_F(CaptivePortalTabHelperTest, HttpsAbort) {
  SimulateAbort(GURL(kHttpsUrl), kCrossProcess);
  // Make sure no state was carried over from the abort.
  SimulateSuccess(GURL(kHttpsUrl));
  EXPECT_FALSE(tab_helper()->IsLoginTab());
}

// A cross-process navigation is aborted by a same-site navigation.
TEST_F(CaptivePortalTabHelperTest, AbortCrossProcess) {
  SimulateAbort(GURL(kHttpsUrl2), kCrossProcess);
  // Make sure no state was carried over from the abort.
  SimulateSuccess(GURL(kHttpUrl));
  EXPECT_FALSE(tab_helper()->IsLoginTab());
}

// Abort while there's a provisional timeout error page loading.
TEST_F(CaptivePortalTabHelperTest, HttpsAbortTimeout) {
  SimulateAbortTimeout(GURL(kHttpsUrl), kCrossProcess);
  // Make sure no state was carried over from the timeout or the abort.
  SimulateSuccess(GURL(kHttpsUrl));
  EXPECT_FALSE(tab_helper()->IsLoginTab());
}

// Abort a cross-process navigation while there's a provisional timeout error
// page loading.
TEST_F(CaptivePortalTabHelperTest, AbortTimeoutCrossProcess) {
  SimulateAbortTimeout(GURL(kHttpsUrl2), kCrossProcess);
  // Make sure no state was carried over from the timeout or the abort.
  SimulateSuccess(GURL(kHttpsUrl));
  EXPECT_FALSE(tab_helper()->IsLoginTab());
}

// Opposite case from above - a same-process error page is aborted in favor of
// a cross-process one.
TEST_F(CaptivePortalTabHelperTest, HttpsAbortTimeoutForCrossProcess) {
  SimulateSuccess(GURL(kHttpsUrl));
  content::RenderFrameHostTester::For(main_rfh())->SimulateNavigationStop();

  SimulateAbortTimeout(GURL(kHttpsUrl), kSameProcess);
  // Make sure no state was carried over from the timeout or the abort.
  SimulateSuccess(GURL(kHttpsUrl2));
  EXPECT_FALSE(tab_helper()->IsLoginTab());
}

// A provisional same-site navigation is interrupted by a cross-process
// navigation without sending an abort first.
TEST_F(CaptivePortalTabHelperTest, UnexpectedProvisionalLoad) {
  GURL same_site_url = GURL(kHttpUrl);
  GURL cross_process_url = GURL(kHttpsUrl2);

  // A same-site load for the original RenderViewHost starts.
  EXPECT_CALL(mock_reloader(),
              OnLoadStart(same_site_url.SchemeIsCryptographic())).Times(1);
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  rfh_tester->SimulateNavigationStart(same_site_url);

  // It's unexpectedly interrupted by a cross-process navigation, which starts
  // navigating before the old navigation cancels.
  EXPECT_CALL(mock_reloader(), OnAbort()).Times(1);
  EXPECT_CALL(mock_reloader(),
              OnLoadStart(cross_process_url.SchemeIsCryptographic())).Times(1);
  content::WebContentsTester::For(web_contents())
      ->StartNavigation(cross_process_url);

  // The cross-process navigation fails.
  content::RenderFrameHostTester* pending_rfh_tester =
      content::RenderFrameHostTester::For(pending_main_rfh());
  pending_rfh_tester->SimulateNavigationError(cross_process_url,
                                              net::ERR_FAILED);

  // The same-site navigation finally is aborted.
  rfh_tester->SimulateNavigationStop();

  EXPECT_CALL(mock_reloader(), OnLoadCommitted(net::ERR_FAILED)).Times(1);
  pending_rfh_tester->SimulateNavigationErrorPageCommit();
}

// Similar to the above test, except the original RenderViewHost manages to
// commit before its navigation is aborted.
TEST_F(CaptivePortalTabHelperTest, UnexpectedCommit) {
  GURL same_site_url = GURL(kHttpUrl);
  GURL cross_process_url = GURL(kHttpsUrl2);

  // A same-site load for the original RenderViewHost starts.
  EXPECT_CALL(mock_reloader(),
              OnLoadStart(same_site_url.SchemeIsCryptographic())).Times(1);
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  rfh_tester->SimulateNavigationStart(same_site_url);

  // It's unexpectedly interrupted by a cross-process navigation, which starts
  // navigating before the old navigation cancels.
  EXPECT_CALL(mock_reloader(), OnAbort()).Times(1);
  EXPECT_CALL(mock_reloader(),
              OnLoadStart(cross_process_url.SchemeIsCryptographic())).Times(1);
  content::WebContentsTester::For(web_contents())
      ->StartNavigation(cross_process_url);

  // The cross-process navigation fails.
  content::RenderFrameHostTester::For(pending_main_rfh())
      ->SimulateNavigationError(cross_process_url, net::ERR_FAILED);

  // The same-site navigation succeeds.
  EXPECT_CALL(mock_reloader(), OnAbort()).Times(1);
  EXPECT_CALL(mock_reloader(),
              OnLoadStart(same_site_url.SchemeIsCryptographic()))
      .Times(1);
  EXPECT_CALL(mock_reloader(), OnLoadCommitted(net::OK)).Times(1);
  rfh_tester->SimulateNavigationCommit(same_site_url);
}

// Simulates navigations for a number of subframes, and makes sure no
// CaptivePortalTabHelper function is called.
TEST_F(CaptivePortalTabHelperTest, HttpsSubframe) {
  GURL url = GURL(kHttpsUrl);

  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe1 = rfh_tester->AppendChild("subframe1");

  // Normal load.
  content::RenderFrameHostTester* subframe_tester1 =
      content::RenderFrameHostTester::For(subframe1);
  subframe_tester1->SimulateNavigationStart(url);
  subframe_tester1->SimulateNavigationCommit(url);
  subframe_tester1->SimulateNavigationStop();

  // Timeout.
  content::RenderFrameHost* subframe2 = rfh_tester->AppendChild("subframe2");
  content::RenderFrameHostTester* subframe_tester2 =
      content::RenderFrameHostTester::For(subframe2);
  subframe_tester2->SimulateNavigationStart(url);
  subframe_tester2->SimulateNavigationError(url, net::ERR_TIMED_OUT);
  subframe_tester2->SimulateNavigationStop();

  // Abort.
  content::RenderFrameHost* subframe3 = rfh_tester->AppendChild("subframe3");
  content::RenderFrameHostTester* subframe_tester3 =
      content::RenderFrameHostTester::For(subframe3);
  subframe_tester3->SimulateNavigationStart(url);
  subframe_tester3->SimulateNavigationError(url, net::ERR_ABORTED);
  subframe_tester3->SimulateNavigationStop();
}

// Simulates a subframe erroring out at the same time as a provisional load,
// but with a different error code.  Make sure the TabHelper sees the correct
// error.
TEST_F(CaptivePortalTabHelperTest, HttpsSubframeParallelError) {
  if (content::IsBrowserSideNavigationEnabled() &&
      content::AreAllSitesIsolatedForTesting()) {
    // http://crbug.com/674734 Fix this test with PlzNavigate and Site Isolation
    return;
  }
  // URL used by both frames.
  GURL url = GURL(kHttpsUrl);
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  content::RenderFrameHostTester* subframe_tester =
      content::RenderFrameHostTester::For(subframe);

  // Loads start.
  EXPECT_CALL(mock_reloader(), OnLoadStart(url.SchemeIsCryptographic()))
      .Times(1);
  rfh_tester->SimulateNavigationStart(url);
  subframe_tester->SimulateNavigationStart(url);

  // Loads return errors.
  rfh_tester->SimulateNavigationError(url, net::ERR_UNEXPECTED);
  subframe_tester->SimulateNavigationError(url, net::ERR_TIMED_OUT);

  // Error page load finishes.
  subframe_tester->SimulateNavigationErrorPageCommit();
  EXPECT_CALL(mock_reloader(), OnLoadCommitted(net::ERR_UNEXPECTED)).Times(1);
  rfh_tester->SimulateNavigationErrorPageCommit();
}

// Simulates an HTTP to HTTPS redirect, which then times out.
TEST_F(CaptivePortalTabHelperTest, HttpToHttpsRedirectTimeout) {
  if (content::IsBrowserSideNavigationEnabled() &&
      content::AreAllSitesIsolatedForTesting()) {
    // http://crbug.com/674734 Fix this test with PlzNavigate and Site Isolation
    return;
  }
  GURL http_url(kHttpUrl);
  EXPECT_CALL(mock_reloader(), OnLoadStart(false)).Times(1);
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  rfh_tester->SimulateNavigationStart(http_url);

  GURL https_url(kHttpsUrl);
  EXPECT_CALL(mock_reloader(), OnRedirect(true)).Times(1);
  rfh_tester->SimulateRedirect(https_url);

  rfh_tester->SimulateNavigationError(https_url, net::ERR_TIMED_OUT);

  EXPECT_CALL(mock_reloader(), OnLoadCommitted(net::ERR_TIMED_OUT)).Times(1);
  rfh_tester->SimulateNavigationErrorPageCommit();
}

// Simulates an HTTPS to HTTP redirect.
TEST_F(CaptivePortalTabHelperTest, HttpsToHttpRedirect) {
  GURL https_url(kHttpsUrl);
  EXPECT_CALL(mock_reloader(), OnLoadStart(https_url.SchemeIsCryptographic()))
      .Times(1);
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  rfh_tester->SimulateNavigationStart(https_url);

  GURL http_url(kHttpUrl);
  EXPECT_CALL(mock_reloader(), OnRedirect(http_url.SchemeIsCryptographic()))
      .Times(1);
  rfh_tester->SimulateRedirect(http_url);

  EXPECT_CALL(mock_reloader(), OnLoadCommitted(net::OK)).Times(1);
  rfh_tester->SimulateNavigationCommit(http_url);
}

// Simulates an HTTP to HTTP redirect.
TEST_F(CaptivePortalTabHelperTest, HttpToHttpRedirect) {
  GURL http_url(kHttpUrl);
  EXPECT_CALL(mock_reloader(), OnLoadStart(http_url.SchemeIsCryptographic()))
      .Times(1);
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  rfh_tester->SimulateNavigationStart(http_url);

  EXPECT_CALL(mock_reloader(), OnRedirect(http_url.SchemeIsCryptographic()))
      .Times(1);
  rfh_tester->SimulateRedirect(http_url);

  EXPECT_CALL(mock_reloader(), OnLoadCommitted(net::OK)).Times(1);
  rfh_tester->SimulateNavigationCommit(http_url);
}

// Tests that a subframe redirect doesn't reset the timer to kick off a captive
// portal probe for the main frame if the main frame request is taking too long.
TEST_F(CaptivePortalTabHelperTest, SubframeRedirect) {
  GURL http_url(kHttpUrl);
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  content::RenderFrameHostTester* subframe_tester =
      content::RenderFrameHostTester::For(subframe);

  EXPECT_CALL(mock_reloader(), OnLoadStart(false)).Times(1);
  rfh_tester->SimulateNavigationStart(http_url);
  subframe_tester->SimulateNavigationStart(http_url);

  GURL https_url(kHttpsUrl);
  subframe_tester->SimulateRedirect(https_url);

  EXPECT_CALL(mock_reloader(), OnLoadCommitted(net::OK)).Times(1);
  rfh_tester->SimulateNavigationCommit(http_url);
}

TEST_F(CaptivePortalTabHelperTest, LoginTabLogin) {
  EXPECT_FALSE(tab_helper()->IsLoginTab());
  SetIsLoginTab();
  EXPECT_TRUE(tab_helper()->IsLoginTab());

  ObservePortalResult(captive_portal::RESULT_INTERNET_CONNECTED,
                      captive_portal::RESULT_INTERNET_CONNECTED);
  EXPECT_FALSE(tab_helper()->IsLoginTab());
}

TEST_F(CaptivePortalTabHelperTest, LoginTabError) {
  EXPECT_FALSE(tab_helper()->IsLoginTab());

  SetIsLoginTab();
  EXPECT_TRUE(tab_helper()->IsLoginTab());

  ObservePortalResult(captive_portal::RESULT_INTERNET_CONNECTED,
                      captive_portal::RESULT_NO_RESPONSE);
  EXPECT_FALSE(tab_helper()->IsLoginTab());
}

TEST_F(CaptivePortalTabHelperTest, LoginTabMultipleResultsBeforeLogin) {
  EXPECT_FALSE(tab_helper()->IsLoginTab());

  SetIsLoginTab();
  EXPECT_TRUE(tab_helper()->IsLoginTab());

  ObservePortalResult(captive_portal::RESULT_INTERNET_CONNECTED,
                      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_TRUE(tab_helper()->IsLoginTab());

  ObservePortalResult(captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
                      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_TRUE(tab_helper()->IsLoginTab());

  ObservePortalResult(captive_portal::RESULT_NO_RESPONSE,
                      captive_portal::RESULT_INTERNET_CONNECTED);
  EXPECT_FALSE(tab_helper()->IsLoginTab());
}

TEST_F(CaptivePortalTabHelperTest, NoLoginTab) {
  EXPECT_FALSE(tab_helper()->IsLoginTab());

  ObservePortalResult(captive_portal::RESULT_INTERNET_CONNECTED,
                      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_FALSE(tab_helper()->IsLoginTab());

  ObservePortalResult(captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
                      captive_portal::RESULT_NO_RESPONSE);
  EXPECT_FALSE(tab_helper()->IsLoginTab());

  ObservePortalResult(captive_portal::RESULT_NO_RESPONSE,
                      captive_portal::RESULT_INTERNET_CONNECTED);
  EXPECT_FALSE(tab_helper()->IsLoginTab());
}
