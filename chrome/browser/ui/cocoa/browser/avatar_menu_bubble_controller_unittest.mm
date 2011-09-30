// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser/avatar_menu_bubble_controller.h"

#include "base/memory/scoped_nsobject.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/avatar_menu_model.h"
#include "chrome/browser/profiles/avatar_menu_model_observer.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/hyperlink_button_cell.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gtest_mac.h"

class FakeBridge : public AvatarMenuModelObserver {
 public:
  void OnAvatarMenuModelChanged(AvatarMenuModel* model) OVERRIDE {}
};

class AvatarMenuBubbleControllerTest : public CocoaTest {
 public:
  AvatarMenuBubbleControllerTest()
      : manager_(static_cast<TestingBrowserProcess*>(g_browser_process)) {
  }

  virtual void SetUp() {
    CocoaTest::SetUp();
    ASSERT_TRUE(manager_.SetUp());

    manager_.CreateTestingProfile("test1", ASCIIToUTF16("Test 1"), 1);
    manager_.CreateTestingProfile("test2", ASCIIToUTF16("Test 2"), 0);

    bridge_ = new FakeBridge;
    model_ = new AvatarMenuModel(manager_.profile_info_cache(), bridge(), NULL);

    NSRect frame = [test_window() frame];
    NSPoint point = NSMakePoint(NSMidX(frame), NSMidY(frame));
    controller_ =
        [[AvatarMenuBubbleController alloc] initWithModel:model()
                                                   bridge:bridge()
                                             parentWindow:test_window()
                                               anchoredAt:point];
  }

  TestingProfileManager* manager() { return &manager_; }
  AvatarMenuBubbleController* controller() { return controller_; }
  AvatarMenuModel* model() { return model_; }
  FakeBridge* bridge() { return bridge_; }

 private:
  TestingProfileManager manager_;

  // Weak; releases self.
  AvatarMenuBubbleController* controller_;

  // Weak; owned by |controller_|.
  AvatarMenuModel* model_;
  FakeBridge* bridge_;
};

TEST_F(AvatarMenuBubbleControllerTest, InitialLayout) {
  [controller() showWindow:nil];

  // Two profiles means two item views and the new button with separator.
  NSView* contents = [[controller() window] contentView];
  EXPECT_EQ(4U, [[contents subviews] count]);

  // Loop over the itmes and match the viewController views to subviews.
  NSMutableArray* subviews =
      [NSMutableArray arrayWithArray:[contents subviews]];
  for (AvatarMenuItemController* viewController in [controller() items]) {
    for (NSView* subview in subviews) {
      if ([viewController view] == subview) {
        [subviews removeObject:subview];
        break;
      }
    }
  }

  // The one remaining subview should be the new user button.
  EXPECT_EQ(2U, [subviews count]);

  BOOL hasButton = NO;
  BOOL hasSeparator = NO;
  for (NSView* subview in subviews) {
    if ([subview isKindOfClass:[NSButton class]]) {
      EXPECT_FALSE(hasButton);
      hasButton = YES;

      NSButton* button = static_cast<NSButton*>(subview);
      EXPECT_EQ(@selector(newProfile:), [button action]);
      EXPECT_EQ(controller(), [button target]);
      EXPECT_TRUE([[button cell] isKindOfClass:[HyperlinkButtonCell class]]);
    } else if ([subview isKindOfClass:[NSBox class]]) {
      EXPECT_FALSE(hasSeparator);
      hasSeparator = YES;
    } else {
      EXPECT_FALSE(subview) << "Unexpected subview: "
                            << [[subview description] UTF8String];
    }
  }

  [controller() close];
}

TEST_F(AvatarMenuBubbleControllerTest, PerformLayout) {
  [controller() showWindow:nil];

  NSView* contents = [[controller() window] contentView];
  EXPECT_EQ(4U, [[contents subviews] count]);

  scoped_nsobject<NSMutableArray> oldItems([[controller() items] copy]);

  // Now create a new profile and notify the delegate.
  manager()->CreateTestingProfile("test3", ASCIIToUTF16("Test 3"), 0);

  // Testing the bridge is not worth the effort...
  [controller() performLayout];

  EXPECT_EQ(5U, [[contents subviews] count]);

  // Make sure that none of the old items exit.
  NSArray* newItems = [controller() items];
  for (AvatarMenuItemController* oldVC in oldItems.get()) {
    EXPECT_FALSE([newItems containsObject:oldVC]);
    EXPECT_FALSE([[contents subviews] containsObject:[oldVC view]]);
  }

  [controller() close];
}

TEST_F(AvatarMenuBubbleControllerTest, HighlightForEventType) {
  scoped_nsobject<AvatarMenuItemController> item(
      [[AvatarMenuItemController alloc] initWithModelIndex:0
                                            menuController:nil]);
  // Test non-active states first.
  [[item activeView] setHidden:YES];

  NSView* editButton = [item editButton];
  NSView* emailField = [item emailField];

  // The edit link remains hidden.
  [item highlightForEventType:NSMouseEntered];
  EXPECT_TRUE(editButton.isHidden);
  EXPECT_FALSE(emailField.isHidden);

  [item highlightForEventType:NSMouseExited];
  EXPECT_TRUE(editButton.isHidden);
  EXPECT_FALSE(emailField.isHidden);

  // Make the item "active" and re-test.
  [[item activeView] setHidden:NO];

  [item highlightForEventType:NSMouseEntered];
  EXPECT_FALSE(editButton.isHidden);
  EXPECT_TRUE(emailField.isHidden);

  [item highlightForEventType:NSMouseExited];
  EXPECT_TRUE(editButton.isHidden);
  EXPECT_FALSE(emailField.isHidden);
}
