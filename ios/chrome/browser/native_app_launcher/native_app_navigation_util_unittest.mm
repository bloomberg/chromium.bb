// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/native_app_launcher/native_app_navigation_util.h"

#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/public/referrer.h"
#include "ios/web/public/test/web_test.h"
#import "ios/web/web_state/web_state_impl.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {

class NativeAppNavigationUtilsTest : public web::WebTest {
 protected:
  void SetUp() override {
    web::WebTest::SetUp();
    // WebStateImpl object is needed here to have access to CRWSessionController
    // for setting up NavigationManager entries.
    std::unique_ptr<web::WebStateImpl> web_state(
        new web::WebStateImpl(GetBrowserState()));
    web_state->GetNavigationManagerImpl().InitializeSession(NO);
    web_state->SetWebUsageEnabled(true);
    web_state_.reset(web_state.release());
  }

  void TearDown() override {
    web_state_.reset();
    web::WebTest::TearDown();
  }

  web::WebState* web_state() { return web_state_->GetWebState(); }

  void AddItem(const std::string& url_spec, ui::PageTransition transition) {
    CRWSessionController* session_controller =
        web_state_->GetNavigationManagerImpl().GetSessionController();
    web_state_->GetNavigationManagerImpl().AddPendingItem(
        GURL(url_spec), web::Referrer(), transition,
        web::NavigationInitiationType::USER_INITIATED);
    [session_controller commitPendingItem];
  }

 private:
  std::unique_ptr<web::WebStateImpl> web_state_;
};

// Tests that default state is not a link click.
TEST_F(NativeAppNavigationUtilsTest, TestEmpty) {
  EXPECT_FALSE(native_app_launcher::IsLinkNavigation(web_state()));
}

// URL typed by user is not a link click.
TEST_F(NativeAppNavigationUtilsTest, TestTypedUrl) {
  AddItem("http://foo.com/page0", ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(native_app_launcher::IsLinkNavigation(web_state()));
}

// Transition state shows that user navigated via a link click.
TEST_F(NativeAppNavigationUtilsTest, TestLinkClicked) {
  AddItem("http://foo.com/page0", ui::PAGE_TRANSITION_LINK);
  EXPECT_TRUE(native_app_launcher::IsLinkNavigation(web_state()));
}

// Transition state shows that user navigated through clicking on a bookmark
// is considered *not* a link click.
TEST_F(NativeAppNavigationUtilsTest, TestAutoBookmark) {
  AddItem("http://foo.com/page0", ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  EXPECT_TRUE(native_app_launcher::IsLinkNavigation(web_state()));
}

// When there are redirects along the way, redirects are skipped until the
// first non-redirect entry.
TEST_F(NativeAppNavigationUtilsTest, TestHasTypedUrlWithRedirect) {
  AddItem("http://foo.com/page0", ui::PAGE_TRANSITION_TYPED);
  AddItem("http://foo.com/page1", ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  EXPECT_FALSE(native_app_launcher::IsLinkNavigation(web_state()));
}

// When there are multiple redirects along the way, redirects are skipped until
// the first non-redirect entry.
TEST_F(NativeAppNavigationUtilsTest, TestHasLinkClickedWithRedirect) {
  AddItem("http://foo.com/page0", ui::PAGE_TRANSITION_LINK);
  AddItem("http://bar.com/page1", ui::PAGE_TRANSITION_SERVER_REDIRECT);
  AddItem("http://zap.com/page2", ui::PAGE_TRANSITION_SERVER_REDIRECT);
  EXPECT_TRUE(native_app_launcher::IsLinkNavigation(web_state()));
}

// The first non-redirect entry is tested. Earlier redirects do not matter.
TEST_F(NativeAppNavigationUtilsTest, TestTypedUrlWithRedirectEarlier) {
  AddItem("http://foo.com/page0", ui::PAGE_TRANSITION_LINK);
  AddItem("http://bar.com/page1", ui::PAGE_TRANSITION_SERVER_REDIRECT);
  AddItem("http://blah.com/page2", ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(native_app_launcher::IsLinkNavigation(web_state()));
}

}  // namespace
