// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/browsing_data_removal_controller.h"

#include <memory>

#include "base/logging.h"
#include "base/run_loop.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "components/open_from_clipboard/fake_clipboard_recent_content.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class BrowsingDataRemovalControllerTest : public PlatformTest {
 public:
  BrowsingDataRemovalControllerTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()) {
    DCHECK_EQ(ClipboardRecentContent::GetInstance(), nullptr);
    ClipboardRecentContent::SetInstance(
        std::make_unique<FakeClipboardRecentContent>());
  }

  ~BrowsingDataRemovalControllerTest() override {
    DCHECK_NE(ClipboardRecentContent::GetInstance(), nullptr);
    ClipboardRecentContent::SetInstance(nullptr);
  }

 protected:
  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<ios::ChromeBrowserState> browser_state_;
};

// Tests that
// -removeBrowsingDataFromBrowserState:mask:timePeriod:completionHandler: can
// finish performing its operation even when a BrowserState is destroyed.
TEST_F(BrowsingDataRemovalControllerTest, PerformAfterBrowserStateDestruction) {
  base::RunLoop run_loop;
  base::RepeatingClosure quit_run_loop = run_loop.QuitClosure();

  BrowsingDataRemovalController* removal_controller =
      [[BrowsingDataRemovalController alloc] init];

  __block BOOL block_was_called = NO;
  const BrowsingDataRemoveMask mask = BrowsingDataRemoveMask::REMOVE_ALL;
  [removal_controller
      removeBrowsingDataFromBrowserState:browser_state_.get()
                                    mask:mask
                              timePeriod:browsing_data::TimePeriod::ALL_TIME
                       completionHandler:^{
                         block_was_called = YES;
                         quit_run_loop.Run();
                       }];

  // Destroy the BrowserState immediately.
  [removal_controller browserStateDestroyed:browser_state_.get()];
  browser_state_.reset();

  run_loop.RunUntilIdle();
  EXPECT_TRUE(block_was_called);
}
