// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_manager_test_base.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

namespace {

class CredentialManagerBrowserTest : public PasswordManagerBrowserTestBase {
 public:
  CredentialManagerBrowserTest() = default;

  void SetUpOnMainThread() override {
    PasswordManagerBrowserTestBase::SetUpOnMainThread();
    // Redirect all requests to localhost.
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  bool IsShowingAccountChooser() {
    return PasswordsModelDelegateFromWebContents(WebContents())->
        GetState() == password_manager::ui::CREDENTIAL_REQUEST_STATE;
  }

  // Similarly to PasswordManagerBrowserTestBase::NavigateToFile this is a
  // wrapper around ui_test_utils::NavigateURL that waits until DidFinishLoad()
  // fires. Different to NavigateToFile this method allows passing a test_server
  // and modifications to the hostname.
  void NavigateToURL(const net::EmbeddedTestServer& test_server,
                     const std::string& hostname,
                     const std::string& relative_url) {
    NavigationObserver observer(WebContents());
    GURL url = test_server.GetURL(hostname, relative_url);
    ui_test_utils::NavigateToURL(browser(), url);
    observer.Wait();
  }

 private:
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
  // Mojo calls from the renderer are asynchronous.
  BubbleObserver(WebContents()).WaitForAccountChooser();
  PasswordsModelDelegateFromWebContents(WebContents())->ChooseCredential(
      signin_form,
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);

  NavigationObserver observer(WebContents());
  observer.SetPathToWaitFor("/password/done.html");
  observer.Wait();

  // Verify that the form's 'skip_zero_click' is updated and not overwritten
  // by the autofill password manager on successful login.
  WaitForPasswordStore();
  password_manager::TestPasswordStore::PasswordMap passwords_map =
      password_store->stored_passwords();
  ASSERT_EQ(1u, passwords_map.size());
  const std::vector<autofill::PasswordForm>& passwords_vector =
      passwords_map.begin()->second;
  ASSERT_EQ(1u, passwords_vector.size());
  const autofill::PasswordForm& form = passwords_vector[0];
  EXPECT_EQ(base::ASCIIToUTF16("user"), form.username_value);
  EXPECT_EQ(base::ASCIIToUTF16("password"), form.password_value);
  EXPECT_FALSE(form.skip_zero_click);
}

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       StoreSavesPSLMatchedCredential) {
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());

  // The call to |GetURL| is needed to get the correct port.
  GURL psl_url = https_test_server().GetURL("psl.example.com", "/");

  autofill::PasswordForm signin_form;
  signin_form.signon_realm = psl_url.spec();
  signin_form.password_value = base::ASCIIToUTF16("password");
  signin_form.username_value = base::ASCIIToUTF16("user");
  signin_form.origin = psl_url;
  password_store->AddLogin(signin_form);

  NavigateToURL(https_test_server(), "www.example.com",
                "/password/password_form.html");

  // Call the API to trigger |get| and |store| and redirect.
  ASSERT_TRUE(
      content::ExecuteScript(RenderViewHost(),
                             "navigator.credentials.get({password: true})"
                             ".then(cred => "
                             "navigator.credentials.store(cred)"
                             ".then(cred => "
                             "window.location = '/password/done.html'))"));

  // Mojo calls from the renderer are asynchronous.
  BubbleObserver(WebContents()).WaitForAccountChooser();
  PasswordsModelDelegateFromWebContents(WebContents())
      ->ChooseCredential(
          signin_form,
          password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);

  NavigationObserver observer(WebContents());
  observer.SetPathToWaitFor("/password/done.html");
  observer.Wait();

  // Wait for the password store before checking the prompt because it pops up
  // after the store replies.
  WaitForPasswordStore();
  BubbleObserver prompt_observer(WebContents());
  EXPECT_FALSE(prompt_observer.IsShowingSavePrompt());
  EXPECT_FALSE(prompt_observer.IsShowingUpdatePrompt());

