// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/captive_portal/captive_portal_tab_helper.h"

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/captive_portal/captive_portal_service.h"
#include "chrome/browser/captive_portal/captive_portal_tab_reloader.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using captive_portal::CaptivePortalResult;
using content::ResourceType;

namespace {

const char* const kHttpUrl = "http://whatever.com/";
const char* const kHttpsUrl = "https://whatever.com/";

// Used for cross-process navigations.  Shouldn't actually matter whether this
// is different from kHttpsUrl, but best to keep things consistent.
const char* const kHttpsUrl2 = "https://cross_process.com/";

// Error pages use a "data:" URL.  Shouldn't actually matter what this is.
const char* const kErrorPageUrl = "data:blah";

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
      : CaptivePortalTabReloader(NULL, NULL, base::Callback<void()>()) {
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
      : tab_helper_(NULL),
        mock_reloader_(new testing::StrictMock<MockCaptivePortalTabReloader>) {
    tab_helper_.SetTabReloaderForTest(mock_reloader_);
  }
  virtual ~CaptivePortalTabHelperTest() {}

  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
    web_contents1_.reset(CreateTestWebContents());
    web_contents2_.reset(CreateTestWebContents());
  }

  virtual void TearDown() OVERRIDE {
    web_contents2_.reset(NULL);
    web_contents1_.reset(NULL);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Simulates a successful load of |url|.
  void SimulateSuccess(const GURL& url,
                       content::RenderViewHost* render_view_host) {
    EXPECT_CALL(mock_reloader(), OnLoadStart(url.SchemeIsSecure())).Times(1);
    tab_helper().DidStartProvisionalLoadForFrame(
        render_view_host->GetMainFrame(), url, false, false);

    EXPECT_CALL(mock_reloader(), OnLoadCommitted(net::OK)).Times(1);
    tab_helper().DidCommitProvisionalLoadForFrame(
        render_view_host->GetMainFrame(),
        url,
        ui::PAGE_TRANSITION_LINK);
  }

  // Simulates a connection timeout while requesting |url|.
  void SimulateTimeout(const GURL& url,
                       content::RenderViewHost* render_view_host) {
    EXPECT_CALL(mock_reloader(), OnLoadStart(url.SchemeIsSecure())).Times(1);
    tab_helper().DidStartProvisionalLoadForFrame(
        render_view_host->GetMainFrame(), url, false, false);

    tab_helper().DidFailProvisionalLoad(render_view_host->GetMainFrame(),
                                        url,
                                        net::ERR_TIMED_OUT,
                                        base::string16());

    // Provisional load starts for the error page.
    tab_helper().DidStartProvisionalLoadForFrame(
        render_view_host->GetMainFrame(), GURL(kErrorPageUrl), true, false);

    EXPECT_CALL(mock_reloader(), OnLoadCommitted(net::ERR_TIMED_OUT)).Times(1);
    tab_helper().DidCommitProvisionalLoadForFrame(
        render_view_host->GetMainFrame(),
        GURL(kErrorPageUrl),
        ui::PAGE_TRANSITION_LINK);
  }

  // Simulates an abort while requesting |url|.
  void SimulateAbort(const GURL& url,
                     content::RenderViewHost* render_view_host,
                     NavigationType navigation_type) {
    EXPECT_CALL(mock_reloader(), OnLoadStart(url.SchemeIsSecure())).Times(1);
    tab_helper().DidStartProvisionalLoadForFrame(
        render_view_host->GetMainFrame(), url, false, false);

    EXPECT_CALL(mock_reloader(), OnAbort()).Times(1);
    if (navigation_type == kSameProcess) {
      tab_helper().DidFailProvisionalLoad(render_view_host->GetMainFrame(),
                                          url,
                                          net::ERR_ABORTED,
                                          base::string16());
    } else {
      // For interrupted provisional cross-process navigations, the
      // RenderViewHost is destroyed without sending a DidFailProvisionalLoad
      // notification.
      tab_helper().RenderViewDeleted(render_view_host);
    }

    // Make sure that above call resulted in abort, for tests that continue
    // after the abort.
    EXPECT_CALL(mock_reloader(), OnAbort()).Times(0);
  }

  // Simulates an abort while loading an error page.
  void SimulateAbortTimeout(const GURL& url,
                            content::RenderViewHost* render_view_host,
                            NavigationType navigation_type) {
    EXPECT_CALL(mock_reloader(), OnLoadStart(url.SchemeIsSecure())).Times(1);
    tab_helper().DidStartProvisionalLoadForFrame(
        render_view_host->GetMainFrame(), url, false, false);

    tab_helper().DidFailProvisionalLoad(render_view_host->GetMainFrame(),
                                        url,
                                        net::ERR_TIMED_OUT,
                                        base::string16());

    // Start event for the error page.
    tab_helper().DidStartProvisionalLoadForFrame(
        render_view_host->GetMainFrame(), url, true, false);

    EXPECT_CALL(mock_reloader(), OnAbort()).Times(1);
    if (navigation_type == kSameProcess) {
      tab_helper().DidFailProvisionalLoad(render_view_host->GetMainFrame(),
                                          url,
                                          net::ERR_ABORTED,
                                          base::string16());
    } else {
      // For interrupted provisional cross-process navigations, the
      // RenderViewHost is destroyed without sending a DidFailProvisionalLoad
      // notification.
      tab_helper().RenderViewDeleted(render_view_host);
    }

    // Make sure that above call resulted in abort, for tests that continue
    // after the abort.
    EXPECT_CALL(mock_reloader(), OnAbort()).Times(0);
  }

  CaptivePortalTabHelper& tab_helper() {
    return tab_helper_;
  }

  // Simulates a captive portal redirect by calling the Observe method.
  void ObservePortalResult(CaptivePortalResult previous_result,
                           CaptivePortalResult result) {
    content::Source<Profile> source_profile(NULL);

    CaptivePortalService::Results results;
    results.previous_result = previous_result;
    results.result = result;
    content::Details<CaptivePortalService::Results> details_results(&results);

    EXPECT_CALL(mock_reloader(), OnCaptivePortalResults(previous_result,
                                                        result)).Times(1);
    tab_helper().Observe(chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT,
                         source_profile,
                         details_results);
  }

  // Simulates a redirect.  Uses OnRedirect rather than Observe, for simplicity.
  void OnRedirect(ResourceType type, const GURL& new_url, int child_id) {
    tab_helper().OnRedirect(child_id, type, new_url);
  }

  MockCaptivePortalTabReloader& mock_reloader() { return *mock_reloader_; }

  void SetIsLoginTab() {
    tab_helper().SetIsLoginTab();
  }

  content::RenderViewHost* render_view_host1() {
    return web_contents1_->GetRenderViewHost();
  }

  content::RenderViewHost* render_view_host2() {
    return web_contents2_->GetRenderViewHost();
  }

  content::RenderFrameHost* main_render_frame1() {
    return web_contents1_->GetMainFrame();
  }

  content::RenderFrameHost* main_render_frame2() {
    return web_contents2_->GetMainFrame();
  }

 private:
  // Only the RenderViewHosts are used.
  scoped_ptr<content::WebContents> web_contents1_;
  scoped_ptr<content::WebContents> web_contents2_;

  CaptivePortalTabHelper tab_helper_;

  // Owned by |tab_helper_|.
  testing::StrictMock<MockCaptivePortalTabReloader>* mock_reloader_;

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalTabHelperTest);
};

