// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_bar.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_bar_item.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NewTabPageBar (Testing)
- (void)buttonDidTap:(UIButton*)button;
@end

namespace {

class NewTabPageBarTest : public PlatformTest {
 protected:
  void SetUp() override {
    CGRect frame = CGRectMake(0, 0, 320, 44);
    bar_ = [[NewTabPageBar alloc] initWithFrame:frame];
  };
  NewTabPageBar* bar_;
};

TEST_F(NewTabPageBarTest, SetItems) {
  NewTabPageBarItem* firstItem = [NewTabPageBarItem
      newTabPageBarItemWithTitle:@"First"
                      identifier:1
                           image:[UIImage imageNamed:@"ntp_bookmarks"]];
  // Tests that identifier test function can return both true and false.
  EXPECT_TRUE(firstItem.identifier == 1U);

  NewTabPageBarItem* secondItem = [NewTabPageBarItem
      newTabPageBarItemWithTitle:@"Second"
                      identifier:2
                           image:[UIImage imageNamed:@"ntp_bookmarks"]];
  NewTabPageBarItem* thirdItem = [NewTabPageBarItem
      newTabPageBarItemWithTitle:@"Third"
                      identifier:3
                           image:[UIImage imageNamed:@"ntp_bookmarks"]];

  [bar_ setItems:[NSArray arrayWithObject:firstItem]];
  EXPECT_EQ(bar_.buttons.count, 1U);
  [bar_ setItems:[NSArray arrayWithObjects:firstItem, secondItem, nil]];
  EXPECT_EQ(bar_.buttons.count, 2U);
  [bar_ setItems:[NSArray
                     arrayWithObjects:firstItem, secondItem, thirdItem, nil]];
  EXPECT_EQ(bar_.buttons.count, 3U);
  [bar_ setItems:[NSArray arrayWithObject:firstItem]];
  EXPECT_EQ(bar_.buttons.count, 1U);
}

TEST_F(NewTabPageBarTest, SetSelectedIndex_iPadOnly) {
  // Selected index isn't meaningful on iPhone.
  if (!IsIPadIdiom()) {
    return;
  }

  NewTabPageBarItem* firstItem = [NewTabPageBarItem
      newTabPageBarItemWithTitle:@"First"
                      identifier:1
                           image:[UIImage imageNamed:@"ntp_bookmarks"]];
  NewTabPageBarItem* secondItem = [NewTabPageBarItem
      newTabPageBarItemWithTitle:@"Second"
                      identifier:2
                           image:[UIImage imageNamed:@"ntp_bookmarks"]];

  NewTabPageBarItem* thirdItem = [NewTabPageBarItem
      newTabPageBarItemWithTitle:@"Third"
                      identifier:3
                           image:[UIImage imageNamed:@"ntp_bookmarks"]];

  [bar_ setItems:[NSArray
                     arrayWithObjects:firstItem, secondItem, thirdItem, nil]];

  UIButton* button = [[bar_ buttons] objectAtIndex:0];
  [button sendActionsForControlEvents:UIControlEventTouchDown];
  EXPECT_TRUE([[[bar_ buttons] objectAtIndex:0] isSelected]);

  id secondItemDelegate =
      [OCMockObject mockForProtocol:@protocol(NewTabPageBarDelegate)];
  [[secondItemDelegate expect] newTabBarItemDidChange:secondItem
                                          changePanel:YES];
  [bar_ setDelegate:secondItemDelegate];
  button = [[bar_ buttons] objectAtIndex:1];
  [button sendActionsForControlEvents:UIControlEventTouchDown];
  EXPECT_TRUE([[[bar_ buttons] objectAtIndex:1] isSelected]);
  EXPECT_OCMOCK_VERIFY(secondItemDelegate);

  id thirdItemDelegate =
      [OCMockObject mockForProtocol:@protocol(NewTabPageBarDelegate)];
  [[thirdItemDelegate expect] newTabBarItemDidChange:thirdItem changePanel:YES];
  [bar_ setDelegate:thirdItemDelegate];
  button = [[bar_ buttons] objectAtIndex:2];
  [button sendActionsForControlEvents:UIControlEventTouchDown];
  EXPECT_TRUE([[[bar_ buttons] objectAtIndex:2] isSelected]);
  EXPECT_OCMOCK_VERIFY(thirdItemDelegate);

  // Reselecting the same item should not cause the method to be called again.
  id uncalledDelegate =
      [OCMockObject niceMockForProtocol:@protocol(NewTabPageBarDelegate)];
  [[uncalledDelegate reject] newTabBarItemDidChange:OCMOCK_ANY changePanel:YES];
  [bar_ setDelegate:uncalledDelegate];
  [bar_ setSelectedIndex:2];
  EXPECT_TRUE([[[bar_ buttons] objectAtIndex:2] isSelected]);
  EXPECT_OCMOCK_VERIFY(uncalledDelegate);
}

}  // anonymous namespace
