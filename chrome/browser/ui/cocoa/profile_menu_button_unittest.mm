// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/profile_menu_button.h"
#import "chrome/browser/ui/cocoa/test_event_utils.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"

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

// A menu delegate that will count the number open/close calls it recieves.
// The delegate will also automatically close the menu after it opens.
@interface ProfileMenuDelegate : NSObject {
  int menuOpenCount_;
  int menuCloseCount_;
}

@property(assign,nonatomic) int menuOpenCount;
@property(assign,nonatomic) int menuCloseCount;

@end

@implementation ProfileMenuDelegate

@synthesize menuOpenCount = menuOpenCount_;
@synthesize menuCloseCount = menuCloseCount_;

- (void)menuWillOpen:(NSMenu*)menu {
  ++menuOpenCount_;
  // Queue a message asking the menu to close.
  NSArray* modes = [NSArray arrayWithObjects:NSEventTrackingRunLoopMode,
                                             NSDefaultRunLoopMode,
                                             nil];
  [menu performSelector:@selector(cancelTrackingWithoutAnimation)
             withObject:nil
             afterDelay:0
                inModes:modes];
}

- (void)menuDidClose:(NSMenu*)menu {
  ++menuCloseCount_;
}

@end

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

TEST_F(ProfileMenuButtonTest, MenuTest) {
  scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@""]);
  [button_ setMenu:menu];

  // Hook into menu events.
  scoped_nsobject<ProfileMenuDelegate> delegate(
      [[ProfileMenuDelegate alloc] init]);
  [[button_ menu] setDelegate:delegate];
  EXPECT_EQ([delegate menuOpenCount], 0);
  EXPECT_EQ([delegate menuCloseCount], 0);

  // Trigger a mouse down to show the menu.
  NSPoint point = NSMakePoint(NSMaxX([button_ bounds]) - 1,
                              NSMaxY([button_ bounds]) - 1);
  point = [button_ convertPointToBase:point];
  NSEvent* downEvent =
      test_event_utils::LeftMouseDownAtPointInWindow(point, test_window());
  [button_ mouseDown:downEvent];

  // Verify that the menu was shown.
  EXPECT_EQ([delegate menuOpenCount], 1);
  EXPECT_EQ([delegate menuCloseCount], 1);
}
