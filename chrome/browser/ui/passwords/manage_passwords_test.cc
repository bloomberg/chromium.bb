// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_test.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "testing/gtest/include/gtest/gtest.h"

void ManagePasswordsTest::SetUpOnMainThread() {
  AddTabAtIndex(0, GURL("http://example.com/"), ui::PAGE_TRANSITION_TYPED);
  // Create the test UIController here so that it's bound to the currently
  // active WebContents.
  new ManagePasswordsUIControllerMock(
      browser()->tab_strip_model()->GetActiveWebContents());
}

ManagePasswordsUIControllerMock* ManagePasswordsTest::controller() {
  return static_cast<ManagePasswordsUIControllerMock*>(
      ManagePasswordsUIController::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents()));
}

void ManagePasswordsTest::ExecuteManagePasswordsCommand() {
  // Show the window to ensure that it's active.
  browser()->window()->Show();

  CommandUpdater* updater = browser()->command_controller()->command_updater();
  EXPECT_TRUE(updater->IsCommandEnabled(IDC_MANAGE_PASSWORDS_FOR_PAGE));
  EXPECT_TRUE(updater->ExecuteCommand(IDC_MANAGE_PASSWORDS_FOR_PAGE));

  // Wait for the command execution to pop up the bubble.
  content::RunAllPendingInMessageLoop();
}

void ManagePasswordsTest::SetupManagingPasswords() {
  base::string16 kTestUsername = base::ASCIIToUTF16("test_username");
  autofill::PasswordFormMap map;
  map[kTestUsername] = test_form();
  controller()->OnPasswordAutofilled(map);
  controller()->UpdateIconAndBubbleState(view());
}

void ManagePasswordsTest::SetupPendingPassword() {
  password_manager::StubPasswordManagerClient client;
  password_manager::StubPasswordManagerDriver driver;
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      new password_manager::PasswordFormManager(
          NULL, &client, &driver, *test_form(), false));
  controller()->OnPasswordSubmitted(test_form_manager.Pass());

  // Wait for the command execution triggered by the automatic popup to pop up
  // the bubble.
  content::RunAllPendingInMessageLoop();
  controller()->UpdateIconAndBubbleState(view());
}

void ManagePasswordsTest::SetupAutomaticPassword() {
  password_manager::StubPasswordManagerClient client;
  password_manager::StubPasswordManagerDriver driver;
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      new password_manager::PasswordFormManager(
          NULL, &client, &driver, *test_form(), false));
  controller()->OnAutomaticPasswordSave(test_form_manager.Pass());

  // Wait for the command execution triggered by the automatic popup to pop up
  // the bubble.
  content::RunAllPendingInMessageLoop();
  controller()->UpdateIconAndBubbleState(view());
}

void ManagePasswordsTest::SetupBlackistedPassword() {
  base::string16 kTestUsername = base::ASCIIToUTF16("test_username");
  autofill::PasswordFormMap map;
  map[kTestUsername] = test_form();
  controller()->OnBlacklistBlockedAutofill(map);
  controller()->UpdateIconAndBubbleState(view());
}

base::HistogramSamples* ManagePasswordsTest::GetSamples(
    const char* histogram) {
  // Ensure that everything has been properly recorded before pulling samples.
  content::RunAllPendingInMessageLoop();
  return histogram_tester_.GetHistogramSamplesSinceCreation(histogram)
      .release();
}
