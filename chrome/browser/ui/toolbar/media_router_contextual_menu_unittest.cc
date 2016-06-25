// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/toolbar/media_router_action.h"
#include "chrome/browser/ui/toolbar/media_router_contextual_menu.h"
#include "chrome/test/base/browser_with_test_window_test.h"

class TestSimpleMenuModel : public ui::SimpleMenuModel {

};

class MediaRouterContextualMenuUnitTest : public BrowserWithTestWindowTest {
 public:
  MediaRouterContextualMenuUnitTest() {}
  ~MediaRouterContextualMenuUnitTest() override {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    signin_manager_ = static_cast<FakeSigninManagerForTesting*>(
        SigninManagerFactory::GetInstance()->GetForProfile(profile()));
    browser_action_test_util_.reset(
        new BrowserActionTestUtil(browser(), false));
    action_.reset(new MediaRouterAction(browser(),
        browser_action_test_util_->GetToolbarActionsBar()));
    model_ = static_cast<ui::SimpleMenuModel*>(action_->GetContextMenu());
  }

  void TearDown() override {
    action_.reset();
    browser_action_test_util_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  FakeSigninManagerForTesting* signin_manager() { return signin_manager_; }
  ui::SimpleMenuModel* model() { return model_; }

 private:
  std::unique_ptr<BrowserActionTestUtil> browser_action_test_util_;
  std::unique_ptr<MediaRouterAction> action_;
  FakeSigninManagerForTesting* signin_manager_;
  ui::SimpleMenuModel* model_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterContextualMenuUnitTest);
};

#if !defined(GOOGLE_CHROME_BUILD)
// Tests the basic state of the contextual menu when not using an official
// Chrome build.
TEST_F(MediaRouterContextualMenuUnitTest, BasicNotOfficialChromeBuild) {
  // Verify the number of menu items, including separators.
  int expected_number_items = 7;
  EXPECT_EQ(model()->GetItemCount(), expected_number_items);

  // For non-official Chrome builds, all menu items are enabled and visible.
  for (int i = 0; i < expected_number_items; i++) {
    EXPECT_TRUE(model()->IsEnabledAt(i));
    EXPECT_TRUE(model()->IsVisibleAt(i));
  }

  // Set up an authenticated account. There should be no difference in the
  // menu since this is not an official Chrome build.
  signin_manager()->SetAuthenticatedAccountInfo("foo@bar.com", "password");

  // Run the same checks as before.
  EXPECT_EQ(model()->GetItemCount(), expected_number_items);
  for (int i = 0; i < expected_number_items; i++) {
    EXPECT_TRUE(model()->IsEnabledAt(i));
    EXPECT_TRUE(model()->IsVisibleAt(i));
  }
}
#else
// Tests the basic state of the contextual menu when using an official
// Chrome build.
TEST_F(MediaRouterContextualMenuUnitTest, BasicOfficialChromeBuild) {
  // Verify the number of menu items, including separators. In official Chrome
  // builds, there's an additional menu item for cloud services.
  int expected_number_items = 8;
  EXPECT_EQ(model()->GetItemCount(), expected_number_items);

  for (int i = 0; i < expected_number_items; i++) {
    EXPECT_TRUE(model()->IsEnabledAt(i));

    // The cloud services toggle exists an is enabled, but not visible until
    // the user has authenticated their account.
    if (model()->GetCommandIdAt(i) == IDC_MEDIA_ROUTER_CLOUD_SERVICES_TOGGLE)
      EXPECT_FALSE(model()->IsVisibleAt(i));
    else
      EXPECT_TRUE(model()->IsVisibleAt(i));
  }

  // Set up an authenticated account. The cloud services menu item should now
  // be visible.
  signin_manager()->SetAuthenticatedAccountInfo("foo@bar.com", "password");

  // Verify the number of menu items is the same.
  EXPECT_EQ(model()->GetItemCount(), expected_number_items);

  // Verify all of the menu items are enabled and visible.
  for (int i = 0; i < expected_number_items; i++) {
    EXPECT_TRUE(model()->IsEnabledAt(i));
    EXPECT_TRUE(model()->IsVisibleAt(i));
  }
}
#endif  // defined(GOOGLE_CHROME_BUILD)
