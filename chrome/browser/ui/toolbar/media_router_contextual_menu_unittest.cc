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

// Tests the basic state of the contextual menu.
TEST_F(MediaRouterContextualMenuUnitTest, Basic) {
  int expected_number_items = 7;

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
  // On all platforms except Linux, there's an additional menu item to access
  // Cast device management.
  expected_number_items++;
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)

#if defined(GOOGLE_CHROME_BUILD)
  // In official Chrome builds, there's an additional menu item to toggle cloud
  // services settings.
  expected_number_items++;
#endif  // GOOGLE_CHROME_BUILD

  // Verify the number of menu items, including separators.
  EXPECT_EQ(model()->GetItemCount(), expected_number_items);

  for (int i = 0; i < expected_number_items; i++) {
    EXPECT_TRUE(model()->IsEnabledAt(i));
    bool expected_visibility = true;

#if defined(GOOGLE_CHROME_BUILD)
    // In official Chrome builds, the cloud services toggle exists and is
    // enabled, but not visible until the user has authenticated their account.
    expected_visibility =
        model()->GetCommandIdAt(i) != IDC_MEDIA_ROUTER_CLOUD_SERVICES_TOGGLE;
#endif  // GOOGLE_CHROME_BUILD

    EXPECT_EQ(expected_visibility, model()->IsVisibleAt(i));
  }

  // Set up an authenticated account.
  signin_manager()->SetAuthenticatedAccountInfo("foo@bar.com", "password");

  // Run the same checks as before. All existing menu items should be now
  // enabled and visible.
  EXPECT_EQ(model()->GetItemCount(), expected_number_items);
  for (int i = 0; i < expected_number_items; i++) {
    EXPECT_TRUE(model()->IsEnabledAt(i));
    EXPECT_TRUE(model()->IsVisibleAt(i));
  }
}
