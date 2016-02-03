// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/passwords/account_chooser_dialog_view.h"
#include "chrome/browser/ui/views/passwords/auto_signin_first_run_dialog_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
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

  AccountChooserPrompt* current_account_chooser() const {
    return current_account_chooser_;
  }

  AutoSigninFirstRunPrompt* current_autosignin_prompt() const {
    return current_autosignin_prompt_;
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

  TestManagePasswordsUIController* controller() const { return controller_; }

  MOCK_METHOD1(OnChooseCredential,
               void(const password_manager::CredentialInfo&));

 private:
  TestManagePasswordsUIController* controller_;
};

void PasswordDialogViewTest::SetUpOnMainThread() {
  // Open a new tab with modified ManagePasswordsUIController.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WebContents* new_tab = content::WebContents::Create(
      content::WebContents::CreateParams(tab->GetBrowserContext()));
  ASSERT_TRUE(new_tab);

  // ManagePasswordsUIController needs ChromePasswordManagerClient for logging.
  // ChromePasswordManagerClient needs ChromeAutofillClient on creation.
  autofill::ChromeAutofillClient::CreateForWebContents(new_tab);
  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      new_tab,
      autofill::ChromeAutofillClient::FromWebContents(new_tab));
  ASSERT_TRUE(ChromePasswordManagerClient::FromWebContents(new_tab));
  controller_ = new TestManagePasswordsUIController(new_tab);
  browser()->tab_strip_model()->AppendWebContents(new_tab, true);

  // Navigate to a Web URL.
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browser(), GURL("http://www.google.com")));
  ASSERT_EQ(controller_, ManagePasswordsUIController::FromWebContents(new_tab));
}

void PasswordDialogViewTest::SetupChooseCredentials(
    ScopedVector<autofill::PasswordForm> local_credentials,
    ScopedVector<autofill::PasswordForm> federated_credentials,
    const GURL& origin) {
  controller()->OnChooseCredentials(
      std::move(local_credentials), std::move(federated_credentials), origin,
      base::Bind(&PasswordDialogViewTest::OnChooseCredential, this));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            controller()->GetState());
}

IN_PROC_BROWSER_TEST_F(PasswordDialogViewTest, PopupAccountChooser) {
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
      NULL,
      base::Bind(&TestURLFetcherCallback::CreateURLFetcher,
                 base::Unretained(&url_callback)));
  factory.SetFakeResponse(icon_url, std::string(), net::HTTP_OK,
                          net::URLRequestStatus::FAILED);
  EXPECT_CALL(url_callback, OnRequestDone(icon_url));

  SetupChooseCredentials(std::move(local_credentials),
                         ScopedVector<autofill::PasswordForm>(), origin);
  ASSERT_TRUE(controller()->current_account_chooser());
  AccountChooserDialogView* dialog = static_cast<AccountChooserDialogView*>(
      controller()->current_account_chooser());
  EXPECT_CALL(*this, OnChooseCredential(
      Field(&password_manager::CredentialInfo::type,
            password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY)));
  EXPECT_CALL(*controller(), OnDialogClosed());
  EXPECT_TRUE(dialog->Close());
}

IN_PROC_BROWSER_TEST_F(PasswordDialogViewTest, PopupAutoSigninPrompt) {
  EXPECT_TRUE(
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          browser()->profile()->GetPrefs()));
  controller()->OnPromptEnableAutoSignin();
  ASSERT_TRUE(controller()->current_autosignin_prompt());
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->GetState());
  AutoSigninFirstRunDialogView* dialog =
      static_cast<AutoSigninFirstRunDialogView*>(
          controller()->current_autosignin_prompt());
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

}  // namespace
