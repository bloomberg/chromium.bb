// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/passwords/account_chooser_dialog_view.h"
#include "chrome/browser/ui/views/passwords/auto_signin_first_run_dialog_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/widget/widget.h"

using ::testing::Field;

namespace {

// A helper class that will create FakeURLFetcher and record the requested URLs.
class TestURLFetcherCallback {
 public:
  scoped_ptr<net::FakeURLFetcher> CreateURLFetcher(
      const GURL& url,
      net::URLFetcherDelegate* d,
      const std::string& response_data,
      net::HttpStatusCode response_code,
      net::URLRequestStatus::Status status) {
    OnRequestDone(url);
    return scoped_ptr<net::FakeURLFetcher>(new net::FakeURLFetcher(
        url, d, response_data, response_code, status));
  }

  MOCK_METHOD1(OnRequestDone, void(const GURL&));
};

// ManagePasswordsUIController subclass to capture the dialog instance
class TestManagePasswordsUIController : public ManagePasswordsUIController {
 public:
  explicit TestManagePasswordsUIController(content::WebContents* web_contents);

  void OnDialogHidden() override;
  AccountChooserPrompt* CreateAccountChooser(
      PasswordDialogController* controller) override;
  AutoSigninFirstRunPrompt* CreateAutoSigninPrompt(
      PasswordDialogController* controller) override;

  AccountChooserDialogView* current_account_chooser() const {
    return static_cast<AccountChooserDialogView*>(current_account_chooser_);
  }

  AutoSigninFirstRunDialogView* current_autosignin_prompt() const {
    return static_cast<AutoSigninFirstRunDialogView*>(
        current_autosignin_prompt_);
  }

  MOCK_METHOD0(OnDialogClosed, void());

 private:
  AccountChooserPrompt* current_account_chooser_;
  AutoSigninFirstRunPrompt* current_autosignin_prompt_;

  DISALLOW_COPY_AND_ASSIGN(TestManagePasswordsUIController);
};

TestManagePasswordsUIController::TestManagePasswordsUIController(
    content::WebContents* web_contents)
    : ManagePasswordsUIController(web_contents),
      current_account_chooser_(nullptr),
      current_autosignin_prompt_(nullptr) {
  // Attach TestManagePasswordsUIController to |web_contents| so the default
  // ManagePasswordsUIController isn't created.
  // Do not silently replace an existing ManagePasswordsUIController because it
  // unregisters itself in WebContentsDestroyed().
  EXPECT_FALSE(web_contents->GetUserData(UserDataKey()));
  web_contents->SetUserData(UserDataKey(), this);
}

void TestManagePasswordsUIController::OnDialogHidden() {
  ManagePasswordsUIController::OnDialogHidden();
  OnDialogClosed();
}

AccountChooserPrompt* TestManagePasswordsUIController::CreateAccountChooser(
    PasswordDialogController* controller) {
  current_account_chooser_ =
      ManagePasswordsUIController::CreateAccountChooser(controller);
  return current_account_chooser_;
}

AutoSigninFirstRunPrompt*
TestManagePasswordsUIController::CreateAutoSigninPrompt(
    PasswordDialogController* controller) {
  current_autosignin_prompt_ =
      ManagePasswordsUIController::CreateAutoSigninPrompt(controller);
  return current_autosignin_prompt_;
}

class PasswordDialogViewTest : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override;

  void SetupChooseCredentials(
      ScopedVector<autofill::PasswordForm> local_credentials,
      ScopedVector<autofill::PasswordForm> federated_credentials,
      const GURL& origin);

  content::WebContents* SetupTabWithTestController(Browser* browser);

  TestManagePasswordsUIController* controller() const { return controller_; }

  ChromePasswordManagerClient* client() const {
    return ChromePasswordManagerClient::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  MOCK_METHOD1(OnChooseCredential,
               void(const password_manager::CredentialInfo&));

 private:
  TestManagePasswordsUIController* controller_;
};

void PasswordDialogViewTest::SetUpOnMainThread() {
  SetupTabWithTestController(browser());
}

void PasswordDialogViewTest::SetupChooseCredentials(
    ScopedVector<autofill::PasswordForm> local_credentials,
    ScopedVector<autofill::PasswordForm> federated_credentials,
    const GURL& origin) {
  client()->PromptUserToChooseCredentials(
      std::move(local_credentials), std::move(federated_credentials), origin,
      base::Bind(&PasswordDialogViewTest::OnChooseCredential, this));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            controller()->GetState());
}

