// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/extensions/browser_action_button.h"
#import "chrome/browser/ui/cocoa/extensions/browser_actions_controller.h"
#import "chrome/browser/ui/cocoa/extensions/toolbar_actions_bar_bubble_mac.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/extensions/extension_message_bubble_browsertest.h"
#include "ui/base/cocoa/cocoa_base_utils.h"

namespace {

// Returns the ToolbarController for the given browser.
ToolbarController* ToolbarControllerForBrowser(Browser* browser) {
  return [[BrowserWindowController browserWindowControllerForWindow:
             browser->window()->GetNativeWindow()] toolbarController];
}

// Checks that the |bubble| is using the |expectedReferenceView|, and is in
// approximately the correct position.
void CheckBubbleAndReferenceView(ToolbarActionsBarBubbleMac* bubble,
                                 NSView* expectedReferenceView) {
  ASSERT_TRUE(bubble);
  ASSERT_TRUE(expectedReferenceView);

  // Do a rough check that the bubble is in the right place.
  // A window's frame (like the bubble's) is already in screen coordinates.
  NSRect bubbleFrame = [[bubble window] frame];

  // Unfortunately, it's more tedious to get the reference view's screen
  // coordinates (since -[NSWindow convertRectToScreen is in OSX 10.7).
  NSRect referenceFrame = [expectedReferenceView bounds];
  referenceFrame =
      [expectedReferenceView convertRect:referenceFrame toView:nil];
  NSWindow* window = [expectedReferenceView window];
  CGFloat refLowY = [expectedReferenceView isFlipped] ?
      NSMaxY(referenceFrame) : NSMinY(referenceFrame);
  NSPoint refLowerLeft = NSMakePoint(NSMinX(referenceFrame), refLowY);
  NSPoint refLowerLeftInScreen =
      ui::ConvertPointFromWindowToScreen(window, refLowerLeft);
  NSPoint refLowerRight = NSMakePoint(NSMaxX(referenceFrame), refLowY);
  NSPoint refLowerRightInScreen =
      ui::ConvertPointFromWindowToScreen(window, refLowerRight);

  // The bubble should be below the reference view, but not too far below.
  EXPECT_LE(NSMaxY(bubbleFrame), refLowerLeftInScreen.y);
  // "Too far below" is kind of ambiguous. The exact logic of where a bubble
  // is positioned with respect to its anchor view should be tested as part of
  // the bubble logic, but we still want to make sure we didn't accidentally
  // place it somewhere crazy (which can happen if we draw it, and then
  // animate or reposition the reference view).
  const int kFudgeFactor = 50;
  EXPECT_LE(NSMaxY(bubbleFrame), refLowerLeftInScreen.y + kFudgeFactor);

  // The bubble should intersect the reference view somewhere along the x-axis.
  EXPECT_LE(refLowerLeftInScreen.x, NSMaxX(bubbleFrame));
  EXPECT_LE(NSMinX(bubbleFrame), refLowerRightInScreen.x);

  // And, of course, the bubble should be visible.
  EXPECT_TRUE([[bubble window] isVisible]);
}

}  // namespace

class ExtensionMessageBubbleBrowserTestMac
    : public ExtensionMessageBubbleBrowserTest {
 public:
  ExtensionMessageBubbleBrowserTestMac() {}
  ~ExtensionMessageBubbleBrowserTestMac() override {}

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void CheckBubble(Browser* browser, AnchorPosition anchor) override;
  void CloseBubble(Browser* browser) override;
  void CheckBubbleIsNotPresent(Browser* browser) override;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageBubbleBrowserTestMac);
};

void ExtensionMessageBubbleBrowserTestMac::SetUpCommandLine(
    base::CommandLine* command_line) {
  ExtensionMessageBubbleBrowserTest::SetUpCommandLine(command_line);
  [ToolbarActionsBarBubbleMac setAnimationEnabledForTesting:NO];
}

void ExtensionMessageBubbleBrowserTestMac::CheckBubble(
    Browser* browser,
    AnchorPosition anchor) {
  ToolbarController* toolbarController = ToolbarControllerForBrowser(browser);
  BrowserActionsController* actionsController =
      [toolbarController browserActionsController];
  NSView* anchorView = nil;
  ToolbarActionsBarBubbleMac* bubble = [actionsController activeBubble];
  switch (anchor) {
    case ANCHOR_BROWSER_ACTION:
      anchorView = [actionsController buttonWithIndex:0];
      break;
    case ANCHOR_APP_MENU:
      anchorView = [toolbarController appMenuButton];
      break;
  }
  CheckBubbleAndReferenceView(bubble, anchorView);
}

void ExtensionMessageBubbleBrowserTestMac::CloseBubble(Browser* browser) {
  BrowserActionsController* controller =
      [ToolbarControllerForBrowser(browser) browserActionsController];
  ToolbarActionsBarBubbleMac* bubble = [controller activeBubble];
  ASSERT_TRUE(bubble);
  [bubble close];
  EXPECT_EQ(nil, [controller activeBubble]);
}

void ExtensionMessageBubbleBrowserTestMac::CheckBubbleIsNotPresent(
    Browser* browser) {
  EXPECT_EQ(
      nil,
      [[ToolbarControllerForBrowser(browser) browserActionsController]
          activeBubble]);
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleBrowserTestMac,
                       ExtensionBubbleAnchoredToExtensionAction) {
  TestBubbleAnchoredToExtensionAction();
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleBrowserTestMac,
                       ExtensionBubbleAnchoredToAppMenu) {
  TestBubbleAnchoredToAppMenu();
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleBrowserTestMac,
                       ExtensionBubbleAnchoredToAppMenuWithOtherAction) {
  TestBubbleAnchoredToAppMenuWithOtherAction();
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleBrowserTestMac,
                       PRE_ExtensionBubbleShowsOnStartup) {
  PreBubbleShowsOnStartup();
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleBrowserTestMac,
                       ExtensionBubbleShowsOnStartup) {
  TestBubbleShowsOnStartup();
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleBrowserTestMac,
                       TestUninstallDangerousExtension) {
  TestUninstallDangerousExtension();
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleBrowserTestMac,
                       TestDevModeBubbleIsntShownTwice) {
  TestDevModeBubbleIsntShownTwice();
}
