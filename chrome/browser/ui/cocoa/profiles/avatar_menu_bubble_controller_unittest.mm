// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/profiles/avatar_menu_bubble_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_pump_mac.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gtest_mac.h"
#import "ui/base/cocoa/controls/hyperlink_button_cell.h"
#include "ui/events/test/cocoa_test_event_utils.h"

class AvatarMenuBubbleControllerTest : public CocoaTest {
 public:
  AvatarMenuBubbleControllerTest()
      : manager_(TestingBrowserProcess::GetGlobal()) {
  }

  virtual void SetUp() {
    CocoaTest::SetUp();
    ASSERT_TRUE(manager_.SetUp());

    manager_.CreateTestingProfile("test1", scoped_ptr<PrefServiceSyncable>(),
                                  base::ASCIIToUTF16("Test 1"), 1,
                                  std::string(),
                                  TestingProfile::TestingFactories());
    manager_.CreateTestingProfile("test2", scoped_ptr<PrefServiceSyncable>(),
                                  base::ASCIIToUTF16("Test 2"), 0,
                                  std::string(),
                                  TestingProfile::TestingFactories());

    menu_ = new AvatarMenu(manager_.profile_info_cache(), NULL, NULL);
    menu_->RebuildMenu();

    NSRect frame = [test_window() frame];
    NSPoint point = NSMakePoint(NSMidX(frame), NSMidY(frame));
    controller_ =
        [[AvatarMenuBubbleController alloc] initWithMenu:menu()
                                             parentWindow:test_window()
                                               anchoredAt:point];
  }

  TestingProfileManager* manager() { return &manager_; }
  AvatarMenuBubbleController* controller() { return controller_; }
  AvatarMenu* menu() { return menu_; }

  AvatarMenuItemController* GetHighlightedItem() {
    for (AvatarMenuItemController* item in [controller() items]) {
      if ([item isHighlighted])
        return item;
    }
    return nil;
  }

 private:
  TestingProfileManager manager_;

  // Weak; releases self.
  AvatarMenuBubbleController* controller_;

  // Weak; owned by |controller_|.
  AvatarMenu* menu_;
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

  base::scoped_nsobject<NSMutableArray> oldItems([[controller() items] copy]);

  // Now create a new profile and notify the delegate.
  manager()->CreateTestingProfile("test3", scoped_ptr<PrefServiceSyncable>(),
                                  base::ASCIIToUTF16("Test 3"), 0,
                                  std::string(),
                                  TestingProfile::TestingFactories());

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

// This subclass is used to inject a delegate into the hide/show edit link
// animation.
@interface TestingAvatarMenuItemController : AvatarMenuItemController
                                                 <NSAnimationDelegate> {
 @private
  scoped_ptr<base::MessagePumpNSRunLoop> pump_;
}
// After calling |-highlightForEventType:| an animation will possibly be
// started. Since the animation is non-blocking, the run loop will need to be
// spun (via the MessagePump) until the animation has finished.
- (void)runMessagePump;
@end

@implementation TestingAvatarMenuItemController
- (void)runMessagePump {
  if (!pump_)
    pump_.reset(new base::MessagePumpNSRunLoop);
  pump_->Run(NULL);
}

- (void)willStartAnimation:(NSAnimation*)anim {
  [anim setDelegate:self];
}

- (void)animationDidEnd:(NSAnimation*)anim {
  [super animationDidEnd:anim];
  pump_->Quit();
}

- (void)animationDidStop:(NSAnimation*)anim {
  [super animationDidStop:anim];
  FAIL() << "Animation stopped before it completed its run";
  pump_->Quit();
}

- (void)sendHighlightMessageForMouseExited {
  [self highlightForEventType:NSMouseExited];
  // Quit the pump because the animation was cancelled before it even ran.
  pump_->Quit();
}
@end

TEST_F(AvatarMenuBubbleControllerTest, HighlightForEventType) {
  base::scoped_nsobject<TestingAvatarMenuItemController> item(
      [[TestingAvatarMenuItemController alloc] initWithMenuIndex:0
                                                   menuController:nil]);
  // Test non-active states first.
  [[item activeView] setHidden:YES];

  NSView* editButton = [item editButton];
  NSView* emailField = [item emailField];

  // The edit link remains hidden.
  [item setIsHighlighted:YES];
  EXPECT_TRUE(editButton.isHidden);
  EXPECT_FALSE(emailField.isHidden);

  [item setIsHighlighted:NO];
  EXPECT_TRUE(editButton.isHidden);
  EXPECT_FALSE(emailField.isHidden);

  // Make the item "active" and re-test.
  [[item activeView] setHidden:NO];

  [item setIsHighlighted:YES];
  [item runMessagePump];

  EXPECT_FALSE(editButton.isHidden);
  EXPECT_TRUE(emailField.isHidden);

  [item setIsHighlighted:NO];
  [item runMessagePump];

  EXPECT_TRUE(editButton.isHidden);
  EXPECT_FALSE(emailField.isHidden);

  // Now mouse over and out quickly, as if scrubbing through the menu, to test
  // the hover dwell delay.
  [item highlightForEventType:NSMouseEntered];
  [item performSelector:@selector(sendHighlightMessageForMouseExited)
             withObject:nil
             afterDelay:0];
  [item runMessagePump];

  EXPECT_TRUE(editButton.isHidden);
  EXPECT_FALSE(emailField.isHidden);
}

TEST_F(AvatarMenuBubbleControllerTest, DownArrow) {
  EXPECT_NSEQ(nil, GetHighlightedItem());

  NSEvent* event =
      cocoa_test_event_utils::KeyEventWithCharacter(NSDownArrowFunctionKey);
  // Going down with no item selected should start the selection at the first
  // item.
  [controller() keyDown:event];
  EXPECT_EQ([[controller() items] objectAtIndex:1], GetHighlightedItem());

  [controller() keyDown:event];
  EXPECT_EQ([[controller() items] objectAtIndex:0], GetHighlightedItem());

  // There are no more items now so going down should stay at the last item.
  [controller() keyDown:event];
  EXPECT_EQ([[controller() items] objectAtIndex:0], GetHighlightedItem());
}

TEST_F(AvatarMenuBubbleControllerTest, UpArrow) {
  EXPECT_NSEQ(nil, GetHighlightedItem());

  NSEvent* event =
      cocoa_test_event_utils::KeyEventWithCharacter(NSUpArrowFunctionKey);
  // Going up with no item selected should start the selection at the last
  // item.
  [controller() keyDown:event];
  EXPECT_EQ([[controller() items] objectAtIndex:0], GetHighlightedItem());

  [controller() keyDown:event];
  EXPECT_EQ([[controller() items] objectAtIndex:1], GetHighlightedItem());

  // There are no more items now so going up should stay at the first item.
  [controller() keyDown:event];
  EXPECT_EQ([[controller() items] objectAtIndex:1], GetHighlightedItem());
}
