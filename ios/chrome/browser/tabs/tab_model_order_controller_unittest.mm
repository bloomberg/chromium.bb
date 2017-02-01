// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/memory/ptr_util.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#import "ios/chrome/browser/sessions/session_window.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_order_controller.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#include "ios/web/public/referrer.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "ios/web/public/web_thread.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

class TabModelOrderControllerTest : public PlatformTest {
 protected:
  TabModelOrderControllerTest()
      : thread_bundle_(web::TestWebThreadBundle::IO_MAINLOOP),
        scoped_browser_state_manager_(
            base::MakeUnique<TestChromeBrowserStateManager>(base::FilePath())) {
  }

  void SetUp() override {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    PlatformTest::SetUp();

    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();

    dummy_tab_.reset([[Tab alloc]
        initWithWindowName:nil
                    opener:nullptr
               openedByDOM:NO
                     model:nil
              browserState:chrome_browser_state_.get()]);

    sessionWindow_.reset([[SessionWindowIOS new] retain]);

    base::scoped_nsobject<TestSessionService> test_service(
        [[TestSessionService alloc] init]);
    tabModel_.reset([[TabModel alloc]
        initWithSessionWindow:sessionWindow_
               sessionService:test_service
                 browserState:chrome_browser_state_.get()]);

    orderController_.reset(
        [[TabModelOrderController alloc] initWithTabModel:tabModel_]);

    // Values to use when creating new tabs.
    url_ = GURL("https://www.some.url.com");
    referrer_ = web::Referrer(GURL("https://www.some.referer.com"),
                              web::ReferrerPolicyDefault);
  }

  void TearDown() override {
    [dummy_tab_ close];

    [tabModel_ browserStateDestroyed];

    PlatformTest::TearDown();
  }

  Tab* addTabToParentInBackground(Tab* parentTab) {
    return [tabModel_
        insertOrUpdateTabWithURL:url_
                        referrer:referrer_
                      transition:ui::PAGE_TRANSITION_LINK
                      windowName:nil
                          opener:parentTab
                     openedByDOM:NO
                         atIndex:TabModelConstants::kTabPositionAutomatically
                    inBackground:YES];
  }

  GURL url_;
  web::Referrer referrer_;
  web::TestWebThreadBundle thread_bundle_;
  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;
  base::scoped_nsobject<SessionWindowIOS> sessionWindow_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  base::scoped_nsobject<Tab> dummy_tab_;
  base::scoped_nsobject<TabModelOrderController> orderController_;
  base::scoped_nsobject<TabModel> tabModel_;
  base::mac::ScopedNSAutoreleasePool pool_;
};

// Verifies that tabs added in the background (e.g. from context menu -> Open in
// new tab) are inserted in the proper order.
TEST_F(TabModelOrderControllerTest, DetermineInsertionIndexForBackgroundTabs) {
  // Add |parentTab|, then add |childTab| in the background.
  Tab* parentTab = [tabModel_ insertTabWithURL:url_
                                      referrer:referrer_
                                    windowName:nil
                                        opener:nil
                                       atIndex:0];
  Tab* childTab = addTabToParentInBackground(parentTab);

  // Verify a second child would be inserted in an index after the first child.
  int index = [orderController_
      insertionIndexForTab:dummy_tab_
                transition:ui::PAGE_TRANSITION_LINK
                    opener:parentTab
                 adjacency:TabModelOrderConstants::kAdjacentAfter];
  EXPECT_EQ(2, index);

  // Now add a child of the child, then verify that a second child of the parent
  // would be inserted before the child's child.
  addTabToParentInBackground(childTab);
  index = [orderController_
      insertionIndexForTab:dummy_tab_
                transition:ui::PAGE_TRANSITION_LINK
                    opener:parentTab
                 adjacency:TabModelOrderConstants::kAdjacentAfter];
  EXPECT_EQ(2, index);
}
}  // anonymous namespace
