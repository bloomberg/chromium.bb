// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "testing/gtest/include/gtest/gtest.h"

// A BrowserWindowCocoa that goes PONG when
// BOOKMARK_BAR_VISIBILITY_PREF_CHANGED is sent.  This is so we can be
// sure we are observing it.
class BrowserWindowCocoaPong : public BrowserWindowCocoa {
 public:
  BrowserWindowCocoaPong(Browser* browser,
                         BrowserWindowController* controller)
      : BrowserWindowCocoa(browser, controller) {
    pong_ = false;
  }
  virtual ~BrowserWindowCocoaPong() { }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) {
    if (type == chrome::NOTIFICATION_PREF_CHANGED) {
      const std::string& pref_name =
          *content::Details<std::string>(details).ptr();
      if (pref_name == prefs::kShowBookmarkBar)
        pong_ = true;
    }
    BrowserWindowCocoa::Observe(type, source, details);
  }

  bool pong_;
};

// Main test class.
class BrowserWindowCocoaTest : public CocoaProfileTest {
  virtual void SetUp() {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(browser());

    controller_ = [[BrowserWindowController alloc] initWithBrowser:browser()
                                                     takeOwnership:NO];
  }

  virtual void TearDown() {
    [controller_ close];
    CocoaProfileTest::TearDown();
  }

 public:
  BrowserWindowController* controller_;
};


TEST_F(BrowserWindowCocoaTest, TestNotification) {
  BrowserWindowCocoaPong *bwc =
      new BrowserWindowCocoaPong(browser(), controller_);

  EXPECT_FALSE(bwc->pong_);
  bookmark_utils::ToggleWhenVisible(profile());
  // Confirm we are listening
  EXPECT_TRUE(bwc->pong_);
  delete bwc;
  // If this does NOT crash it confirms we stopped listening in the destructor.
  bookmark_utils::ToggleWhenVisible(profile());
}


TEST_F(BrowserWindowCocoaTest, TestBookmarkBarVisible) {
  BrowserWindowCocoaPong *bwc = new BrowserWindowCocoaPong(
    browser(),
    controller_);
  scoped_ptr<BrowserWindowCocoaPong> scoped_bwc(bwc);

  bool before = bwc->IsBookmarkBarVisible();
  bookmark_utils::ToggleWhenVisible(profile());
  EXPECT_NE(before, bwc->IsBookmarkBarVisible());

  bookmark_utils::ToggleWhenVisible(profile());
  EXPECT_EQ(before, bwc->IsBookmarkBarVisible());
}

@interface FakeController : NSWindowController {
  BOOL fullscreen_;
}
@end

@implementation FakeController
- (void)enterFullscreenForURL:(const GURL&)url
                   bubbleType:(FullscreenExitBubbleType)bubbleType {
  fullscreen_ = YES;
}
- (void)exitFullscreen {
  fullscreen_ = NO;
}
- (BOOL)isFullscreen {
  return fullscreen_;
}
@end

TEST_F(BrowserWindowCocoaTest, TestFullscreen) {
  // Wrap the FakeController in a scoped_nsobject instead of autoreleasing in
  // windowWillClose: because we never actually open a window in this test (so
  // windowWillClose: never gets called).
  scoped_nsobject<FakeController> fake_controller(
      [[FakeController alloc] init]);
  BrowserWindowCocoaPong* bwc = new BrowserWindowCocoaPong(
    browser(),
    (BrowserWindowController*)fake_controller.get());
  scoped_ptr<BrowserWindowCocoaPong> scoped_bwc(bwc);

  EXPECT_FALSE(bwc->IsFullscreen());
  bwc->EnterFullscreen(GURL(), FEB_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION);
  EXPECT_TRUE(bwc->IsFullscreen());
  bwc->ExitFullscreen();
  EXPECT_FALSE(bwc->IsFullscreen());
  [fake_controller close];
}

// TODO(???): test other methods of BrowserWindowCocoa