content::WebContents* PasswordDialogViewTest::SetupTabWithTestController(
    Browser* browser) {
  // Open a new tab with modified ManagePasswordsUIController.
  content::WebContents* tab =
      browser->tab_strip_model()->GetActiveWebContents();
  content::WebContents* new_tab = content::WebContents::Create(
      content::WebContents::CreateParams(tab->GetBrowserContext()));
  EXPECT_TRUE(new_tab);

  // ManagePasswordsUIController needs ChromePasswordManagerClient for logging.
  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      new_tab, nullptr);
  EXPECT_TRUE(ChromePasswordManagerClient::FromWebContents(new_tab));
  controller_ = new TestManagePasswordsUIController(new_tab);
  browser->tab_strip_model()->AppendWebContents(new_tab, true);

  // Navigate to a Web URL.
  EXPECT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browser, GURL("http://www.google.com")));
  EXPECT_EQ(controller_, ManagePasswordsUIController::FromWebContents(new_tab));
  return new_tab;
}

IN_PROC_BROWSER_TEST_F(PasswordDialogViewTest,
                       PopupAccountChooserWithMultipleCredentialsReturnEmpty) {
  GURL origin("https://example.com");
  ScopedVector<autofill::PasswordForm> local_credentials;
  autofill::PasswordForm form;
  form.origin = origin;
  form.display_name = base::ASCIIToUTF16("Peter");
  form.username_value = base::ASCIIToUTF16("peter@pan.test");
  form.icon_url = GURL("broken url");
  local_credentials.push_back(new autofill::PasswordForm(form));
  GURL icon_url("https://google.com/icon.png");
  form.icon_url = icon_url;
  form.display_name = base::ASCIIToUTF16("Peter Pen");
  form.federation_url = GURL("https://google.com/federation");
  local_credentials.push_back(new autofill::PasswordForm(form));

  // Prepare to capture the network request.
  TestURLFetcherCallback url_callback;
  net::FakeURLFetcherFactory factory(
      NULL, base::Bind(&TestURLFetcherCallback::CreateURLFetcher,
                       base::Unretained(&url_callback)));
  factory.SetFakeResponse(icon_url, std::string(), net::HTTP_OK,
                          net::URLRequestStatus::FAILED);
  EXPECT_CALL(url_callback, OnRequestDone(icon_url));

  SetupChooseCredentials(std::move(local_credentials),
                         ScopedVector<autofill::PasswordForm>(), origin);
  ASSERT_TRUE(controller()->current_account_chooser());
  AccountChooserDialogView* dialog = controller()->current_account_chooser();
  EXPECT_CALL(*this,
              OnChooseCredential(Field(
                  &password_manager::CredentialInfo::type,
                  password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY)));
  EXPECT_CALL(*controller(), OnDialogClosed());
  EXPECT_TRUE(dialog->Close());

  EXPECT_FALSE(controller()->current_autosignin_prompt());
}

IN_PROC_BROWSER_TEST_F(
    PasswordDialogViewTest,
    PopupAccountChooserWithMultipleCredentialsReturnNonEmpty) {
  GURL origin("https://example.com");
  ScopedVector<autofill::PasswordForm> local_credentials;
  autofill::PasswordForm form;
  form.origin = origin;
  form.display_name = base::ASCIIToUTF16("Peter");
  form.username_value = base::ASCIIToUTF16("peter@pan.test");
  form.icon_url = GURL("broken url");
  local_credentials.push_back(new autofill::PasswordForm(form));
  GURL icon_url("https://google.com/icon.png");
  form.icon_url = icon_url;
  form.display_name = base::ASCIIToUTF16("Peter Pen");
  form.federation_url = GURL("https://google.com/federation");
  local_credentials.push_back(new autofill::PasswordForm(form));

  SetupChooseCredentials(std::move(local_credentials),
                         ScopedVector<autofill::PasswordForm>(), origin);
  ASSERT_TRUE(controller()->current_account_chooser());

  // After picking a credential, we should pass it back to the caller via the
  // callback, but we should not pop up the autosignin prompt as there were
  // multiple credentials available.
  EXPECT_CALL(
      *this, OnChooseCredential(Field(
                 &password_manager::CredentialInfo::type,
                 password_manager::CredentialType::CREDENTIAL_TYPE_FEDERATED)));
  EXPECT_TRUE(
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          browser()->profile()->GetPrefs()));
  controller()->ChooseCredential(
      form, password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
  EXPECT_FALSE(controller()->current_autosignin_prompt());
}

