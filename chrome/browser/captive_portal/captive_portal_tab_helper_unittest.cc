// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/captive_portal/captive_portal_tab_helper.h"

#include "base/callback.h"
#include "chrome/browser/captive_portal/captive_portal_service.h"
#include "chrome/browser/captive_portal/captive_portal_tab_reloader.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace captive_portal {

namespace {

const char* const kHttpUrl = "http://whatever.com/";
const char* const kHttpsUrl = "https://whatever.com/";
// Error pages use a "data:" URL.  Shouldn't actually matter what this is.
const char* const kErrorPageUrl = "data:blah";

}  // namespace

class MockCaptivePortalTabReloader : public CaptivePortalTabReloader {
 public:
  MockCaptivePortalTabReloader()
      : CaptivePortalTabReloader(NULL, NULL, base::Callback<void()>()) {
  }

  MOCK_METHOD1(OnLoadStart, void(bool));
  MOCK_METHOD1(OnLoadCommitted, void(int));
  MOCK_METHOD0(OnAbort, void());
  MOCK_METHOD2(OnCaptivePortalResults, void(Result, Result));
};

class CaptivePortalTabHelperTest : public testing::Test {
 public:
  CaptivePortalTabHelperTest()
      : tab_helper_(NULL, NULL),
        mock_reloader_(new testing::StrictMock<MockCaptivePortalTabReloader>) {
    tab_helper_.SetTabReloaderForTest(mock_reloader_);
  }
  virtual ~CaptivePortalTabHelperTest() {}

  // Simulates a successful load of |url|.
  void SimulateSuccess(const GURL& url) {
    EXPECT_CALL(mock_reloader(), OnLoadStart(url.SchemeIsSecure())).Times(1);
    tab_helper().DidStartProvisionalLoadForFrame(1, true, url, false, NULL);

    EXPECT_CALL(mock_reloader(), OnLoadCommitted(net::OK)).Times(1);
    tab_helper().DidCommitProvisionalLoadForFrame(
        1, true, url, content::PAGE_TRANSITION_LINK, NULL);
  }

  // Simulates a connection timeout while requesting |url|.
  void SimulateTimeout(const GURL& url) {
    EXPECT_CALL(mock_reloader(), OnLoadStart(url.SchemeIsSecure())).Times(1);
    tab_helper().DidStartProvisionalLoadForFrame(1, true, url, false, NULL);

    tab_helper().DidFailProvisionalLoad(
        1, true, url, net::ERR_TIMED_OUT, string16(), NULL);

    // Provisional load starts for the error page.
    tab_helper().DidStartProvisionalLoadForFrame(
        1, true, GURL(kErrorPageUrl), true, NULL);

    EXPECT_CALL(mock_reloader(), OnLoadCommitted(net::ERR_TIMED_OUT)).Times(1);
    tab_helper().DidCommitProvisionalLoadForFrame(
        1, true, GURL(kErrorPageUrl), content::PAGE_TRANSITION_LINK, NULL);
  }

  // Simulates an abort while requesting |url|.
  void SimulateAbort(const GURL& url) {
    EXPECT_CALL(mock_reloader(), OnLoadStart(url.SchemeIsSecure())).Times(1);
    tab_helper().DidStartProvisionalLoadForFrame(1, true, url, false, NULL);

    EXPECT_CALL(mock_reloader(), OnAbort()).Times(1);
    tab_helper().DidFailProvisionalLoad(
        1, true, url, net::ERR_ABORTED, string16(), NULL);
  }

  // Simulates an abort while loading an error page.
  void SimulateAbortTimeout(const GURL& url) {
    EXPECT_CALL(mock_reloader(), OnLoadStart(url.SchemeIsSecure())).Times(1);
    tab_helper().DidStartProvisionalLoadForFrame(1, true, url, false, NULL);

    tab_helper().DidFailProvisionalLoad(
        1, true, url, net::ERR_TIMED_OUT, string16(), NULL);

    // Start event for the error page.
    tab_helper().DidStartProvisionalLoadForFrame(1, true, url, true, NULL);

    EXPECT_CALL(mock_reloader(), OnAbort()).Times(1);
    tab_helper().DidFailProvisionalLoad(
        1, true, url, net::ERR_ABORTED, string16(), NULL);
  }

