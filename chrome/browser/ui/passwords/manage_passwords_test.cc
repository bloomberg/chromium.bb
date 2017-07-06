// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_test.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/passwords_client_ui_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/stub_form_saver.h"

namespace {
constexpr char kTestOrigin[] = "https://www.example.com";
constexpr char kTestUsername[] = "test_username";
constexpr char kTestPassword[] = "test_password";
}  // namespace

ManagePasswordsTest::ManagePasswordsTest() {
  fetcher_.Fetch();

  // Populate |test_form_| with some dummy data.
  test_form_.signon_realm = kTestOrigin;
  test_form_.origin = GURL(kTestOrigin);
  test_form_.username_value = base::ASCIIToUTF16(kTestUsername);
  test_form_.password_value = base::ASCIIToUTF16(kTestPassword);
}

ManagePasswordsTest::~ManagePasswordsTest() {
}

void ManagePasswordsTest::SetUpOnMainThread() {
  AddTabAtIndex(0, GURL(kTestOrigin), ui::PAGE_TRANSITION_TYPED);
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
  std::map<base::string16, const autofill::PasswordForm*> map;
  map.insert(std::make_pair(base::ASCIIToUTF16(kTestUsername), test_form()));
  GetController()->OnPasswordAutofilled(map, map.begin()->second->origin,
                                        nullptr);
}

void ManagePasswordsTest::SetupPendingPassword() {
  std::unique_ptr<password_manager::PasswordFormManager> test_form_manager(
      new password_manager::PasswordFormManager(
          nullptr, &client_, driver_.AsWeakPtr(), *test_form(),
          base::WrapUnique(new password_manager::StubFormSaver), &fetcher_));
  test_form_manager->Init(nullptr);
  fetcher_.SetNonFederated(std::vector<const autofill::PasswordForm*>(), 0u);
  GetController()->OnPasswordSubmitted(std::move(test_form_manager));
}

void ManagePasswordsTest::SetupAutomaticPassword() {
  std::unique_ptr<password_manager::PasswordFormManager> test_form_manager(
      new password_manager::PasswordFormManager(
          nullptr, &client_, driver_.AsWeakPtr(), *test_form(),
          base::WrapUnique(new password_manager::StubFormSaver), &fetcher_));
  test_form_manager->Init(nullptr);
  fetcher_.SetNonFederated(std::vector<const autofill::PasswordForm*>(), 0u);
  GetController()->OnAutomaticPasswordSave(std::move(test_form_manager));
}

void ManagePasswordsTest::SetupAutoSignin(
    std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials) {
  ASSERT_FALSE(local_credentials.empty());
  GURL origin = local_credentials[0]->origin;
  GetController()->OnAutoSignin(std::move(local_credentials), origin);
}

std::unique_ptr<base::HistogramSamples> ManagePasswordsTest::GetSamples(
    const char* histogram) {
  // Ensure that everything has been properly recorded before pulling samples.
  content::RunAllPendingInMessageLoop();
  return histogram_tester_.GetHistogramSamplesSinceCreation(histogram);
}

PasswordsClientUIDelegate* ManagePasswordsTest::GetController() {
  return PasswordsClientUIDelegateFromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
}