IN_PROC_BROWSER_TEST_F(PasswordDialogViewTest,
                       PopupAccountChooserWithSingleCredentialReturnEmpty) {
  GURL origin("https://example.com");
  ScopedVector<autofill::PasswordForm> local_credentials;
  autofill::PasswordForm form;
  form.origin = origin;
  form.display_name = base::ASCIIToUTF16("Peter");
  form.username_value = base::ASCIIToUTF16("peter@pan.test");
  local_credentials.push_back(new autofill::PasswordForm(form));

  SetupChooseCredentials(std::move(local_credentials),
                         ScopedVector<autofill::PasswordForm>(), origin);

  EXPECT_TRUE(controller()->current_account_chooser());
  AccountChooserDialogView* dialog = controller()->current_account_chooser();
  EXPECT_CALL(*this,
              OnChooseCredential(Field(
                  &password_manager::CredentialInfo::type,
                  password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY)));
  EXPECT_CALL(*controller(), OnDialogClosed());
  EXPECT_TRUE(dialog->Close());
  EXPECT_FALSE(controller()->current_autosignin_prompt());
}

IN_PROC_BROWSER_TEST_F(PasswordDialogViewTest,
                       PopupAccountChooserWithSingleCredentialReturnNonEmpty) {
  GURL origin("https://example.com");
  ScopedVector<autofill::PasswordForm> local_credentials;
  autofill::PasswordForm form;
  form.origin = origin;
  form.display_name = base::ASCIIToUTF16("Peter");
  form.username_value = base::ASCIIToUTF16("peter@pan.test");
  local_credentials.push_back(new autofill::PasswordForm(form));

  SetupChooseCredentials(std::move(local_credentials),
                         ScopedVector<autofill::PasswordForm>(), origin);

  EXPECT_TRUE(controller()->current_account_chooser());

  // After picking a credential, we should pass it back to the caller via the
  // callback, and pop up the autosignin prompt iff we should show it.
  EXPECT_CALL(*this,
              OnChooseCredential(Field(
                  &password_manager::CredentialInfo::type,
                  password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD)));
  EXPECT_TRUE(
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          browser()->profile()->GetPrefs()));
  controller()->ChooseCredential(
      form, password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);

  EXPECT_CALL(*controller(), OnDialogClosed());
  EXPECT_TRUE(controller()->current_autosignin_prompt());
}

IN_PROC_BROWSER_TEST_F(PasswordDialogViewTest,
                       PopupAccountChooserWithDisabledAutoSignin) {
  EXPECT_TRUE(
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          browser()->profile()->GetPrefs()));
  GURL origin("https://example.com");
  ScopedVector<autofill::PasswordForm> local_credentials;
  autofill::PasswordForm form;
  form.origin = origin;
  form.display_name = base::ASCIIToUTF16("Peter");
  form.username_value = base::ASCIIToUTF16("peter@pan.test");
  local_credentials.push_back(new autofill::PasswordForm(form));

  SetupChooseCredentials(std::move(local_credentials),
                         ScopedVector<autofill::PasswordForm>(), origin);

  EXPECT_TRUE(controller()->current_account_chooser());

  // After picking a credential, we should pass it back to the caller via the
  // callback, and pop up the autosignin prompt iff we should show it.
  EXPECT_CALL(*this,
              OnChooseCredential(Field(
                  &password_manager::CredentialInfo::type,
                  password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD)));
  browser()->profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, false);
  controller()->ChooseCredential(
      form, password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);

  // The first run experience isn't shown because the setting is off.
  EXPECT_FALSE(controller()->current_autosignin_prompt());
}