TEST_F(CaptivePortalTabHelperTest, HttpSuccess) {
  SimulateSuccess(GURL(kHttpUrl), render_view_host1());
  tab_helper().DidStopLoading(render_view_host1());
}

TEST_F(CaptivePortalTabHelperTest, HttpTimeout) {
  SimulateTimeout(GURL(kHttpUrl), render_view_host1());
  tab_helper().DidStopLoading(render_view_host1());
}

// Same as above, but simulates what happens when the Link Doctor is enabled,
// which adds another provisional load/commit for the error page, after the
// first two.
TEST_F(CaptivePortalTabHelperTest, HttpTimeoutLinkDoctor) {
  SimulateTimeout(GURL(kHttpUrl), render_view_host1());

  EXPECT_CALL(mock_reloader(), OnLoadStart(false)).Times(1);
  // Provisional load starts for the error page.
  tab_helper().DidStartProvisionalLoadForFrame(
      main_render_frame1(), GURL(kErrorPageUrl), true, false);

  EXPECT_CALL(mock_reloader(), OnLoadCommitted(net::OK)).Times(1);
  tab_helper().DidCommitProvisionalLoadForFrame(main_render_frame1(),
                                                GURL(kErrorPageUrl),
                                                ui::PAGE_TRANSITION_LINK);
  tab_helper().DidStopLoading(render_view_host1());
}

