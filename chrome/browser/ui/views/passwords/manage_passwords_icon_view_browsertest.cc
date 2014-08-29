// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_icon_view.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon.h"
#include "chrome/browser/ui/passwords/manage_passwords_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "content/public/test/test_utils.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

class ManagePasswordsIconViewTest : public ManagePasswordsTest {
 public:
  ManagePasswordsIconViewTest() {}
  virtual ~ManagePasswordsIconViewTest() {}

  virtual ManagePasswordsIconView* view() OVERRIDE {
    BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
    return static_cast<ManagePasswordsIconView*>(
        browser_view->GetToolbarView()
            ->location_bar()
            ->manage_passwords_icon_view());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsIconViewTest);
};

IN_PROC_BROWSER_TEST_F(ManagePasswordsIconViewTest, DefaultStateIsInactive) {
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, view()->state());
  EXPECT_FALSE(view()->visible());
  EXPECT_EQ(0, view()->icon_id());
  EXPECT_EQ(0, view()->tooltip_text_id());
  view()->SetActive(true);
  EXPECT_EQ(0, view()->icon_id());
  view()->SetActive(false);
  EXPECT_EQ(0, view()->icon_id());
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsIconViewTest, PendingState) {
  SetupPendingPassword();
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE, view()->state());
  EXPECT_TRUE(view()->visible());
  EXPECT_TRUE(view()->active());
  EXPECT_EQ(IDR_SAVE_PASSWORD_ACTIVE, view()->icon_id());
  EXPECT_EQ(IDS_PASSWORD_MANAGER_TOOLTIP_SAVE, view()->tooltip_text_id());
  view()->SetActive(false);
  EXPECT_EQ(IDR_SAVE_PASSWORD_INACTIVE, view()->icon_id());
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsIconViewTest, ManageState) {
  SetupManagingPasswords();
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, view()->state());
  EXPECT_TRUE(view()->visible());
  EXPECT_EQ(IDR_SAVE_PASSWORD_INACTIVE, view()->icon_id());
  EXPECT_EQ(IDS_PASSWORD_MANAGER_TOOLTIP_MANAGE, view()->tooltip_text_id());
  view()->SetActive(true);
  EXPECT_EQ(IDR_SAVE_PASSWORD_ACTIVE, view()->icon_id());
  view()->SetActive(false);
  EXPECT_EQ(IDR_SAVE_PASSWORD_INACTIVE, view()->icon_id());
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsIconViewTest, BlacklistedState) {
  SetupBlackistedPassword();
  EXPECT_EQ(password_manager::ui::BLACKLIST_STATE, view()->state());
  EXPECT_TRUE(view()->visible());
  EXPECT_EQ(IDR_SAVE_PASSWORD_DISABLED_INACTIVE, view()->icon_id());
  EXPECT_EQ(IDS_PASSWORD_MANAGER_TOOLTIP_MANAGE, view()->tooltip_text_id());
  view()->SetActive(true);
  EXPECT_EQ(IDR_SAVE_PASSWORD_DISABLED_ACTIVE, view()->icon_id());
  view()->SetActive(false);
  EXPECT_EQ(IDR_SAVE_PASSWORD_DISABLED_INACTIVE, view()->icon_id());
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsIconViewTest, CloseOnClick) {
  SetupPendingPassword();
  EXPECT_TRUE(view()->visible());
  EXPECT_TRUE(view()->active());
  ui::MouseEvent mouse_down(ui::ET_MOUSE_PRESSED,
                            gfx::Point(10, 10), gfx::Point(900, 60),
                            ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  view()->OnMousePressed(mouse_down);
  // Wait for the command execution to close the bubble.
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(view()->active());
}
