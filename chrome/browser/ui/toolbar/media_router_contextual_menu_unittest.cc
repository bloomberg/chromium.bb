// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/toolbar/component_toolbar_actions_factory.h"
#include "chrome/browser/ui/toolbar/media_router_action.h"
#include "chrome/browser/ui/toolbar/media_router_action_controller.h"
#include "chrome/browser/ui/toolbar/media_router_contextual_menu.h"
#include "chrome/test/base/browser_with_test_window_test.h"

namespace {

bool HasCommandId(ui::MenuModel* menu_model, int command_id) {
  for (int i = 0; i < menu_model->GetItemCount(); i++) {
    if (menu_model->GetCommandIdAt(i) == command_id)
      return true;
  }
  return false;
}

}  // namespace

class MediaRouterContextualMenuUnitTest : public BrowserWithTestWindowTest {
 public:
  MediaRouterContextualMenuUnitTest() {}
  ~MediaRouterContextualMenuUnitTest() override {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    toolbar_actions_model_ =
        extensions::extension_action_test_util::CreateToolbarModelForProfile(
            profile());

    signin_manager_ =
        SigninManagerFactory::GetInstance()->GetForProfile(profile());
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

  SigninManagerBase* signin_manager() { return signin_manager_; }
  ui::SimpleMenuModel* model() { return model_; }
  ToolbarActionsModel* toolbar_actions_model() {
    return toolbar_actions_model_;
  }

 private:
  std::unique_ptr<BrowserActionTestUtil> browser_action_test_util_;
  std::unique_ptr<MediaRouterAction> action_;
  SigninManagerBase* signin_manager_;
  ui::SimpleMenuModel* model_;
  ToolbarActionsModel* toolbar_actions_model_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterContextualMenuUnitTest);
};

// Tests the basic state of the contextual menu.
TEST_F(MediaRouterContextualMenuUnitTest, Basic) {
  // About
  // -----
  // Learn more
  // Help
  // Always show icon (checkbox)
  // -----
  // Enable cloud services (checkbox)
  // Report an issue
  int expected_number_items = 8;

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
  // On all platforms except Linux, there's an additional menu item to access
  // Cast device management.
  expected_number_items++;
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)

  // Verify the number of menu items, including separators.
  EXPECT_EQ(model()->GetItemCount(), expected_number_items);

  for (int i = 0; i < expected_number_items; i++) {
    EXPECT_TRUE(model()->IsEnabledAt(i));

    // The cloud services toggle exists and is enabled, but not visible until
    // the user has authenticated their account.
    const bool expected_visibility =
        model()->GetCommandIdAt(i) != IDC_MEDIA_ROUTER_CLOUD_SERVICES_TOGGLE;
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

// Tests whether the cloud services item is correctly toggled. This menu item
// is only availble on official Chrome builds.
TEST_F(MediaRouterContextualMenuUnitTest, ToggleCloudServicesItem) {
  // The Media Router Action has a getter for the model, but not the delegate.
  // Create the MediaRouterContextualMenu ui::SimpleMenuModel::Delegate here.
  MediaRouterContextualMenu menu(browser());

  // Set up an authenticated account such that the cloud services menu item is
  // surfaced. Whether or not it is surfaced is tested in the "Basic" test.
  signin_manager()->SetAuthenticatedAccountInfo("foo@bar.com", "password");

  // By default, the command is not checked.
  EXPECT_FALSE(menu.IsCommandIdChecked(
      IDC_MEDIA_ROUTER_CLOUD_SERVICES_TOGGLE));

  menu.ExecuteCommand(IDC_MEDIA_ROUTER_CLOUD_SERVICES_TOGGLE, 0);
  EXPECT_TRUE(menu.IsCommandIdChecked(
      IDC_MEDIA_ROUTER_CLOUD_SERVICES_TOGGLE));

  menu.ExecuteCommand(IDC_MEDIA_ROUTER_CLOUD_SERVICES_TOGGLE, 0);
  EXPECT_FALSE(menu.IsCommandIdChecked(
      IDC_MEDIA_ROUTER_CLOUD_SERVICES_TOGGLE));
}

TEST_F(MediaRouterContextualMenuUnitTest, ToggleAlwaysShowIconItem) {
  MediaRouterContextualMenu menu(browser());

  // Whether the option is checked should reflect the pref.
  MediaRouterActionController::SetAlwaysShowActionPref(profile(), true);
  EXPECT_TRUE(
      menu.IsCommandIdChecked(IDC_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION));

  MediaRouterActionController::SetAlwaysShowActionPref(profile(), false);
  EXPECT_FALSE(
      menu.IsCommandIdChecked(IDC_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION));

  // Executing the option should toggle the pref.
  menu.ExecuteCommand(IDC_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION, 0);
  EXPECT_TRUE(MediaRouterActionController::GetAlwaysShowActionPref(profile()));

  menu.ExecuteCommand(IDC_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION, 0);
  EXPECT_FALSE(MediaRouterActionController::GetAlwaysShowActionPref(profile()));
}

TEST_F(MediaRouterContextualMenuUnitTest, ActionShownByPolicy) {
  // Create a contextual menu for an icon shown by administrator policy.
  MediaRouterContextualMenu menu(browser(), true);

  // The item "Added by your administrator" should be shown disabled.
  EXPECT_TRUE(menu.IsCommandIdVisible(IDC_MEDIA_ROUTER_SHOWN_BY_POLICY));
  EXPECT_FALSE(menu.IsCommandIdEnabled(IDC_MEDIA_ROUTER_SHOWN_BY_POLICY));

  // The checkbox item "Always show icon" should not be shown.
  EXPECT_FALSE(HasCommandId(menu.menu_model(),
                            IDC_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION));
}