TEST_F(CaptivePortalTabHelperTest, HttpsSuccess) {
  SimulateSuccess(GURL(kHttpsUrl), render_view_host1());
  tab_helper().DidStopLoading(render_view_host1());
  EXPECT_FALSE(tab_helper().IsLoginTab());
}

TEST_F(CaptivePortalTabHelperTest, HttpsTimeout) {
  SimulateTimeout(GURL(kHttpsUrl), render_view_host1());
  // Make sure no state was carried over from the timeout.
  SimulateSuccess(GURL(kHttpsUrl), render_view_host1());
  EXPECT_FALSE(tab_helper().IsLoginTab());
}

TEST_F(CaptivePortalTabHelperTest, HttpsAbort) {
  SimulateAbort(GURL(kHttpsUrl), render_view_host1(), kSameProcess);
  // Make sure no state was carried over from the abort.
  SimulateSuccess(GURL(kHttpsUrl), render_view_host1());
  EXPECT_FALSE(tab_helper().IsLoginTab());
}

// A cross-process navigation is aborted by a same-site navigation.
TEST_F(CaptivePortalTabHelperTest, AbortCrossProcess) {
  SimulateAbort(GURL(kHttpsUrl2), render_view_host2(), kCrossProcess);
  // Make sure no state was carried over from the abort.
  SimulateSuccess(GURL(kHttpUrl), render_view_host1());
  EXPECT_FALSE(tab_helper().IsLoginTab());
}

// Abort while there's a provisional timeout error page loading.
TEST_F(CaptivePortalTabHelperTest, HttpsAbortTimeout) {
  SimulateAbortTimeout(GURL(kHttpsUrl), render_view_host1(), kSameProcess);
  // Make sure no state was carried over from the timeout or the abort.
  SimulateSuccess(GURL(kHttpsUrl), render_view_host1());
  EXPECT_FALSE(tab_helper().IsLoginTab());
}

// Abort a cross-process navigation while there's a provisional timeout error
// page loading.
TEST_F(CaptivePortalTabHelperTest, AbortTimeoutCrossProcess) {
  SimulateAbortTimeout(GURL(kHttpsUrl2), render_view_host2(),
                       kCrossProcess);
  // Make sure no state was carried over from the timeout or the abort.
  SimulateSuccess(GURL(kHttpsUrl), render_view_host1());
  EXPECT_FALSE(tab_helper().IsLoginTab());
}

// Opposite case from above - a same-process error page is aborted in favor of
// a cross-process one.
TEST_F(CaptivePortalTabHelperTest, HttpsAbortTimeoutForCrossProcess) {
  SimulateAbortTimeout(GURL(kHttpsUrl), render_view_host1(), kSameProcess);
  // Make sure no state was carried over from the timeout or the abort.
  SimulateSuccess(GURL(kHttpsUrl2), render_view_host2());
  EXPECT_FALSE(tab_helper().IsLoginTab());
}

// A provisional same-site navigation is interrupted by a cross-process
// navigation without sending an abort first.
TEST_F(CaptivePortalTabHelperTest, UnexpectedProvisionalLoad) {
  GURL same_site_url = GURL(kHttpUrl);
  GURL cross_process_url = GURL(kHttpsUrl2);

  // A same-site load for the original RenderViewHost starts.
  EXPECT_CALL(mock_reloader(),
              OnLoadStart(same_site_url.SchemeIsSecure())).Times(1);
  tab_helper().DidStartProvisionalLoadForFrame(
      main_render_frame1(), same_site_url, false, false);

  // It's unexpectedly interrupted by a cross-process navigation, which starts
  // navigating before the old navigation cancels.  We generate an abort message
  // for the old navigation.
  EXPECT_CALL(mock_reloader(), OnAbort()).Times(1);
  EXPECT_CALL(mock_reloader(),
              OnLoadStart(cross_process_url.SchemeIsSecure())).Times(1);
  tab_helper().DidStartProvisionalLoadForFrame(
      main_render_frame2(), cross_process_url, false, false);

  // The cross-process navigation fails.
  tab_helper().DidFailProvisionalLoad(main_render_frame2(),
                                      cross_process_url,
                                      net::ERR_FAILED,
                                      base::string16());

  // The same-site navigation finally is aborted.
  tab_helper().DidFailProvisionalLoad(main_render_frame1(),
                                      same_site_url,
                                      net::ERR_ABORTED,
                                      base::string16());

  // The provisional load starts for the error page for the cross-process
  // navigation.
  tab_helper().DidStartProvisionalLoadForFrame(
      main_render_frame2(), GURL(kErrorPageUrl), true, false);

  EXPECT_CALL(mock_reloader(), OnLoadCommitted(net::ERR_FAILED)).Times(1);
  tab_helper().DidCommitProvisionalLoadForFrame(main_render_frame2(),
                                                GURL(kErrorPageUrl),
                                                ui::PAGE_TRANSITION_TYPED);
}

