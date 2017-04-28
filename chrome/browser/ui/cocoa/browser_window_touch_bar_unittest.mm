// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_touch_bar.h"
#include "chrome/browser/ui/cocoa/test/cocoa_profile_test.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class BrowserWindowTouchBarUnitTest : public CocoaProfileTest {
 public:
  void SetUp() override {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(browser());

    feature_list.InitAndEnableFeature(features::kBrowserTouchBar);

    BOOL yes = YES;
    bwc_ = [OCMockObject mockForClass:[BrowserWindowController class]];
    [[[bwc_ stub] andReturnValue:OCMOCK_VALUE(yes)]
        isKindOfClass:[BrowserWindowController class]];
    [[bwc_ stub] invalidateTouchBar];

    touch_bar_.reset([[BrowserWindowTouchBar alloc] initWithBrowser:browser()
                                            browserWindowController:bwc_]);
  }

  id bwc() const { return bwc_; }

  void TearDown() override { CocoaProfileTest::TearDown(); }

  // A mock BrowserWindowController object.
  id bwc_;

  // Used to enable the the browser window touch bar.
  base::test::ScopedFeatureList feature_list;

  base::scoped_nsobject<BrowserWindowTouchBar> touch_bar_;
};

// Tests to check if the touch bar contains the correct items.
TEST_F(BrowserWindowTouchBarUnitTest, TouchBarItems) {
  if (!base::mac::IsAtLeastOS10_12())
    return;

  BOOL yes = YES;
  [[[bwc() expect] andReturnValue:OCMOCK_VALUE(yes)]
      isFullscreenForTabContentOrExtension];

  PrefService* prefs = profile()->GetPrefs();
  DCHECK(prefs);
  prefs->SetBoolean(prefs::kShowHomeButton, true);

  // The touch bar should be empty since the toolbar is hidden when the browser
  // is in tab fullscreen.
  NSTouchBar* touch_bar = [touch_bar_ makeTouchBar];
  NSArray* touch_bar_items = [touch_bar itemIdentifiers];
  EXPECT_TRUE([touch_bar_items
      containsObject:[BrowserWindowTouchBar
                         touchBarIdForItemId:@"FULLSCREEN-ORIGIN-LABEL"]]);
  EXPECT_TRUE([[touch_bar escapeKeyReplacementItemIdentifier]
      isEqualToString:[BrowserWindowTouchBar
                          touchBarIdForItemId:@"EXIT-FULLSCREEN"]]);

  BOOL no = NO;
  [[[bwc() stub] andReturnValue:OCMOCK_VALUE(no)]
      isFullscreenForTabContentOrExtension];

  touch_bar_items = [[touch_bar_ makeTouchBar] itemIdentifiers];
  EXPECT_TRUE([touch_bar_items
      containsObject:[BrowserWindowTouchBar touchBarIdForItemId:@"BACK-FWD"]]);
  EXPECT_TRUE(
      [touch_bar_items containsObject:[BrowserWindowTouchBar
                                          touchBarIdForItemId:@"RELOAD-STOP"]]);
  EXPECT_TRUE([touch_bar_items
      containsObject:[BrowserWindowTouchBar touchBarIdForItemId:@"HOME"]]);
  EXPECT_TRUE([touch_bar_items
      containsObject:[BrowserWindowTouchBar touchBarIdForItemId:@"SEARCH"]]);
  EXPECT_TRUE([touch_bar_items
      containsObject:[BrowserWindowTouchBar touchBarIdForItemId:@"BOOKMARK"]]);
  EXPECT_TRUE([touch_bar_items
      containsObject:[BrowserWindowTouchBar touchBarIdForItemId:@"NEW-TAB"]]);

  prefs->SetBoolean(prefs::kShowHomeButton, false);
  touch_bar_items = [[touch_bar_ makeTouchBar] itemIdentifiers];
  EXPECT_TRUE([touch_bar_items
      containsObject:[BrowserWindowTouchBar touchBarIdForItemId:@"BACK-FWD"]]);
  EXPECT_TRUE(
      [touch_bar_items containsObject:[BrowserWindowTouchBar
                                          touchBarIdForItemId:@"RELOAD-STOP"]]);
  EXPECT_TRUE([touch_bar_items
      containsObject:[BrowserWindowTouchBar touchBarIdForItemId:@"SEARCH"]]);
  EXPECT_TRUE([touch_bar_items
      containsObject:[BrowserWindowTouchBar touchBarIdForItemId:@"BOOKMARK"]]);
  EXPECT_TRUE([touch_bar_items
      containsObject:[BrowserWindowTouchBar touchBarIdForItemId:@"NEW-TAB"]]);
}

// Tests the reload or stop touch bar item.
TEST_F(BrowserWindowTouchBarUnitTest, ReloadOrStopTouchBarItem) {
  if (!base::mac::IsAtLeastOS10_12())
    return;

  BOOL no = NO;
  [[[bwc() stub] andReturnValue:OCMOCK_VALUE(no)]
      isFullscreenForTabContentOrExtension];

  NSTouchBar* touch_bar = [touch_bar_ makeTouchBar];
  [touch_bar_ setIsPageLoading:NO];

  NSTouchBarItem* item =
      [touch_bar_ touchBar:touch_bar
          makeItemForIdentifier:[BrowserWindowTouchBar
                                    touchBarIdForItemId:@"RELOAD-STOP"]];
  EXPECT_EQ(IDC_RELOAD, [[item view] tag]);

  [touch_bar_ setIsPageLoading:YES];
  item = [touch_bar_ touchBar:touch_bar
        makeItemForIdentifier:[BrowserWindowTouchBar
                                  touchBarIdForItemId:@"RELOAD-STOP"]];
  EXPECT_EQ(IDC_STOP, [[item view] tag]);
}
