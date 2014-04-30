// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_view_test.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_ui_controller_mock.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/mock_password_manager_driver.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

void ManagePasswordsViewTest::SetUpOnMainThread() {
  // Create the test UIController here so that it's bound to the currently
  // active WebContents.
  new ManagePasswordsBubbleUIControllerMock(
      browser()->tab_strip_model()->GetActiveWebContents());
}

ManagePasswordsBubbleUIControllerMock* ManagePasswordsViewTest::controller() {
  return static_cast<ManagePasswordsBubbleUIControllerMock*>(
      ManagePasswordsBubbleUIController::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents()));
}

ManagePasswordsIconView* ManagePasswordsViewTest::view() {
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  return browser_view->GetToolbarView()->location_bar()->
      manage_passwords_icon_view();
}

void ManagePasswordsViewTest::ExecuteManagePasswordsCommand() {
  CommandUpdater* updater = browser()->command_controller()->command_updater();
  EXPECT_TRUE(updater->IsCommandEnabled(IDC_MANAGE_PASSWORDS_FOR_PAGE));
  EXPECT_TRUE(updater->ExecuteCommand(IDC_MANAGE_PASSWORDS_FOR_PAGE));
}

void ManagePasswordsViewTest::SetupManagingPasswords() {
  base::string16 kTestUsername = base::ASCIIToUTF16("test_username");
  autofill::PasswordFormMap map;
  map[kTestUsername] = test_form();
  controller()->OnPasswordAutofilled(map);
  controller()->UpdateIconAndBubbleState(view());
}

void ManagePasswordsViewTest::SetupPendingPassword() {
  password_manager::StubPasswordManagerClient client;
  password_manager::MockPasswordManagerDriver driver;
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      new password_manager::PasswordFormManager(
          NULL, &client, &driver, *test_form(), false));
  controller()->OnPasswordSubmitted(test_form_manager.release());
  controller()->UpdateIconAndBubbleState(view());
}

void ManagePasswordsViewTest::SetupBlackistedPassword() {
  controller()->OnBlacklistBlockedAutofill();
  controller()->UpdateIconAndBubbleState(view());
}
