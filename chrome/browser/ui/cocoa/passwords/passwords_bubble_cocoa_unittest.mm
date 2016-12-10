// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/passwords/passwords_bubble_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/compiler_specific.h"
#include "base/mac/foundation_util.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/cocoa/location_bar/manage_passwords_decoration.h"
#import "chrome/browser/ui/cocoa/passwords/passwords_bubble_controller.h"
#include "chrome/browser/ui/cocoa/test/cocoa_profile_test.h"
#include "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

using testing::Return;
using testing::ReturnRef;

class ManagePasswordsBubbleCocoaTest : public CocoaProfileTest {
 public:
  void SetUp() override {
    CocoaProfileTest::SetUp();

    // Create the WebContents.
    siteInstance_ = content::SiteInstance::Create(profile());
    test_web_contents_ = CreateWebContents();

    // Create the test UIController here so that it's bound to
    // |test_web_contents_| and therefore accessible to the model. It should be
    // done before AppendWebContents() so the real ManagePasswordsUIController
    // isn't created.
    new testing::NiceMock<ManagePasswordsUIControllerMock>(test_web_contents_);
    EXPECT_CALL(*UIController(), GetState())
        .WillOnce(Return(password_manager::ui::INACTIVE_STATE));
    browser()->tab_strip_model()->AppendWebContents(
        test_web_contents_, /*foreground=*/true);
    PasswordStoreFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), password_manager::BuildPasswordStore<
                       content::BrowserContext,
                       testing::NiceMock<password_manager::MockPasswordStore>>);
  }

  content::WebContents* CreateWebContents() {
    return content::WebContentsTester::CreateTestWebContents(
        profile(), siteInstance_.get());
  }

  void ShowBubble(bool user_action) {
    ManagePasswordsUIControllerMock* mock = UIController();
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(mock));
    autofill::PasswordForm form;
    EXPECT_CALL(*mock, GetPendingPassword()).WillOnce(ReturnRef(form));
    GURL origin;
    EXPECT_CALL(*mock, GetOrigin()).WillOnce(ReturnRef(origin));
    EXPECT_CALL(*mock, GetState())
        .WillOnce(Return(password_manager::ui::PENDING_PASSWORD_STATE));

    TabDialogs::FromWebContents(test_web_contents_)
        ->ShowManagePasswordsBubble(user_action);
    if (ManagePasswordsBubbleCocoa::instance()) {
      // Disable animations so that closing happens immediately.
      InfoBubbleWindow* bubbleWindow = base::mac::ObjCCast<InfoBubbleWindow>(
          [ManagePasswordsBubbleCocoa::instance()->controller_ window]);
      [bubbleWindow setAllowedAnimations:info_bubble::kAnimateNone];
    }
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(mock));
  }

  void CloseBubble() {
    ManagePasswordsBubbleCocoa::instance()->Close();
  }

  NSWindow* bubbleWindow() {
    ManagePasswordsBubbleCocoa* bubble = ManagePasswordsBubbleCocoa::instance();
    return bubble ? [bubble->controller_ window] : nil;
  }

  ManagePasswordsUIControllerMock* UIController() {
    return static_cast<ManagePasswordsUIControllerMock*>(
        ManagePasswordsUIController::FromWebContents(test_web_contents_));
  }

  ManagePasswordsBubbleController* controller() {
    ManagePasswordsBubbleCocoa* bubble = ManagePasswordsBubbleCocoa::instance();
    return bubble ? bubble->controller_ : nil;
  }

  ManagePasswordsIconCocoa* icon() {
    return static_cast<ManagePasswordsIconCocoa*>(
        ManagePasswordsBubbleCocoa::instance()->icon_);
  }

 private:
  scoped_refptr<content::SiteInstance> siteInstance_;
  content::WebContents* test_web_contents_;  // weak
};

TEST_F(ManagePasswordsBubbleCocoaTest, ShowShouldCreateAndShowBubble) {
  EXPECT_FALSE(ManagePasswordsBubbleCocoa::instance());
  EXPECT_FALSE([bubbleWindow() isVisible]);
  ShowBubble(false);
  EXPECT_TRUE(ManagePasswordsBubbleCocoa::instance());
  EXPECT_TRUE([bubbleWindow() isVisible]);
  EXPECT_TRUE([bubbleWindow() autorecalculatesKeyViewLoop]);
}

TEST_F(ManagePasswordsBubbleCocoaTest, CloseShouldCloseAndDeleteBubble) {
  ShowBubble(false);
  EXPECT_TRUE(ManagePasswordsBubbleCocoa::instance());
  EXPECT_TRUE([bubbleWindow() isVisible]);
  CloseBubble();
  EXPECT_FALSE(ManagePasswordsBubbleCocoa::instance());
  EXPECT_FALSE([bubbleWindow() isVisible]);
}

TEST_F(ManagePasswordsBubbleCocoaTest, BackgroundCloseShouldDeleteBubble) {
  ShowBubble(false);
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

  // Open a second tab as inactive.
  content::WebContents* webContents2 = CreateWebContents();
  new testing::StrictMock<ManagePasswordsUIControllerMock>(webContents2);
  tabStripModel->AppendWebContents(webContents2, /*foreground=*/false);
  EXPECT_EQ(0, tabStripModel->active_index());
  EXPECT_EQ(2, tabStripModel->count());

  // Try to show the bubble on the inactive tab. Nothing should happen.
  TabDialogs::FromWebContents(tabStripModel->GetWebContentsAt(1))
      ->ShowManagePasswordsBubble(false);
  EXPECT_FALSE(ManagePasswordsBubbleCocoa::instance());

  // Show the bubble for the active tab.
  ShowBubble(false);
  EXPECT_TRUE(ManagePasswordsBubbleCocoa::instance());
  EXPECT_TRUE([bubbleWindow() isVisible]);
}

TEST_F(ManagePasswordsBubbleCocoaTest, HideBubbleOnChangedState) {
  ShowBubble(false);
  EXPECT_TRUE(ManagePasswordsBubbleCocoa::instance());
  EXPECT_TRUE([bubbleWindow() isVisible]);
  EXPECT_TRUE(icon()->active());

  icon()->OnChangingState();
  EXPECT_FALSE(ManagePasswordsBubbleCocoa::instance());
  EXPECT_FALSE([bubbleWindow() isVisible]);
}

TEST_F(ManagePasswordsBubbleCocoaTest, ShowBubbleTwice) {
  ShowBubble(false);
  base::scoped_nsobject<NSWindow> bubble([bubbleWindow() retain]);
  // Opening the bubble again should retrieve the data from the UI controller
  // again.
  ShowBubble(false);
  EXPECT_TRUE(ManagePasswordsBubbleCocoa::instance());
  EXPECT_NSNE(bubble, bubbleWindow());
  EXPECT_TRUE([bubbleWindow() isVisible]);
}

TEST_F(ManagePasswordsBubbleCocoaTest, OpenWithoutFocus) {
  ShowBubble(false);
  EXPECT_TRUE(ManagePasswordsBubbleCocoa::instance());
  EXPECT_FALSE([controller() shouldOpenAsKeyWindow]);
  EXPECT_FALSE([bubbleWindow() defaultButtonCell]);
}

TEST_F(ManagePasswordsBubbleCocoaTest, OpenWithFocus) {
  ShowBubble(true);
  EXPECT_TRUE(ManagePasswordsBubbleCocoa::instance());
  EXPECT_TRUE([controller() shouldOpenAsKeyWindow]);
  EXPECT_TRUE([bubbleWindow() defaultButtonCell]);
}
