// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/ui/cocoa/browser_window_touch_bar.h"
#include "chrome/browser/ui/cocoa/test/cocoa_profile_test.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class BrowserWindowTouchBarUnitTest : public CocoaProfileTest {
 public:
  void SetUp() override {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(browser());
    browserWindowTouchBar_.reset([[BrowserWindowTouchBar alloc]
                initWithBrowser:browser()
        browserWindowController:nil]);
  }

  void TearDown() override { CocoaProfileTest::TearDown(); }

  base::scoped_nsobject<BrowserWindowTouchBar> browserWindowTouchBar_;
};

TEST_F(BrowserWindowTouchBarUnitTest, TouchBarItems) {
  if (!base::mac::IsAtLeastOS10_12())
    return;

  PrefService* prefs = profile()->GetPrefs();
  DCHECK(prefs);
  prefs->SetBoolean(prefs::kShowHomeButton, true);

  NSArray* touchBarItemIds =
      [[browserWindowTouchBar_ makeTouchBar] itemIdentifiers];
  EXPECT_TRUE([touchBarItemIds containsObject:@"BackForwardTouchId"]);
  EXPECT_TRUE([touchBarItemIds containsObject:@"ReloadOrStopTouchId"]);
  EXPECT_TRUE([touchBarItemIds containsObject:@"HomeTouchId"]);
  EXPECT_TRUE([touchBarItemIds containsObject:@"SearchTouchId"]);
  EXPECT_TRUE([touchBarItemIds containsObject:@"NewTabTouchId"]);
  EXPECT_TRUE([touchBarItemIds containsObject:@"StarTouchId"]);

  prefs->SetBoolean(prefs::kShowHomeButton, false);
  touchBarItemIds = [[browserWindowTouchBar_ makeTouchBar] itemIdentifiers];
  EXPECT_TRUE([touchBarItemIds containsObject:@"BackForwardTouchId"]);
  EXPECT_TRUE([touchBarItemIds containsObject:@"ReloadOrStopTouchId"]);
  EXPECT_FALSE([touchBarItemIds containsObject:@"HomeTouchId"]);
  EXPECT_TRUE([touchBarItemIds containsObject:@"SearchTouchId"]);
  EXPECT_TRUE([touchBarItemIds containsObject:@"NewTabTouchId"]);
  EXPECT_TRUE([touchBarItemIds containsObject:@"StarTouchId"]);
}

TEST_F(BrowserWindowTouchBarUnitTest, ReloadOrStopTouchBarItem) {
  if (!base::mac::IsAtLeastOS10_12())
    return;

  NSTouchBar* touchBar = [browserWindowTouchBar_ makeTouchBar];
  [browserWindowTouchBar_ setIsPageLoading:NO];
  NSTouchBarItem* item =
      [browserWindowTouchBar_ touchBar:touchBar
                 makeItemForIdentifier:@"ReloadOrStopTouchId"];

  EXPECT_EQ(IDC_RELOAD, [[item view] tag]);

  [browserWindowTouchBar_ setIsPageLoading:YES];
  item = [browserWindowTouchBar_ touchBar:touchBar
                    makeItemForIdentifier:@"ReloadOrStopTouchId"];

  EXPECT_EQ(IDC_STOP, [[item view] tag]);
}
