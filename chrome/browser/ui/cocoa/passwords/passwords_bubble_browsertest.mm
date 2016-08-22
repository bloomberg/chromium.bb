// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_objc_class_swizzler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/cocoa/location_bar/manage_passwords_decoration.h"
#include "chrome/browser/ui/cocoa/passwords/passwords_bubble_cocoa.h"
#include "chrome/browser/ui/cocoa/passwords/passwords_bubble_controller.h"
#include "chrome/browser/ui/cocoa/passwords/save_pending_password_view_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_test.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest_mac.h"

// A helper class to swizzle [NSWindow isKeyWindow] to always return true.
@interface AlwaysKeyNSWindow : NSWindow
- (BOOL)isKeyWindow;
@end

@implementation AlwaysKeyNSWindow
- (BOOL)isKeyWindow {
  return YES;
}
@end

// Integration tests for the Mac password bubble.
class ManagePasswordsBubbleTest : public ManagePasswordsTest {
 public:
  void SetUpOnMainThread() override {
    ManagePasswordsTest::SetUpOnMainThread();
    browser()->window()->Show();
  }

  void TearDownOnMainThread() override {
    ManagePasswordsTest::TearDownOnMainThread();
  }

  ManagePasswordsBubbleController* controller() {
    return ManagePasswordsBubbleCocoa::instance()
               ? ManagePasswordsBubbleCocoa::instance()->controller_
               : nil;
  }

  void DoWithSwizzledNSWindow(void (^block)(void)) {
    // Swizzle [NSWindow isKeyWindow] so that BrowserWindow::IsActive will
    // return true and the bubble can be displayed.
    base::mac::ScopedObjCClassSwizzler swizzler(
        [NSWindow class], [AlwaysKeyNSWindow class], @selector(isKeyWindow));
    block();
  }

  void DisableAnimationsOnController() {
    InfoBubbleWindow* window =
        base::mac::ObjCCast<InfoBubbleWindow>([controller() window]);
    [window setAllowedAnimations:info_bubble::kAnimateNone];
  }

  ManagePasswordsDecoration* decoration() {
    NSWindow* window = browser()->window()->GetNativeWindow();
    BrowserWindowController* bwc =
        [BrowserWindowController browserWindowControllerForWindow:window];
    return [bwc locationBarBridge]->manage_passwords_decoration();
  }

  ManagePasswordsIcon* view() override { return decoration()->icon(); }
};

IN_PROC_BROWSER_TEST_F(ManagePasswordsBubbleTest,
                       PasswordEntryShowsPendingSaveView) {
  EXPECT_FALSE(ManagePasswordsBubbleCocoa::instance());
  DoWithSwizzledNSWindow(^{ SetupPendingPassword(); });
  EXPECT_TRUE(ManagePasswordsBubbleCocoa::instance());
  EXPECT_EQ([SavePendingPasswordViewController class],
            [controller().currentController class]);
  EXPECT_TRUE(view()->active());
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsBubbleTest, IconClickTogglesBubble) {
  // Show the bubble automatically.
  DoWithSwizzledNSWindow(^{ SetupPendingPassword(); });

  // Close the bubble by clicking on the decoration.
  DisableAnimationsOnController();
  decoration()->OnMousePressed(NSZeroRect, NSZeroPoint);
  EXPECT_FALSE(ManagePasswordsBubbleCocoa::instance());

  // Show the bubble by clicking on the decoration.
  DoWithSwizzledNSWindow(^{
      decoration()->OnMousePressed(NSZeroRect, NSZeroPoint);
  });
  EXPECT_TRUE(ManagePasswordsBubbleCocoa::instance());
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsBubbleTest, TabChangeTogglesIcon) {
  // Show the bubble (and icon) automatically.
  DoWithSwizzledNSWindow(^{ SetupPendingPassword(); });
  EXPECT_TRUE(decoration()->IsVisible());

  // Open a new tab.
  int firstTab = browser()->tab_strip_model()->active_index();
  AddTabAtIndex(
      firstTab + 1, GURL("http://foo.bar/"), ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(decoration()->IsVisible());

  // Switch back to the previous tab.
  browser()->tab_strip_model()->ActivateTabAt(firstTab, true);
  EXPECT_TRUE(decoration()->IsVisible());
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsBubbleTest, DoubleOpenBubble) {
  // Open the bubble first.
  DoWithSwizzledNSWindow(^{ SetupPendingPassword(); });
  base::scoped_nsobject<ManagePasswordsBubbleController> bubble_controller(
      [controller() retain]);
  EXPECT_TRUE(bubble_controller);
  EXPECT_TRUE(view()->active());

  // Open the bubble again, the first one should be replaced.
  DoWithSwizzledNSWindow(^{ SetupPendingPassword(); });
  EXPECT_NSNE(bubble_controller, controller());
  EXPECT_TRUE(controller());
  EXPECT_TRUE(view()->active());
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsBubbleTest, DoubleOpenDifferentBubbles) {
  // Open the autosignin bubble first.
  DoWithSwizzledNSWindow(^{
    std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials;
    local_credentials.emplace_back(new autofill::PasswordForm(*test_form()));
    SetupAutoSignin(std::move(local_credentials));
  });
  EXPECT_TRUE(controller());
  EXPECT_TRUE(view()->active());

  // Open the save bubble. The previous one is closed twice (with and without
  // animation). It shouldn't cause DCHECK.
  DoWithSwizzledNSWindow(^{ SetupPendingPassword(); });
  EXPECT_TRUE(controller());
  EXPECT_TRUE(view()->active());
}
