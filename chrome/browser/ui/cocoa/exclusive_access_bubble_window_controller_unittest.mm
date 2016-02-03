// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/exclusive_access_bubble_window_controller.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/browser/exclusive_access_controller_views.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest_mac.h"
#include "ui/base/accelerators/platform_accelerator_cocoa.h"
#import "ui/base/cocoa/controls/hyperlink_text_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

using content::SiteInstance;
using content::WebContents;

@interface ExclusiveAccessBubbleWindowController (JustForTesting)
// Already defined.
+ (NSString*)keyCommandString;
+ (NSString*)keyCombinationForAccelerator:
        (const ui::PlatformAcceleratorCocoa&)item;
- (void)initializeLabelAndButton;
@end

@interface ExclusiveAccessBubbleWindowController (ExposedForTesting)
- (NSTextField*)exitLabelPlaceholder;
- (NSTextView*)exitLabel;
- (NSString*)denyButtonText;
@end

@implementation ExclusiveAccessBubbleWindowController (ExposedForTesting)
- (NSTextField*)exitLabelPlaceholder {
  return exitLabelPlaceholder_;
}

- (HyperlinkTextView*)exitLabel {
  return exitLabel_;
}

- (NSString*)denyButtonText {
  return [denyButton_ title];
}
@end

class ExclusiveAccessBubbleWindowControllerTest : public CocoaProfileTest {
 public:
  void SetUp() override {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(profile());

    site_instance_ = SiteInstance::Create(profile());
    controller_.reset([[ExclusiveAccessBubbleWindowController alloc]
                   initWithOwner:nil
        exclusive_access_manager:browser()->exclusive_access_manager()
                         profile:browser()->profile()
                             url:GURL()
                      bubbleType:
                          EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION]);
    EXPECT_TRUE([controller_ window]);
  }

  void TearDown() override {
    [controller_ close];
    controller_.reset();
    CocoaProfileTest::TearDown();
  }

  void AppendTabToStrip() {
    WebContents* web_contents = WebContents::Create(
        content::WebContents::CreateParams(profile(), site_instance_.get()));
    browser()->tab_strip_model()->AppendWebContents(web_contents,
                                                    /*foreground=*/true);
  }

  scoped_refptr<SiteInstance> site_instance_;
  base::scoped_nsobject<ExclusiveAccessBubbleWindowController> controller_;
};

// http://crbug.com/103912
TEST_F(ExclusiveAccessBubbleWindowControllerTest,
       DISABLED_DenyExitsFullscreen) {
  NSWindow* window = browser()->window()->GetNativeWindow();
  BrowserWindowController* bwc =
      [BrowserWindowController browserWindowControllerForWindow:window];

  [bwc showWindow:nil];

  AppendTabToStrip();
  WebContents* fullscreen_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  {
    base::mac::ScopedNSAutoreleasePool pool;
    content::WindowedNotificationObserver fullscreen_observer(
        chrome::NOTIFICATION_FULLSCREEN_CHANGED,
        content::NotificationService::AllSources());
    browser()->EnterFullscreenModeForTab(fullscreen_tab, GURL());
    fullscreen_observer.Wait();
    ASSERT_TRUE(browser()->window()->IsFullscreen());
  }

  ExclusiveAccessController* access_controller =
      [bwc exclusiveAccessController];
  EXPECT_TRUE(access_controller->cocoa_bubble());
  {
    content::WindowedNotificationObserver fullscreen_observer(
        chrome::NOTIFICATION_FULLSCREEN_CHANGED,
        content::NotificationService::AllSources());
    [access_controller->cocoa_bubble() deny:nil];
    fullscreen_observer.Wait();
  }
  EXPECT_FALSE(access_controller->cocoa_bubble());
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  CloseBrowserWindow();
}

TEST_F(ExclusiveAccessBubbleWindowControllerTest, LabelWasReplaced) {
  EXPECT_FALSE([controller_ exitLabelPlaceholder]);
  EXPECT_TRUE([controller_ exitLabel]);
}

TEST_F(ExclusiveAccessBubbleWindowControllerTest, ShortcutText) {
  ui::PlatformAcceleratorCocoa cmd_F(@"F", NSCommandKeyMask);
  ui::PlatformAcceleratorCocoa cmd_shift_f(@"f",
                                           NSCommandKeyMask | NSShiftKeyMask);
  NSString* cmd_F_text = [ExclusiveAccessBubbleWindowController
      keyCombinationForAccelerator:cmd_F];
  NSString* cmd_shift_f_text = [ExclusiveAccessBubbleWindowController
      keyCombinationForAccelerator:cmd_shift_f];
  EXPECT_NSEQ(cmd_shift_f_text, cmd_F_text);
  EXPECT_NSEQ(@"\u2318\u21E7F", cmd_shift_f_text);
}

// http://crbug.com/139944
TEST_F(ExclusiveAccessBubbleWindowControllerTest, DenyButtonText) {
  controller_.reset([[ExclusiveAccessBubbleWindowController alloc]
                 initWithOwner:nil
      exclusive_access_manager:browser()->exclusive_access_manager()
                       profile:browser()->profile()
                           url:GURL()
                    bubbleType:EXCLUSIVE_ACCESS_BUBBLE_TYPE_MOUSELOCK_BUTTONS]);
  [controller_ initializeLabelAndButton];
  NSString* mouselock_deny_button_text = [controller_ denyButtonText];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_FULLSCREEN_DENY),
    mouselock_deny_button_text);

  controller_.reset([[ExclusiveAccessBubbleWindowController alloc]
                 initWithOwner:nil
      exclusive_access_manager:browser()->exclusive_access_manager()
                       profile:browser()->profile()
                           url:GURL()
                    bubbleType:
                        EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_MOUSELOCK_BUTTONS]);
  [controller_ initializeLabelAndButton];
  NSString* fullscreen_mouselock_deny_button_text =
      [controller_ denyButtonText];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_FULLSCREEN_EXIT),
    fullscreen_mouselock_deny_button_text);
}