  CaptivePortalTabHelper& tab_helper() {
    return tab_helper_;
  }

  void ObservePortalResult(Result previous_result, Result result) {
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

  MockCaptivePortalTabReloader& mock_reloader() { return *mock_reloader_; }

  void SetIsLoginTab() {
    tab_helper().SetIsLoginTab();
  }

 private:
  CaptivePortalTabHelper tab_helper_;

  // Owned by |tab_helper_|.
  testing::StrictMock<MockCaptivePortalTabReloader>* mock_reloader_;

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalTabHelperTest);
};

TEST_F(CaptivePortalTabHelperTest, HttpSuccess) {
  SimulateSuccess(GURL(kHttpUrl));
  tab_helper().DidStopLoading();
}

TEST_F(CaptivePortalTabHelperTest, HttpTimeout) {
  SimulateTimeout(GURL(kHttpUrl));
  tab_helper().DidStopLoading();
}

// Same as above, but simulates what happens when the Link Doctor is enabled,
// which adds another provisional load/commit for the error page, after the
// first two.
TEST_F(CaptivePortalTabHelperTest, HttpTimeoutLinkDoctor) {
  SimulateTimeout(GURL(kHttpUrl));

  EXPECT_CALL(mock_reloader(), OnLoadStart(false)).Times(1);
  // Provisional load starts for the error page.
  tab_helper().DidStartProvisionalLoadForFrame(
      1, true, GURL(kErrorPageUrl), true, NULL);

  EXPECT_CALL(mock_reloader(), OnLoadCommitted(net::OK)).Times(1);
  tab_helper().DidCommitProvisionalLoadForFrame(
      1, true, GURL(kErrorPageUrl), content::PAGE_TRANSITION_LINK, NULL);
  tab_helper().DidStopLoading();
}

TEST_F(CaptivePortalTabHelperTest, HttpsSuccess) {
  SimulateSuccess(GURL(kHttpsUrl));
  tab_helper().DidStopLoading();
  EXPECT_FALSE(tab_helper().IsLoginTab());
}

TEST_F(CaptivePortalTabHelperTest, HttpsTimeout) {
  SimulateTimeout(GURL(kHttpsUrl));
  // Make sure no state was carried over from the timeout.
  SimulateSuccess(GURL(kHttpsUrl));
  EXPECT_FALSE(tab_helper().IsLoginTab());
}

TEST_F(CaptivePortalTabHelperTest, HttpsAbort) {
  SimulateAbort(GURL(kHttpsUrl));
  // Make sure no state was carried over from the abort.
  SimulateSuccess(GURL(kHttpsUrl));
  EXPECT_FALSE(tab_helper().IsLoginTab());
}

// Abort while there's a provisional timeout error page loading.
TEST_F(CaptivePortalTabHelperTest, HttpsAbortTimeout) {
  SimulateAbortTimeout(GURL(kHttpsUrl));
  // Make sure no state was carried over from the timeout or the abort.
  SimulateSuccess(GURL(kHttpsUrl));
  EXPECT_FALSE(tab_helper().IsLoginTab());
}

// Simulates navigations for a number of subframes, and makes sure no
// CaptivePortalTabHelper function is called.
TEST_F(CaptivePortalTabHelperTest, HttpsSubframe) {
  GURL url = GURL(kHttpsUrl);
  // Normal load.
  tab_helper().DidStartProvisionalLoadForFrame(1, false, url, false, NULL);
  tab_helper().DidCommitProvisionalLoadForFrame(
      1, false, url, content::PAGE_TRANSITION_LINK, NULL);

  // Timeout.
  tab_helper().DidStartProvisionalLoadForFrame(2, false, url, false, NULL);
  tab_helper().DidFailProvisionalLoad(
      2, false, url, net::ERR_TIMED_OUT, string16(), NULL);
  tab_helper().DidStartProvisionalLoadForFrame(2, false, url, true, NULL);
  tab_helper().DidFailProvisionalLoad(
      2, false, url, net::ERR_ABORTED, string16(), NULL);

  // Abort.
  tab_helper().DidStartProvisionalLoadForFrame(3, false, url, false, NULL);
  tab_helper().DidFailProvisionalLoad(
      3, false, url, net::ERR_ABORTED, string16(), NULL);
}

