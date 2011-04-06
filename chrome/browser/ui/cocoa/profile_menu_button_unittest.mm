// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/profile_menu_button.h"
#import "chrome/browser/ui/cocoa/test_event_utils.h"
#import "testing/gtest_mac.h"

class ProfileMenuButtonTest : public CocoaTest {
 public:
  ProfileMenuButtonTest() {
    scoped_nsobject<ProfileMenuButton> button([[ProfileMenuButton alloc]
        initWithFrame:NSMakeRect(50, 50, 100, 100)
            pullsDown:NO]);
    button_ = button.get();
    [[test_window() contentView] addSubview:button_];
  }

  ProfileMenuButton* button_;
};

// A stub to check that popUpContextMenu:withEvent:forView: is called.
@interface ProfileShowMenuHandler : NSObject {
  int showMenuCount_;
}

@property(assign, nonatomic) int showMenuCount;

- (void)popUpContextMenu:(NSMenu*)menu
               withEvent:(NSEvent*)event
                 forView:(NSView*)view;

@end

@implementation ProfileShowMenuHandler

@synthesize showMenuCount = showMenuCount_;

- (void)popUpContextMenu:(NSMenu*)menu
               withEvent:(NSEvent*)event
                 forView:(NSView*)view {
  showMenuCount_++;
}

@end


TEST_F(ProfileMenuButtonTest, ControlSize) {
  scoped_nsobject<ProfileMenuButton> button([[ProfileMenuButton alloc]
      initWithFrame:NSZeroRect
          pullsDown:NO]);

  NSSize minSize = [button minControlSize];
  EXPECT_TRUE(NSEqualSizes(minSize, [button desiredControlSize]));

  [button setProfileDisplayName:@"Test"];
  EXPECT_TRUE(NSEqualSizes(minSize, [button desiredControlSize]));
  EXPECT_TRUE(NSEqualSizes(minSize, [button desiredControlSize]));

  [button setShouldShowProfileDisplayName:YES];
  EXPECT_TRUE(NSEqualSizes(minSize, [button minControlSize]));
  EXPECT_GT([button desiredControlSize].height, minSize.height);
  EXPECT_GT([button desiredControlSize].width, minSize.width);

  [button setShouldShowProfileDisplayName:NO];
  EXPECT_TRUE(NSEqualSizes(minSize, [button desiredControlSize]));
  EXPECT_TRUE(NSEqualSizes(minSize, [button desiredControlSize]));
}

// Tests display, add/remove.
TEST_VIEW(ProfileMenuButtonTest, button_);

TEST_F(ProfileMenuButtonTest, HitTest) {
  NSRect mouseRect = NSInsetRect([button_ frame], 1, 1);
  NSPoint topRight = NSMakePoint(NSMaxX(mouseRect), NSMaxY(mouseRect));
  NSPoint bottomRight = NSMakePoint(NSMaxX(mouseRect), NSMinY(mouseRect));
  NSPoint outsidePoint = NSOffsetRect(mouseRect, -10, -10).origin;

  // Without profile display name. Only topRight should hit.
  EXPECT_NSEQ([button_ hitTest:topRight], button_);
  EXPECT_NSEQ([button_ hitTest:bottomRight], NULL);
  EXPECT_NSEQ([button_ hitTest:outsidePoint], NULL);

  // With profile display name. The profile display name should not hit.
  [button_ setProfileDisplayName:@"Test"];
  [button_ setShouldShowProfileDisplayName:YES];
  EXPECT_NSEQ([button_ hitTest:topRight], button_);
  EXPECT_NSEQ([button_ hitTest:bottomRight], NULL);
  EXPECT_NSEQ([button_ hitTest:outsidePoint], NULL);
}

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(ProfileMenuButtonTest, Display) {
  // With profile display name.
  [button_ display];

  // With profile display name.
  [button_ setProfileDisplayName:@"Test"];
  [button_ setShouldShowProfileDisplayName:YES];
  [button_ display];
}

// Checks that a menu is displayed on mouse down. Also makes sure that
// nothing leaks or crashes when displaying the button in its pressed state.
TEST_F(ProfileMenuButtonTest, MenuTest) {
  scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@""]);
  [button_ setMenu:menu];

  // Trigger a mouse down to show the menu.
  NSPoint point = NSMakePoint(NSMaxX([button_ bounds]) - 1,
                              NSMaxY([button_ bounds]) - 1);
  point = [button_ convertPointToBase:point];
  NSEvent* downEvent =
      test_event_utils::LeftMouseDownAtPointInWindow(point, test_window());
  scoped_nsobject<ProfileShowMenuHandler> showMenuHandler(
      [[ProfileShowMenuHandler alloc] init]);
  [button_   mouseDown:downEvent
    withShowMenuTarget:showMenuHandler];

  EXPECT_EQ(1, [showMenuHandler showMenuCount]);
}
