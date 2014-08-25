// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/compiler_specific.h"
#include "base/mac/foundation_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

class ManagePasswordsBubbleCocoaTest : public CocoaProfileTest {
 public:
  virtual void SetUp() OVERRIDE {
    CocoaProfileTest::SetUp();

    // Create the WebContents.
    siteInstance_ = content::SiteInstance::Create(profile());
    webContents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile(), siteInstance_.get()));
    browser()->tab_strip_model()->AppendWebContents(
        webContents_, /*foreground=*/true);

    // Create the test UIController here so that it's bound to
    // |test_web_contents_| and therefore accessible to the model.
    new ManagePasswordsUIControllerMock(webContents_);
  }

  content::WebContents* webContents() { return webContents_; }

  void ShowBubble() {
    ManagePasswordsBubbleCocoa::ShowBubble(
        webContents(), ManagePasswordsBubble::DisplayReason::AUTOMATIC);
    // Disable animations so that closing happens immediately.
    InfoBubbleWindow* window = base::mac::ObjCCast<InfoBubbleWindow>(
        [ManagePasswordsBubbleCocoa::instance()->controller_ window]);
    [window setAllowedAnimations:info_bubble::kAnimateNone];
  }

  void CloseBubble() {
    ManagePasswordsBubbleCocoa::instance()->Close();
  }

  NSWindow* bubbleWindow() {
    ManagePasswordsBubbleCocoa* bubble = ManagePasswordsBubbleCocoa::instance();
    return bubble ? [bubble->controller_ window] : nil;
  }

 private:
  scoped_refptr<content::SiteInstance> siteInstance_;
  content::WebContents* webContents_;  // weak
};

TEST_F(ManagePasswordsBubbleCocoaTest, ShowShouldCreateAndShowBubble) {
  EXPECT_FALSE(ManagePasswordsBubbleCocoa::instance());
  EXPECT_FALSE([bubbleWindow() isVisible]);
  ShowBubble();
  EXPECT_TRUE(ManagePasswordsBubbleCocoa::instance());
  EXPECT_TRUE([bubbleWindow() isVisible]);
}

TEST_F(ManagePasswordsBubbleCocoaTest, CloseShouldCloseAndDeleteBubble) {
  ShowBubble();
  EXPECT_TRUE(ManagePasswordsBubbleCocoa::instance());
  EXPECT_TRUE([bubbleWindow() isVisible]);
  CloseBubble();
  EXPECT_FALSE(ManagePasswordsBubbleCocoa::instance());
  EXPECT_FALSE([bubbleWindow() isVisible]);
}

TEST_F(ManagePasswordsBubbleCocoaTest, BackgroundCloseShouldDeleteBubble) {
  ShowBubble();
  EXPECT_TRUE(ManagePasswordsBubbleCocoa::instance());
  EXPECT_TRUE([bubbleWindow() isVisible]);
  // Close the window directly instead of using the bubble interface.
  [bubbleWindow() close];
  EXPECT_FALSE(ManagePasswordsBubbleCocoa::instance());
  EXPECT_FALSE([bubbleWindow() isVisible]);
}