IN_PROC_BROWSER_TEST_F(PasswordDialogViewTest,
                       PopupAccountChooserInIncognito) {
  EXPECT_TRUE(
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          browser()->profile()->GetPrefs()));
  EXPECT_TRUE(
      browser()->profile()->GetPrefs()->GetBoolean(
          password_manager::prefs::kCredentialsEnableAutosignin));
  GURL origin("https://example.com");
  ScopedVector<autofill::PasswordForm> local_credentials;
  autofill::PasswordForm form;
  form.origin = origin;
  form.display_name = base::ASCIIToUTF16("Peter");
  form.username_value = base::ASCIIToUTF16("peter@pan.test");
  local_credentials.push_back(new autofill::PasswordForm(form));

  Browser* incognito = CreateIncognitoBrowser();
  content::WebContents* tab = SetupTabWithTestController(incognito);
  ChromePasswordManagerClient* client =
      ChromePasswordManagerClient::FromWebContents(tab);
  client->PromptUserToChooseCredentials(
      std::move(local_credentials), ScopedVector<autofill::PasswordForm>(),
      origin,
      base::Bind(&PasswordDialogViewTest::OnChooseCredential, this));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            controller()->GetState());
  EXPECT_TRUE(controller()->current_account_chooser());

  EXPECT_CALL(*this,
              OnChooseCredential(Field(
                  &password_manager::CredentialInfo::type,
                  password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD)));
  controller()->ChooseCredential(
      form, password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);

  // The first run experience isn't shown because of Incognito.
  EXPECT_FALSE(controller()->current_autosignin_prompt());
}

IN_PROC_BROWSER_TEST_F(PasswordDialogViewTest, PopupAutoSigninPrompt) {
  EXPECT_TRUE(
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          browser()->profile()->GetPrefs()));
  controller()->OnPromptEnableAutoSignin();
  ASSERT_TRUE(controller()->current_autosignin_prompt());
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->GetState());
  AutoSigninFirstRunDialogView* dialog =
      controller()->current_autosignin_prompt();
  // This is the way how ESC is processed. It's important to reproduce it
  // because of double AutoSigninFirstRunDialogView::OnClosed call due to a bug
  // http://crbug.com/583330.
  ui::Accelerator esc(ui::VKEY_ESCAPE, 0);
  EXPECT_CALL(*controller(), OnDialogClosed());
  EXPECT_TRUE(dialog->GetWidget()->client_view()->AcceleratorPressed(esc));
  testing::Mock::VerifyAndClearExpectations(controller());
  EXPECT_TRUE(
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          browser()->profile()->GetPrefs()));
}

IN_PROC_BROWSER_TEST_F(PasswordDialogViewTest,
                       PopupAutoSigninPromptAfterBlockedZeroclick) {
  EXPECT_TRUE(
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          browser()->profile()->GetPrefs()));

  GURL origin("https://example.com");
  autofill::PasswordForm form;
  form.origin = origin;
  form.username_value = base::ASCIIToUTF16("peter@pan.test");
  form.password_value = base::ASCIIToUTF16("I can fly!");

  // Successful login alone will not prompt:
  client()->NotifySuccessfulLoginWithExistingPassword(form);
  ASSERT_FALSE(controller()->current_autosignin_prompt());

  // Blocked automatic sign-in will not prompt:
  scoped_ptr<autofill::PasswordForm> blocked_form(
      new autofill::PasswordForm(form));
  client()->NotifyUserAutoSigninBlockedOnFirstRun(std::move(blocked_form));
  ASSERT_FALSE(controller()->current_autosignin_prompt());

  // Successful login with a distinct form after block will not prompt:
  blocked_form.reset(new autofill::PasswordForm(form));
  client()->NotifyUserAutoSigninBlockedOnFirstRun(std::move(blocked_form));
  form.username_value = base::ASCIIToUTF16("notpeter@pan.test");
  client()->NotifySuccessfulLoginWithExistingPassword(form);
  ASSERT_FALSE(controller()->current_autosignin_prompt());

  // Successful login with the same form after block will not prompt if auto
  // sign-in is off:
  browser()->profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, false);
  blocked_form.reset(new autofill::PasswordForm(form));
  client()->NotifyUserAutoSigninBlockedOnFirstRun(std::move(blocked_form));
  client()->NotifySuccessfulLoginWithExistingPassword(form);
  ASSERT_FALSE(controller()->current_autosignin_prompt());
  browser()->profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, true);

  // Successful login with the same form after block will prompt:
  blocked_form.reset(new autofill::PasswordForm(form));
  client()->NotifyUserAutoSigninBlockedOnFirstRun(std::move(blocked_form));
  client()->NotifySuccessfulLoginWithExistingPassword(form);
  EXPECT_CALL(*controller(), OnDialogClosed());
  ASSERT_TRUE(controller()->current_autosignin_prompt());
}

}  // namespace
