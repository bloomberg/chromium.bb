// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/native_app_launcher/native_app_navigation_util.h"

#include "base/memory/ptr_util.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/platform_test.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests the implementation of IsLinkNavigation(). The function being tested
// uses public NavigationManager interfaces and can be tested by using
// TestNavigationManager that implements the same interface.
class NativeAppNavigationUtilsTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    std::unique_ptr<web::TestNavigationManager> test_navigation_manager =
        base::MakeUnique<web::TestNavigationManager>();
    test_navigation_manager_ = test_navigation_manager.get();
    test_web_state_.SetNavigationManager(std::move(test_navigation_manager));
  }

  web::WebState* web_state() { return &test_web_state_; }

  // Adds a navigation item of |transition| type to current WebState.
  void AddItem(const std::string& url_spec, ui::PageTransition transition) {
    test_navigation_manager_->AddItem(GURL(url_spec), transition);
  }

 private:
  web::TestNavigationManager* test_navigation_manager_;
  web::TestWebState test_web_state_;
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
  AddItem("http://bar.com/page1", ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  AddItem("http://zap.com/page2", ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  EXPECT_TRUE(native_app_launcher::IsLinkNavigation(web_state()));
}

// The first non-redirect entry is tested. Earlier redirects do not matter.
TEST_F(NativeAppNavigationUtilsTest, TestTypedUrlWithRedirectEarlier) {
  AddItem("http://foo.com/page0", ui::PAGE_TRANSITION_LINK);
  AddItem("http://bar.com/page1", ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  AddItem("http://blah.com/page2", ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(native_app_launcher::IsLinkNavigation(web_state()));
}