  // There should be an entry for both psl.example.com and www.example.com.
  password_manager::TestPasswordStore::PasswordMap passwords =
      password_store->stored_passwords();
  GURL www_url = https_test_server().GetURL("www.example.com", "/");
  EXPECT_EQ(2U, passwords.size());
  EXPECT_TRUE(base::ContainsKey(passwords, psl_url.spec()));
  EXPECT_TRUE(base::ContainsKey(passwords, www_url.spec()));
}

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       ObsoleteHttpCredentialMovedOnMigrationToHstsSite) {
  // Add an http credential to the password store.
  GURL https_origin = https_test_server().base_url();
  ASSERT_TRUE(https_origin.SchemeIs(url::kHttpsScheme));
  GURL::Replacements rep;
  rep.SetSchemeStr(url::kHttpScheme);
  GURL http_origin = https_origin.ReplaceComponents(rep);
  autofill::PasswordForm http_form;
  http_form.signon_realm = http_origin.spec();
  http_form.origin = http_origin;
  http_form.username_value = base::ASCIIToUTF16("user");
  http_form.password_value = base::ASCIIToUTF16("12345");
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  password_store->AddLogin(http_form);
  WaitForPasswordStore();

  // Treat the host of the HTTPS test server as HSTS.
  AddHSTSHost(https_test_server().host_port_pair().host());

  // Navigate to HTTPS page and trigger the migration.
  ui_test_utils::NavigateToURL(
      browser(), https_test_server().GetURL("/password/done.html"));

  // Call the API to trigger the account chooser.
  ASSERT_TRUE(content::ExecuteScript(
      RenderViewHost(), "navigator.credentials.get({password: true})"));
  BubbleObserver(WebContents()).WaitForAccountChooser();

  // Wait for the migration logic to actually touch the password store.
  WaitForPasswordStore();
  // Only HTTPS passwords should be present.
  EXPECT_TRUE(
      password_store->stored_passwords().at(http_origin.spec()).empty());
  EXPECT_FALSE(
      password_store->stored_passwords().at(https_origin.spec()).empty());
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
      ".then(cred => window.location = '/password/done.html');"));

  NavigationObserver observer(WebContents());
  observer.SetPathToWaitFor("/password/done.html");
  observer.Wait();

  BubbleObserver prompt_observer(WebContents());
  // The autofill password manager shouldn't react to the successful login
  // because it was suppressed when the site got the credential back.
  EXPECT_FALSE(prompt_observer.IsShowingSavePrompt());
}

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest, SaveViaAPIAndAutofill) {
  NavigateToFile("/password/password_form.html");

  ASSERT_TRUE(content::ExecuteScript(
      RenderViewHost(),
      "document.getElementById('input_submit_button').addEventListener('click',"
      "function(event) {"
        "var c = new PasswordCredential({ id: 'user', password: 'API' });"
        "navigator.credentials.store(c);"
      "});"));
  // Fill the password and click the button to submit the page. The API should
  // suppress the autofill password manager.
  NavigationObserver form_submit_observer(WebContents());
  ASSERT_TRUE(content::ExecuteScript(
      RenderViewHost(),
      "document.getElementById('username_field').value = 'user';"
      "document.getElementById('password_field').value = 'autofill';"
      "document.getElementById('input_submit_button').click();"));
  form_submit_observer.Wait();

  WaitForPasswordStore();
  BubbleObserver prompt_observer(WebContents());
  ASSERT_TRUE(prompt_observer.IsShowingSavePrompt());
  prompt_observer.AcceptSavePrompt();

  WaitForPasswordStore();
  password_manager::TestPasswordStore::PasswordMap stored =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get())->stored_passwords();
  ASSERT_EQ(1u, stored.size());
  autofill::PasswordForm signin_form = stored.begin()->second[0];
  EXPECT_EQ(base::ASCIIToUTF16("user"), signin_form.username_value);
  EXPECT_EQ(base::ASCIIToUTF16("API"), signin_form.password_value);
  EXPECT_EQ(embedded_test_server()->base_url().spec(),
            signin_form.signon_realm);
  EXPECT_EQ(embedded_test_server()->base_url(), signin_form.origin);
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
  signin_form.password_value = base::ASCIIToUTF16("old_pass");
  signin_form.username_value = base::ASCIIToUTF16("user");
  signin_form.origin = embedded_test_server()->base_url();
  signin_form.skip_zero_click = true;
  signin_form.preferred = true;
  password_store->AddLogin(signin_form);

  NavigateToFile("/password/password_form.html");

  ASSERT_TRUE(content::ExecuteScript(
      RenderViewHost(),
      "document.getElementById('input_submit_button').addEventListener('click',"
      "function(event) {"
        "var c = new PasswordCredential({ id: 'user', password: 'API' });"
        "navigator.credentials.store(c);"
      "});"));
  // Fill the new password and click the button to submit the page later. The
  // API should suppress the autofill password manager and overwrite the
  // password.
  NavigationObserver form_submit_observer(WebContents());
  ASSERT_TRUE(content::ExecuteScript(
      RenderViewHost(),
      "document.getElementById('username_field').value = 'user';"
      "document.getElementById('password_field').value = 'autofill';"
      "document.getElementById('input_submit_button').click();"));
  form_submit_observer.Wait();

  // Wait for the password store before checking the prompt because it pops up
  // after the store replies.
  WaitForPasswordStore();
  BubbleObserver prompt_observer(WebContents());
  EXPECT_FALSE(prompt_observer.IsShowingSavePrompt());
  EXPECT_FALSE(prompt_observer.IsShowingUpdatePrompt());
  signin_form.skip_zero_click = false;
  signin_form.times_used = 1;
  signin_form.password_value = base::ASCIIToUTF16("API");
  password_manager::TestPasswordStore::PasswordMap stored =
      password_store->stored_passwords();
  ASSERT_EQ(1u, stored.size());
  EXPECT_EQ(signin_form, stored[signin_form.signon_realm][0]);
}

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest, CredentialsAutofilled) {
  NavigateToFile("/password/password_form.html");

  ASSERT_TRUE(content::ExecuteScript(
      RenderFrameHost(),
      "var c = new PasswordCredential({ id: 'user', password: '12345' });"
      "navigator.credentials.store(c);"));
  BubbleObserver bubble_observer(WebContents());
  bubble_observer.WaitForSavePrompt();
  bubble_observer.AcceptSavePrompt();

  // Reload the page and make sure it's autofilled.
  NavigateToFile("/password/password_form.html");
  WaitForElementValue("username_field", "user");
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(1, 1));
  WaitForElementValue("password_field", "12345");
}

}  // namespace