// Similar to the above test, except the original RenderViewHost manages to
// commit before its navigation is aborted.
TEST_F(CaptivePortalTabHelperTest, UnexpectedCommit) {
  GURL same_site_url = GURL(kHttpUrl);
  GURL cross_process_url = GURL(kHttpsUrl2);

  // A same-site load for the original RenderViewHost starts.
  EXPECT_CALL(mock_reloader(),
              OnLoadStart(same_site_url.SchemeIsSecure())).Times(1);
  tab_helper().DidStartProvisionalLoadForFrame(
      main_render_frame1(), same_site_url, false, false);

  // It's unexpectedly interrupted by a cross-process navigation, which starts
  // navigating before the old navigation cancels.  We generate an abort message
  // for the old navigation.
  EXPECT_CALL(mock_reloader(), OnAbort()).Times(1);
  EXPECT_CALL(mock_reloader(),
              OnLoadStart(cross_process_url.SchemeIsSecure())).Times(1);
  tab_helper().DidStartProvisionalLoadForFrame(
      main_render_frame2(), cross_process_url, false, false);

  // The cross-process navigation fails.
  tab_helper().DidFailProvisionalLoad(main_render_frame2(),
                                      cross_process_url,
                                      net::ERR_FAILED,
                                      base::string16());

  // The same-site navigation succeeds.
  EXPECT_CALL(mock_reloader(), OnAbort()).Times(1);
  EXPECT_CALL(mock_reloader(),
              OnLoadStart(same_site_url.SchemeIsSecure())).Times(1);
  EXPECT_CALL(mock_reloader(), OnLoadCommitted(net::OK)).Times(1);
  tab_helper().DidCommitProvisionalLoadForFrame(
      main_render_frame1(), same_site_url, ui::PAGE_TRANSITION_LINK);
}

// Simulates navigations for a number of subframes, and makes sure no
// CaptivePortalTabHelper function is called.
TEST_F(CaptivePortalTabHelperTest, HttpsSubframe) {
  GURL url = GURL(kHttpsUrl);

  content::RenderFrameHostTester* render_frame_host_tester =
      content::RenderFrameHostTester::For(main_render_frame1());
  content::RenderFrameHost* subframe1 =
      render_frame_host_tester->AppendChild("subframe1");

  // Normal load.
  tab_helper().DidStartProvisionalLoadForFrame(subframe1, url, false, false);
  tab_helper().DidCommitProvisionalLoadForFrame(
      subframe1, url, ui::PAGE_TRANSITION_LINK);

  // Timeout.
  content::RenderFrameHost* subframe2 =
      render_frame_host_tester->AppendChild("subframe2");
  tab_helper().DidStartProvisionalLoadForFrame(subframe2, url, false, false);
  tab_helper().DidFailProvisionalLoad(
      subframe2, url, net::ERR_TIMED_OUT, base::string16());
  tab_helper().DidStartProvisionalLoadForFrame(subframe2, url, true, false);
  tab_helper().DidFailProvisionalLoad(
      subframe2, url, net::ERR_ABORTED, base::string16());

  // Abort.
  content::RenderFrameHost* subframe3 =
      render_frame_host_tester->AppendChild("subframe3");
  tab_helper().DidStartProvisionalLoadForFrame(subframe3, url, false, false);
  tab_helper().DidFailProvisionalLoad(
      subframe3, url, net::ERR_ABORTED, base::string16());
}