// Simulates a subframe erroring out at the same time as a provisional load,
// but with a different error code.  Make sure the TabHelper sees the correct
// error.
TEST_F(CaptivePortalTabHelperTest, HttpsSubframeParallelError) {
  // URL used by both frames.
  GURL url = GURL(kHttpsUrl);

  int frame_id = 2;
  int subframe_id = 1;

  // Loads start.
  EXPECT_CALL(mock_reloader(), OnLoadStart(url.SchemeIsSecure())).Times(1);
  tab_helper().DidStartProvisionalLoadForFrame(
      frame_id, true, url, false, NULL);
  tab_helper().DidStartProvisionalLoadForFrame(
      subframe_id, false, url, false, NULL);

  // Loads return errors.
  tab_helper().DidFailProvisionalLoad(
      frame_id, true, url, net::ERR_UNEXPECTED, string16(), NULL);
  tab_helper().DidFailProvisionalLoad(
      subframe_id, false, url, net::ERR_TIMED_OUT, string16(), NULL);

  // Provisional load starts for the error pages.
  tab_helper().DidStartProvisionalLoadForFrame(
      frame_id, true, url, true, NULL);
  tab_helper().DidStartProvisionalLoadForFrame(
      subframe_id, false, url, true, NULL);

  // Error page load finishes.
  tab_helper().DidCommitProvisionalLoadForFrame(
      subframe_id, false, url, content::PAGE_TRANSITION_AUTO_SUBFRAME, NULL);
  EXPECT_CALL(mock_reloader(), OnLoadCommitted(net::ERR_UNEXPECTED)).Times(1);
  tab_helper().DidCommitProvisionalLoadForFrame(
      frame_id, true, url, content::PAGE_TRANSITION_LINK, NULL);
}

TEST_F(CaptivePortalTabHelperTest, LoginTabLogin) {
  EXPECT_FALSE(tab_helper().IsLoginTab());
  SetIsLoginTab();
  EXPECT_TRUE(tab_helper().IsLoginTab());

  ObservePortalResult(RESULT_INTERNET_CONNECTED, RESULT_INTERNET_CONNECTED);
  EXPECT_FALSE(tab_helper().IsLoginTab());
}

TEST_F(CaptivePortalTabHelperTest, LoginTabError) {
  EXPECT_FALSE(tab_helper().IsLoginTab());

  SetIsLoginTab();
  EXPECT_TRUE(tab_helper().IsLoginTab());

  ObservePortalResult(RESULT_INTERNET_CONNECTED, RESULT_NO_RESPONSE);
  EXPECT_FALSE(tab_helper().IsLoginTab());
}

TEST_F(CaptivePortalTabHelperTest, LoginTabMultipleResultsBeforeLogin) {
  EXPECT_FALSE(tab_helper().IsLoginTab());

  SetIsLoginTab();
  EXPECT_TRUE(tab_helper().IsLoginTab());

  ObservePortalResult(RESULT_INTERNET_CONNECTED, RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_TRUE(tab_helper().IsLoginTab());

  ObservePortalResult(RESULT_BEHIND_CAPTIVE_PORTAL,
                      RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_TRUE(tab_helper().IsLoginTab());

  ObservePortalResult(RESULT_NO_RESPONSE, RESULT_INTERNET_CONNECTED);
  EXPECT_FALSE(tab_helper().IsLoginTab());
}

TEST_F(CaptivePortalTabHelperTest, NoLoginTab) {
  EXPECT_FALSE(tab_helper().IsLoginTab());

  ObservePortalResult(RESULT_INTERNET_CONNECTED, RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_FALSE(tab_helper().IsLoginTab());

  ObservePortalResult(RESULT_BEHIND_CAPTIVE_PORTAL, RESULT_NO_RESPONSE);
  EXPECT_FALSE(tab_helper().IsLoginTab());

  ObservePortalResult(RESULT_NO_RESPONSE, RESULT_INTERNET_CONNECTED);
  EXPECT_FALSE(tab_helper().IsLoginTab());
}

}  // namespace captive_portal
