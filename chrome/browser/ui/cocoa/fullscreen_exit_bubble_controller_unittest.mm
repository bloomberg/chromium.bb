// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/fullscreen_exit_bubble_controller.h"

#include "base/mac/mac_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest_mac.h"
#include "ui/base/accelerators/platform_accelerator_cocoa.h"

using content::SiteInstance;
using content::WebContents;

@interface FullscreenExitBubbleController(JustForTesting)
// Already defined.
+ (NSString*)keyCommandString;
+ (NSString*)keyCombinationForAccelerator:
    (const ui::PlatformAcceleratorCocoa&)item;
@end

@interface FullscreenExitBubbleController(ExposedForTesting)
- (NSTextField*)exitLabelPlaceholder;
- (NSTextView*)exitLabel;
@end

@implementation FullscreenExitBubbleController(ExposedForTesting)
- (NSTextField*)exitLabelPlaceholder {
  return exitLabelPlaceholder_;
}

- (NSTextView*)exitLabel {
  return exitLabel_;
}
@end

class FullscreenExitBubbleControllerTest : public CocoaProfileTest {
 public:
  virtual void SetUp() {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(profile());

    site_instance_ = SiteInstance::Create(profile());
    controller_.reset(
        [[FullscreenExitBubbleController alloc] initWithOwner:nil
                                                      browser:browser()
                                                          url:GURL()
                      bubbleType:FEB_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION]);
    EXPECT_TRUE([controller_ window]);
  }

  virtual void TearDown() {
    [controller_ close];
    controller_.reset();
    CocoaProfileTest::TearDown();
  }

  void AppendTabToStrip() {
    WebContents* web_contents = WebContents::Create(
        content::WebContents::CreateParams(profile(), site_instance_.get()));
    browser()->tab_strip_model()->AppendWebContents(
        web_contents, /*foreground=*/true);
  }

  scoped_refptr<SiteInstance> site_instance_;
  base::scoped_nsobject<FullscreenExitBubbleController> controller_;
};

// http://crbug.com/103912
TEST_F(FullscreenExitBubbleControllerTest, DISABLED_DenyExitsFullscreen) {
  NSWindow* window = browser()->window()->GetNativeWindow();
  BrowserWindowController* bwc = [BrowserWindowController
      browserWindowControllerForWindow:window];

  [bwc showWindow:nil];

  AppendTabToStrip();
  WebContents* fullscreen_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  {
    base::mac::ScopedNSAutoreleasePool pool;
    content::WindowedNotificationObserver fullscreen_observer(
        chrome::NOTIFICATION_FULLSCREEN_CHANGED,
        content::NotificationService::AllSources());
    browser()->ToggleFullscreenModeForTab(fullscreen_tab, true);
    fullscreen_observer.Wait();
    ASSERT_TRUE(browser()->window()->IsFullscreen());
  }

  FullscreenExitBubbleController* bubble = [bwc fullscreenExitBubbleController];
  EXPECT_TRUE(bubble);
  {
    content::WindowedNotificationObserver fullscreen_observer(
        chrome::NOTIFICATION_FULLSCREEN_CHANGED,
        content::NotificationService::AllSources());
    [bubble deny:nil];
    fullscreen_observer.Wait();
  }
  EXPECT_FALSE([bwc fullscreenExitBubbleController]);
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  CloseBrowserWindow();
}

TEST_F(FullscreenExitBubbleControllerTest, LabelWasReplaced) {
  EXPECT_FALSE([controller_ exitLabelPlaceholder]);
  EXPECT_TRUE([controller_ exitLabel]);
}

TEST_F(FullscreenExitBubbleControllerTest, ShortcutText) {
  ui::PlatformAcceleratorCocoa cmd_F(@"F", NSCommandKeyMask);
  ui::PlatformAcceleratorCocoa cmd_shift_f(
      @"f", NSCommandKeyMask | NSShiftKeyMask);
  NSString* cmd_F_text = [FullscreenExitBubbleController
      keyCombinationForAccelerator:cmd_F];
  NSString* cmd_shift_f_text = [FullscreenExitBubbleController
      keyCombinationForAccelerator:cmd_shift_f];
  EXPECT_NSEQ(cmd_shift_f_text, cmd_F_text);
  EXPECT_NSEQ(@"\u2318\u21E7F", cmd_shift_f_text);
}