// Simulates a subframe erroring out at the same time as a provisional load,
// but with a different error code.  Make sure the TabHelper sees the correct
// error.
TEST_F(CaptivePortalTabHelperTest, HttpsSubframeParallelError) {
  // URL used by both frames.
  GURL url = GURL(kHttpsUrl);
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_render_frame1())
          ->AppendChild("subframe");

  // Loads start.
  EXPECT_CALL(mock_reloader(), OnLoadStart(url.SchemeIsSecure())).Times(1);
  tab_helper().DidStartProvisionalLoadForFrame(
      main_render_frame1(), url, false, false);
  tab_helper().DidStartProvisionalLoadForFrame(subframe, url, false, false);

  // Loads return errors.
  tab_helper().DidFailProvisionalLoad(
      main_render_frame1(), url, net::ERR_UNEXPECTED, base::string16());
  tab_helper().DidFailProvisionalLoad(
      subframe, url, net::ERR_TIMED_OUT, base::string16());

  // Provisional load starts for the error pages.
  tab_helper().DidStartProvisionalLoadForFrame(
      main_render_frame1(), url, true, false);
  tab_helper().DidStartProvisionalLoadForFrame(subframe, url, true, false);

  // Error page load finishes.
  tab_helper().DidCommitProvisionalLoadForFrame(
      subframe, url, ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  EXPECT_CALL(mock_reloader(), OnLoadCommitted(net::ERR_UNEXPECTED)).Times(1);
  tab_helper().DidCommitProvisionalLoadForFrame(
      main_render_frame1(), url, ui::PAGE_TRANSITION_LINK);
}

// Simulates an HTTP to HTTPS redirect, which then times out.
TEST_F(CaptivePortalTabHelperTest, HttpToHttpsRedirectTimeout) {
  GURL http_url(kHttpUrl);
  EXPECT_CALL(mock_reloader(), OnLoadStart(false)).Times(1);
  tab_helper().DidStartProvisionalLoadForFrame(
      main_render_frame1(), http_url, false, false);

  GURL https_url(kHttpsUrl);
  EXPECT_CALL(mock_reloader(), OnRedirect(true)).Times(1);
  OnRedirect(content::RESOURCE_TYPE_MAIN_FRAME,
             https_url,
             render_view_host1()->GetProcess()->GetID());

  tab_helper().DidFailProvisionalLoad(main_render_frame1(),
                                      https_url,
                                      net::ERR_TIMED_OUT,
                                      base::string16());

  // Provisional load starts for the error page.
  tab_helper().DidStartProvisionalLoadForFrame(
      main_render_frame1(), GURL(kErrorPageUrl), true, false);

  EXPECT_CALL(mock_reloader(), OnLoadCommitted(net::ERR_TIMED_OUT)).Times(1);
  tab_helper().DidCommitProvisionalLoadForFrame(main_render_frame1(),
                                                GURL(kErrorPageUrl),
                                                ui::PAGE_TRANSITION_LINK);
}

// Simulates an HTTPS to HTTP redirect.
TEST_F(CaptivePortalTabHelperTest, HttpsToHttpRedirect) {
  GURL https_url(kHttpsUrl);
  EXPECT_CALL(mock_reloader(),
              OnLoadStart(https_url.SchemeIsSecure())).Times(1);
  tab_helper().DidStartProvisionalLoadForFrame(
      main_render_frame1(), https_url, false, false);

  GURL http_url(kHttpUrl);
  EXPECT_CALL(mock_reloader(), OnRedirect(http_url.SchemeIsSecure())).Times(1);
  OnRedirect(content::RESOURCE_TYPE_MAIN_FRAME, http_url,
             render_view_host1()->GetProcess()->GetID());

  EXPECT_CALL(mock_reloader(), OnLoadCommitted(net::OK)).Times(1);
  tab_helper().DidCommitProvisionalLoadForFrame(
      main_render_frame1(), http_url, ui::PAGE_TRANSITION_LINK);
}

// Simulates an HTTPS to HTTPS redirect.
TEST_F(CaptivePortalTabHelperTest, HttpToHttpRedirect) {
  GURL http_url(kHttpUrl);
  EXPECT_CALL(mock_reloader(),
              OnLoadStart(http_url.SchemeIsSecure())).Times(1);
  tab_helper().DidStartProvisionalLoadForFrame(
      main_render_frame1(), http_url, false, false);

  EXPECT_CALL(mock_reloader(), OnRedirect(http_url.SchemeIsSecure())).Times(1);
  OnRedirect(content::RESOURCE_TYPE_MAIN_FRAME, http_url,
             render_view_host1()->GetProcess()->GetID());

  EXPECT_CALL(mock_reloader(), OnLoadCommitted(net::OK)).Times(1);
  tab_helper().DidCommitProvisionalLoadForFrame(
      main_render_frame1(), http_url, ui::PAGE_TRANSITION_LINK);
}

