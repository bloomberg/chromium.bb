// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_test.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "testing/gtest/include/gtest/gtest.h"

ManagePasswordsTest::ManagePasswordsTest() {
}

ManagePasswordsTest::~ManagePasswordsTest() {
}

void ManagePasswordsTest::SetUpOnMainThread() {
  AddTabAtIndex(0, GURL("http://example.com/"), ui::PAGE_TRANSITION_TYPED);
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
  map.insert(kTestUsername,
             make_scoped_ptr(new autofill::PasswordForm(*test_form())));
  GetController()->OnPasswordAutofilled(map, map.begin()->second->origin);
}

void ManagePasswordsTest::SetupPendingPassword() {
  password_manager::StubPasswordManagerClient client;
  password_manager::StubPasswordManagerDriver driver;
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      new password_manager::PasswordFormManager(
          NULL, &client, driver.AsWeakPtr(), *test_form(), false));
  test_form_manager->SimulateFetchMatchingLoginsFromPasswordStore();
  ScopedVector<autofill::PasswordForm> best_matches;
  test_form_manager->OnGetPasswordStoreResults(best_matches.Pass());
  GetController()->OnPasswordSubmitted(test_form_manager.Pass());
}

void ManagePasswordsTest::SetupAutomaticPassword() {
  password_manager::StubPasswordManagerClient client;
  password_manager::StubPasswordManagerDriver driver;
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      new password_manager::PasswordFormManager(
          NULL, &client, driver.AsWeakPtr(), *test_form(), false));
  GetController()->OnAutomaticPasswordSave(test_form_manager.Pass());
}

void ManagePasswordsTest::SetupChooseCredentials(
    ScopedVector<autofill::PasswordForm> local_credentials,
    ScopedVector<autofill::PasswordForm> federated_credentials,
    const GURL& origin) {
  base::string16 kTestUsername = base::ASCIIToUTF16("test_username");
  autofill::PasswordFormMap map;
  map.insert(kTestUsername,
             make_scoped_ptr(new autofill::PasswordForm(*test_form())));
  GetController()->OnChooseCredentials(
      local_credentials.Pass(), federated_credentials.Pass(), origin,
      base::Bind(&ManagePasswordsTest::OnChooseCredential, this));
}

void ManagePasswordsTest::SetupAutoSignin(
    ScopedVector<autofill::PasswordForm> local_credentials) {
  GetController()->OnAutoSignin(local_credentials.Pass());
}

scoped_ptr<base::HistogramSamples> ManagePasswordsTest::GetSamples(
    const char* histogram) {
  // Ensure that everything has been properly recorded before pulling samples.
  content::RunAllPendingInMessageLoop();
  return histogram_tester_.GetHistogramSamplesSinceCreation(histogram).Pass();
}

ManagePasswordsUIController* ManagePasswordsTest::GetController() {
  return ManagePasswordsUIController::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
}
