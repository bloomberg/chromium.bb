// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/extensions/extension_toolbar_icon_surfacing_bubble_mac.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/run_loop_testing.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_bubble_delegate.h"
#import "ui/events/test/cocoa_test_event_utils.h"

using ExtensionToolbarIconSurfacingBubbleTest = CocoaTest;

namespace {

class TestDelegate : public ToolbarActionsBarBubbleDelegate {
 public:
  TestDelegate() {}
  ~TestDelegate() {}

  void OnToolbarActionsBarBubbleShown() override {}
  void OnToolbarActionsBarBubbleClosed(CloseAction action) override {
    EXPECT_FALSE(action_);
    action_.reset(new CloseAction(action));
  }

  CloseAction* action() const { return action_.get(); }

 private:
  scoped_ptr<CloseAction> action_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

}  // namespace

// A simple class to observe when a window is destructing.
@interface WindowObserver : NSObject {
  BOOL windowIsClosing_;
}

- (id)initWithWindow:(NSWindow*)window;

- (void)dealloc;

- (void)onWindowClosing:(NSNotification*)notification;

@property(nonatomic, assign) BOOL windowIsClosing;

@end

@implementation WindowObserver

- (id)initWithWindow:(NSWindow*)window {
  if ((self = [super init])) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(onWindowClosing:)
               name:NSWindowWillCloseNotification
             object:window];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)onWindowClosing:(NSNotification*)notification {
  windowIsClosing_ = YES;
}

@synthesize windowIsClosing = windowIsClosing_;

@end

TEST_F(ExtensionToolbarIconSurfacingBubbleTest,
       ExtensionToolbarIconSurfacingBubbleTest) {
  {
    // Test clicking on the button.
    TestDelegate delegate;
    ExtensionToolbarIconSurfacingBubbleMac* bubble =
        [[ExtensionToolbarIconSurfacingBubbleMac alloc]
              initWithParentWindow:test_window()
                       anchorPoint:NSZeroPoint
                          delegate:&delegate];
    [bubble showWindow:nil];
    [base::mac::ObjCCastStrict<InfoBubbleWindow>([bubble window])
        setAllowedAnimations:info_bubble::kAnimateNone];
    base::scoped_nsobject<WindowObserver> windowObserver(
        [[WindowObserver alloc] initWithWindow:[bubble window]]);
    chrome::testing::NSRunLoopRunAllPending();
    EXPECT_FALSE(delegate.action());
    EXPECT_FALSE([windowObserver windowIsClosing]);

    // Find the button and click on it.
    NSView* button = nil;
    NSArray* subviews = [[[bubble window] contentView] subviews];
    for (NSView* view in subviews) {
      if ([view isKindOfClass:[NSButton class]]) {
        button = view;
        break;
      }
    }
    ASSERT_TRUE(button);
    std::pair<NSEvent*, NSEvent*> events =
        cocoa_test_event_utils::MouseClickInView(button, 1);
    [NSApp postEvent:events.second atStart:YES];
    [NSApp sendEvent:events.first];
    chrome::testing::NSRunLoopRunAllPending();

    // The bubble should be closed, and the delegate should be told that the
    // button was clicked.
    ASSERT_TRUE(delegate.action());
    EXPECT_EQ(ToolbarActionsBarBubbleDelegate::ACKNOWLEDGED,
              *delegate.action());
    EXPECT_TRUE([windowObserver windowIsClosing]);
  }

  {
    // Test dismissing the bubble without clicking the button.
    TestDelegate delegate;
    ExtensionToolbarIconSurfacingBubbleMac* bubble =
        [[ExtensionToolbarIconSurfacingBubbleMac alloc]
              initWithParentWindow:test_window()
                       anchorPoint:NSZeroPoint
                          delegate:&delegate];
    [bubble showWindow:nil];
    [base::mac::ObjCCastStrict<InfoBubbleWindow>([bubble window])
        setAllowedAnimations:info_bubble::kAnimateNone];
    base::scoped_nsobject<WindowObserver> windowObserver(
        [[WindowObserver alloc] initWithWindow:[bubble window]]);
    chrome::testing::NSRunLoopRunAllPending();
    EXPECT_FALSE(delegate.action());
    EXPECT_FALSE([windowObserver windowIsClosing]);

    // Close the bubble. The delegate should be told it was dismissed.
    [bubble close];
    chrome::testing::NSRunLoopRunAllPending();
    ASSERT_TRUE(delegate.action());
    EXPECT_EQ(ToolbarActionsBarBubbleDelegate::DISMISSED,
              *delegate.action());
    EXPECT_TRUE([windowObserver windowIsClosing]);
  }
}