// Tests that a subframe redirect doesn't reset the timer to kick off a captive
// portal probe for the main frame if the main frame request is taking too long.
TEST_F(CaptivePortalTabHelperTest, SubframeRedirect) {
  GURL http_url(kHttpUrl);
  EXPECT_CALL(mock_reloader(), OnLoadStart(false)).Times(1);
  tab_helper().DidStartProvisionalLoadForFrame(
      main_render_frame1(), http_url, false, false);

  GURL https_url(kHttpsUrl);
  OnRedirect(content::RESOURCE_TYPE_SUB_FRAME,
             https_url,
             render_view_host1()->GetProcess()->GetID());

  EXPECT_CALL(mock_reloader(), OnLoadCommitted(net::OK)).Times(1);
  tab_helper().DidCommitProvisionalLoadForFrame(
      main_render_frame1(), GURL(kErrorPageUrl), ui::PAGE_TRANSITION_LINK);
}

// Simulates a redirect, for another RenderViewHost.
TEST_F(CaptivePortalTabHelperTest, OtherRenderViewHostRedirect) {
  GURL http_url(kHttpUrl);
  EXPECT_CALL(mock_reloader(), OnLoadStart(false)).Times(1);
  tab_helper().DidStartProvisionalLoadForFrame(
      main_render_frame1(), http_url, false, false);

  // Another RenderViewHost sees a redirect.  None of the reloader's functions
  // should be called.
  GURL https_url(kHttpsUrl);
  OnRedirect(content::RESOURCE_TYPE_MAIN_FRAME,
             https_url,
             render_view_host2()->GetProcess()->GetID());

  tab_helper().DidFailProvisionalLoad(main_render_frame1(),
                                      https_url,
                                      net::ERR_TIMED_OUT,
                                      base::string16());

  // Provisional load starts for the error page.
  tab_helper().DidStartProvisionalLoadForFrame(
      main_render_frame1(), GURL(kErrorPageUrl), true, false);

  EXPECT_CALL(mock_reloader(), OnLoadCommitted(net::ERR_TIMED_OUT)).Times(1);
  tab_helper().DidCommitProvisionalLoadForFrame(main_render_frame1(),
                                                GURL(kErrorPageUrl),
                                                ui::PAGE_TRANSITION_LINK);
}

TEST_F(CaptivePortalTabHelperTest, LoginTabLogin) {
  EXPECT_FALSE(tab_helper().IsLoginTab());
  SetIsLoginTab();
  EXPECT_TRUE(tab_helper().IsLoginTab());

  ObservePortalResult(captive_portal::RESULT_INTERNET_CONNECTED,
                      captive_portal::RESULT_INTERNET_CONNECTED);
  EXPECT_FALSE(tab_helper().IsLoginTab());
}

TEST_F(CaptivePortalTabHelperTest, LoginTabError) {
  EXPECT_FALSE(tab_helper().IsLoginTab());

  SetIsLoginTab();
  EXPECT_TRUE(tab_helper().IsLoginTab());

  ObservePortalResult(captive_portal::RESULT_INTERNET_CONNECTED,
                      captive_portal::RESULT_NO_RESPONSE);
  EXPECT_FALSE(tab_helper().IsLoginTab());
}

TEST_F(CaptivePortalTabHelperTest, LoginTabMultipleResultsBeforeLogin) {
  EXPECT_FALSE(tab_helper().IsLoginTab());

  SetIsLoginTab();
  EXPECT_TRUE(tab_helper().IsLoginTab());

  ObservePortalResult(captive_portal::RESULT_INTERNET_CONNECTED,
                      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_TRUE(tab_helper().IsLoginTab());

  ObservePortalResult(captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
                      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_TRUE(tab_helper().IsLoginTab());

  ObservePortalResult(captive_portal::RESULT_NO_RESPONSE,
                      captive_portal::RESULT_INTERNET_CONNECTED);
  EXPECT_FALSE(tab_helper().IsLoginTab());
}

TEST_F(CaptivePortalTabHelperTest, NoLoginTab) {
  EXPECT_FALSE(tab_helper().IsLoginTab());

  ObservePortalResult(captive_portal::RESULT_INTERNET_CONNECTED,
                      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_FALSE(tab_helper().IsLoginTab());

  ObservePortalResult(captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
                      captive_portal::RESULT_NO_RESPONSE);
  EXPECT_FALSE(tab_helper().IsLoginTab());

  ObservePortalResult(captive_portal::RESULT_NO_RESPONSE,
                      captive_portal::RESULT_INTERNET_CONNECTED);
  EXPECT_FALSE(tab_helper().IsLoginTab());
}
