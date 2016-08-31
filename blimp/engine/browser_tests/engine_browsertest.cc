// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "blimp/client/core/contents/ime_feature.h"
#include "blimp/client/core/contents/mock_ime_feature_delegate.h"
#include "blimp/client/core/contents/mock_navigation_feature_delegate.h"
#include "blimp/client/core/contents/navigation_feature.h"
#include "blimp/client/core/contents/tab_control_feature.h"
#include "blimp/client/core/render_widget/mock_render_widget_feature_delegate.h"
#include "blimp/client/core/render_widget/render_widget_feature.h"
#include "blimp/client/core/session/assignment_source.h"
#include "blimp/client/public/session/assignment.h"
#include "blimp/client/session/test_client_session.h"
#include "blimp/engine/browser_tests/blimp_browser_test.h"
#include "content/public/test/browser_test.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

using ::testing::InvokeWithoutArgs;

namespace blimp {
namespace {

const int kDummyTabId = 0;
const char kPage1Path[] = "/page1.html";
const char kPage2Path[] = "/page2.html";
const char kPage1Title[] = "page1";
const char kPage2Title[] = "page2";

// Uses a headless client session to test a full engine.
class EngineBrowserTest : public BlimpBrowserTest {
 public:
  EngineBrowserTest() {}

 protected:
  void SetUpOnMainThread() override {
    BlimpBrowserTest::SetUpOnMainThread();

    // Create a headless client on UI thread.
    client_session_.reset(new client::TestClientSession);

    // Set feature delegates.
    client_session_->GetNavigationFeature()->SetDelegate(
        kDummyTabId, &client_nav_feature_delegate_);
    client_session_->GetRenderWidgetFeature()->SetDelegate(
        kDummyTabId, &client_rw_feature_delegate_);
    client_session_->GetImeFeature()->set_delegate(
        &client_ime_feature_delegate_);

    // Skip assigner. Engine info is already available.
    client_session_->ConnectWithAssignment(client::ASSIGNMENT_REQUEST_RESULT_OK,
                                           GetAssignment());
    client_session_->GetTabControlFeature()->SetSizeAndScale(
        gfx::Size(100, 100), 1);
    client_session_->GetTabControlFeature()->CreateTab(kDummyTabId);

    // Record the last page title known to the client. Page loads will sometimes
    // involve multiple title changes in transition, including the requested
    // URL. When a page is done loading, the last title should be the one from
    // the <title> tag.
    ON_CALL(client_nav_feature_delegate_,
            OnTitleChanged(kDummyTabId, testing::_))
        .WillByDefault(testing::SaveArg<1>(&last_page_title_));

    EXPECT_TRUE(embedded_test_server()->Start());
  }

  // Given a path on the embedded local server, tell the client to navigate to
  // that page by URL.
  void NavigateToLocalUrl(const std::string& path) {
    GURL url = embedded_test_server()->GetURL(path);
    client_session_->GetNavigationFeature()->NavigateToUrlText(kDummyTabId,
                                                               url.spec());
  }

  // Set mock expectations that a page will load. A page has loaded when the
  // page load status changes to true, and then to false.
  void ExpectPageLoad() {
    testing::InSequence s;

    // There may be redundant events indicating the page load status is true,
    // so allow more than one.
    EXPECT_CALL(client_nav_feature_delegate_,
                OnLoadingChanged(kDummyTabId, true))
        .Times(testing::AtLeast(1));
    EXPECT_CALL(client_nav_feature_delegate_,
                OnLoadingChanged(kDummyTabId, false))
        .WillOnce(
            InvokeWithoutArgs(this, &EngineBrowserTest::SignalCompletion));
  }

  void RunAndVerify() {
    RunUntilCompletion();
    testing::Mock::VerifyAndClearExpectations(&client_rw_feature_delegate_);
    testing::Mock::VerifyAndClearExpectations(&client_nav_feature_delegate_);
  }

  client::MockNavigationFeatureDelegate client_nav_feature_delegate_;
  client::MockRenderWidgetFeatureDelegate client_rw_feature_delegate_;
  client::MockImeFeatureDelegate client_ime_feature_delegate_;
  std::unique_ptr<client::TestClientSession> client_session_;
  std::string last_page_title_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EngineBrowserTest);
};

IN_PROC_BROWSER_TEST_F(EngineBrowserTest, LoadUrl) {
  EXPECT_CALL(client_rw_feature_delegate_, OnRenderWidgetCreated(1));
  ExpectPageLoad();
  NavigateToLocalUrl(kPage1Path);
  RunAndVerify();
  EXPECT_EQ(kPage1Title, last_page_title_);
}

IN_PROC_BROWSER_TEST_F(EngineBrowserTest, Reload) {
  ExpectPageLoad();
  NavigateToLocalUrl(kPage1Path);
  RunAndVerify();
  EXPECT_EQ(kPage1Title, last_page_title_);

  ExpectPageLoad();
  client_session_->GetNavigationFeature()->Reload(kDummyTabId);
  RunAndVerify();
  EXPECT_EQ(kPage1Title, last_page_title_);
}

IN_PROC_BROWSER_TEST_F(EngineBrowserTest, GoBackAndGoForward) {
  ExpectPageLoad();
  NavigateToLocalUrl(kPage1Path);
  RunAndVerify();
  EXPECT_EQ(kPage1Title, last_page_title_);

  ExpectPageLoad();
  NavigateToLocalUrl(kPage2Path);
  RunAndVerify();
  EXPECT_EQ(kPage2Title, last_page_title_);

  ExpectPageLoad();
  client_session_->GetNavigationFeature()->GoBack(kDummyTabId);
  RunAndVerify();
  EXPECT_EQ(kPage1Title, last_page_title_);

  ExpectPageLoad();
  client_session_->GetNavigationFeature()->GoForward(kDummyTabId);
  RunAndVerify();
  EXPECT_EQ(kPage2Title, last_page_title_);
}

IN_PROC_BROWSER_TEST_F(EngineBrowserTest, InvalidGoBack) {
  // Try an invalid GoBack before loading a page, and assert that the page still
  // loads correctly.
  ExpectPageLoad();
  client_session_->GetNavigationFeature()->GoBack(kDummyTabId);
  NavigateToLocalUrl(kPage1Path);
  RunAndVerify();
  EXPECT_EQ(kPage1Title, last_page_title_);
}

IN_PROC_BROWSER_TEST_F(EngineBrowserTest, InvalidGoForward) {
  ExpectPageLoad();
  NavigateToLocalUrl(kPage1Path);
  RunAndVerify();
  EXPECT_EQ(kPage1Title, last_page_title_);

  // Try an invalid GoForward before loading a different page, and
  // assert that the page still loads correctly.
  ExpectPageLoad();
  client_session_->GetNavigationFeature()->GoForward(kDummyTabId);
  NavigateToLocalUrl(kPage2Path);
  RunAndVerify();
  EXPECT_EQ(kPage2Title, last_page_title_);
}

}  // namespace
}  // namespace blimp
