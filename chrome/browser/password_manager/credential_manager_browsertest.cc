// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_manager_test_base.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {

class CredentialManagerBrowserTest : public PasswordManagerBrowserTestBase {
 public:
  CredentialManagerBrowserTest() = default;

  bool IsShowingAccountChooser() {
    return PasswordsModelDelegateFromWebContents(WebContents())->GetState() ==
        password_manager::ui::CREDENTIAL_REQUEST_STATE;
  }

 private:
  net::EmbeddedTestServer https_test_server_;

  DISALLOW_COPY_AND_ASSIGN(CredentialManagerBrowserTest);
};

// Tests.

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       AccountChooserWithOldCredentialAndNavigation) {
  // Save credentials with 'skip_zero_click'.
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS).get());
  autofill::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.password_value = base::ASCIIToUTF16("password");
  signin_form.username_value = base::ASCIIToUTF16("user");
  signin_form.origin = embedded_test_server()->base_url();
  signin_form.skip_zero_click = true;
  password_store->AddLogin(signin_form);

  NavigateToFile("/password/password_form.html");
  std::string fill_password =
  "document.getElementById('username_field').value = 'user';"
  "document.getElementById('password_field').value = 'password';";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_password));

  // Call the API to trigger the notification to the client.
  ASSERT_TRUE(content::ExecuteScript(
      RenderViewHost(),
      "navigator.credentials.get({password: true})"
      ".then(cred => window.location = '/password/done.html')"));
  ASSERT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            PasswordsModelDelegateFromWebContents(WebContents())->GetState());
  PasswordsModelDelegateFromWebContents(WebContents())->ChooseCredential(
      signin_form,
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);

  NavigationObserver observer(WebContents());
  observer.SetPathToWaitFor("/password/done.html");
  observer.Wait();

  // Verify that the form's 'skip_zero_click' is updated and not overwritten
  // by the autofill password manager on successful login.
  auto& passwords_map = password_store->stored_passwords();
  ASSERT_EQ(1u, passwords_map.size());
  auto& passwords_vector = passwords_map.begin()->second;
  ASSERT_EQ(1u, passwords_vector.size());
  const autofill::PasswordForm& form = passwords_vector[0];
  EXPECT_EQ(base::ASCIIToUTF16("user"), form.username_value);
  EXPECT_EQ(base::ASCIIToUTF16("password"), form.password_value);
  EXPECT_FALSE(form.skip_zero_click);
}

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       AutoSigninOldCredentialAndNavigation) {
  // Save credentials with 'skip_zero_click' false.
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS).get());
  autofill::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.password_value = base::ASCIIToUTF16("password");
  signin_form.username_value = base::ASCIIToUTF16("user");
  signin_form.origin = embedded_test_server()->base_url();
  signin_form.skip_zero_click = false;
  password_store->AddLogin(signin_form);

  // Enable 'auto signin' for the profile.
  password_bubble_experiment::RecordAutoSignInPromptFirstRunExperienceWasShown(
      browser()->profile()->GetPrefs());

  NavigateToFile("/password/password_form.html");
  std::string fill_password =
  "document.getElementById('username_field').value = 'trash';"
  "document.getElementById('password_field').value = 'trash';";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_password));

  // Call the API to trigger the notification to the client.
  ASSERT_TRUE(content::ExecuteScript(
      RenderViewHost(),
      "navigator.credentials.get({password: true})"
      ".then(cred => window.location = '/password/done.html')"));

  NavigationObserver observer(WebContents());
  observer.SetPathToWaitFor("/password/done.html");
  observer.Wait();

  std::unique_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  // The autofill password manager shouldn't react to the successful login.
  EXPECT_FALSE(prompt_observer->IsShowingPrompt());
}

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest, SaveViaAPIAndAutofill) {
  NavigateToFile("/password/password_form.html");
  std::string fill_password =
  "document.getElementById('username_field').value = 'user';"
  "document.getElementById('password_field').value = '12345';";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_password));

  // Call the API to save the form.
  ASSERT_TRUE(content::ExecuteScript(
      RenderViewHost(),
      "var c = new PasswordCredential({ id: 'user', password: '12345' });"
      "navigator.credentials.store(c);"));
  std::unique_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  EXPECT_TRUE(prompt_observer->IsShowingPrompt());
  prompt_observer->Dismiss();

  NavigationObserver form_submit_observer(WebContents());
  ASSERT_TRUE(content::ExecuteScript(
      RenderViewHost(),
      "document.getElementById('input_submit_button').click();"));
  form_submit_observer.Wait();
  EXPECT_FALSE(prompt_observer->IsShowingPrompt());
}

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest, UpdateViaAPIAndAutofill) {
  // Save credentials with 'skip_zero_click' false.
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  autofill::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.password_value = base::ASCIIToUTF16("12345");
  signin_form.username_value = base::ASCIIToUTF16("user");
  signin_form.origin = embedded_test_server()->base_url();
  signin_form.skip_zero_click = true;
  signin_form.preferred = true;
  password_store->AddLogin(signin_form);

  NavigateToFile("/password/password_form.html");
  std::string fill_password =
      "document.getElementById('username_field').value = 'user';"
      "document.getElementById('password_field').value = '12345';";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_password));

  // Call the API to update the form.
  ASSERT_TRUE(content::ExecuteScript(
      RenderViewHost(),
      "var c = new PasswordCredential({ id: 'user', password: '12345' });"
      "navigator.credentials.store(c);"));
  std::unique_ptr<PromptObserver> prompt_observer(
      PromptObserver::Create(WebContents()));
  EXPECT_FALSE(prompt_observer->IsShowingPrompt());
  EXPECT_FALSE(prompt_observer->IsShowingUpdatePrompt());
  signin_form.skip_zero_click = false;
  signin_form.times_used = 1;
  password_manager::TestPasswordStore::PasswordMap stored =
      password_store->stored_passwords();
  ASSERT_EQ(1u, stored.size());
  EXPECT_EQ(signin_form, stored[signin_form.signon_realm][0]);

  // Verify that the autofill password manager was suppressed and didn't touch
  // the store. It would definitely update the '*_element' fields.
  NavigationObserver form_submit_observer(WebContents());
  ASSERT_TRUE(content::ExecuteScript(
      RenderViewHost(),
      "document.getElementById('input_submit_button').click();"));
  form_submit_observer.Wait();
  EXPECT_FALSE(prompt_observer->IsShowingPrompt());
  EXPECT_FALSE(prompt_observer->IsShowingUpdatePrompt());
  stored = password_store->stored_passwords();
  ASSERT_EQ(1u, stored.size());
  EXPECT_EQ(signin_form, stored[signin_form.signon_realm][0]);
}

}  // namespace
