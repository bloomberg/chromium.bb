// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/compiler_specific.h"
#include "base/mac/foundation_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/cocoa/location_bar/manage_passwords_decoration.h"
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
    webContents_ = CreateWebContents();
    browser()->tab_strip_model()->AppendWebContents(
        webContents_, /*foreground=*/true);

    // Create the test UIController here so that it's bound to
    // |test_web_contents_| and therefore accessible to the model.
    ManagePasswordsUIControllerMock* ui_controller =
        new ManagePasswordsUIControllerMock(webContents_);
    // Set the initial state.
    ui_controller->SetState(password_manager::ui::PENDING_PASSWORD_STATE);
  }

  content::WebContents* webContents() { return webContents_; }

  content::WebContents* CreateWebContents() {
    return content::WebContents::Create(
        content::WebContents::CreateParams(profile(), siteInstance_.get()));
  }

  void ShowBubble() {
    chrome::ShowManagePasswordsBubble(webContents());
    if (ManagePasswordsBubbleCocoa::instance()) {
      // Disable animations so that closing happens immediately.
      InfoBubbleWindow* bubbleWindow = base::mac::ObjCCast<InfoBubbleWindow>(
          [ManagePasswordsBubbleCocoa::instance()->controller_ window]);
      [bubbleWindow setAllowedAnimations:info_bubble::kAnimateNone];
    }
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

TEST_F(ManagePasswordsBubbleCocoaTest, ShowBubbleOnInactiveTabShouldDoNothing) {
  // Start in the tab that we'll try to show the bubble on.
  TabStripModel* tabStripModel = browser()->tab_strip_model();
  EXPECT_EQ(0, tabStripModel->active_index());

  // Open a second tab and make it active.
  content::WebContents* webContents2 = CreateWebContents();
  tabStripModel->AppendWebContents(webContents2, /*foreground=*/true);
  EXPECT_EQ(1, tabStripModel->active_index());
  EXPECT_EQ(2, tabStripModel->count());

  // Try to show the bubble on the inactive tab. Nothing should happen.
  ShowBubble();
  EXPECT_FALSE(ManagePasswordsBubbleCocoa::instance());
}
