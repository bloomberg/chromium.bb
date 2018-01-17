// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/passwords/account_chooser_dialog_view.h"
#include "chrome/browser/ui/views/passwords/auto_signin_first_run_dialog_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

using ::testing::Field;
using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace {

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
  web_contents->SetUserData(UserDataKey(), base::WrapUnique(this));
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

class PasswordDialogViewTest : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void SetUpOnMainThread() override;
  void ShowUi(const std::string& name) override;

  void SetupChooseCredentials(
      std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials,
      const GURL& origin);

  content::WebContents* SetupTabWithTestController(Browser* browser);

  TestManagePasswordsUIController* controller() const { return controller_; }

  ChromePasswordManagerClient* client() const {
    return ChromePasswordManagerClient::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  MOCK_METHOD1(OnChooseCredential, void(const autofill::PasswordForm*));
  MOCK_METHOD0(OnIconRequestDone, void());

  // Called on the server background thread.
  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    std::unique_ptr<BasicHttpResponse> response(new BasicHttpResponse);
    if (request.relative_url == "/icon.png") {
      OnIconRequestDone();
    }
    return std::move(response);
  }

 private:
  TestManagePasswordsUIController* controller_;
};

void PasswordDialogViewTest::SetUpOnMainThread() {
  SetupTabWithTestController(browser());
}

void PasswordDialogViewTest::SetupChooseCredentials(
    std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials,
    const GURL& origin) {
  client()->PromptUserToChooseCredentials(
      std::move(local_credentials), origin,
      base::Bind(&PasswordDialogViewTest::OnChooseCredential,
                 base::Unretained(this)));
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
  // Set up the test server to handle the form icon request.
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &PasswordDialogViewTest::HandleRequest, base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  GURL origin("https://example.com");
  std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials;
  autofill::PasswordForm form;
  form.origin = origin;
  form.display_name = base::ASCIIToUTF16("Peter");
  form.username_value = base::ASCIIToUTF16("peter@pan.test");
  form.icon_url = GURL("broken url");
  local_credentials.push_back(std::make_unique<autofill::PasswordForm>(form));
  form.icon_url = embedded_test_server()->GetURL("/icon.png");
  form.display_name = base::ASCIIToUTF16("Peter Pan");
  form.federation_origin =
      url::Origin::Create(GURL("https://google.com/federation"));
  local_credentials.push_back(std::make_unique<autofill::PasswordForm>(form));

  // Prepare to capture the network request.
  EXPECT_CALL(*this, OnIconRequestDone());
  embedded_test_server()->StartAcceptingConnections();

  SetupChooseCredentials(std::move(local_credentials), origin);
  ASSERT_TRUE(controller()->current_account_chooser());
  AccountChooserDialogView* dialog = controller()->current_account_chooser();
  EXPECT_CALL(*this, OnChooseCredential(nullptr));
  EXPECT_CALL(*controller(), OnDialogClosed());
  dialog->GetWidget()->Close();

  EXPECT_FALSE(controller()->current_autosignin_prompt());
}

IN_PROC_BROWSER_TEST_F(
    PasswordDialogViewTest,
    PopupAccountChooserWithMultipleCredentialsReturnNonEmpty) {
  GURL origin("https://example.com");
  std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials;
  autofill::PasswordForm form;
  form.origin = origin;
  form.display_name = base::ASCIIToUTF16("Peter");
  form.username_value = base::ASCIIToUTF16("peter@pan.test");
  form.icon_url = GURL("broken url");
  local_credentials.push_back(std::make_unique<autofill::PasswordForm>(form));
  GURL icon_url("https://google.com/icon.png");
  form.icon_url = icon_url;
  form.display_name = base::ASCIIToUTF16("Peter Pan");
  form.federation_origin =
      url::Origin::Create(GURL("https://google.com/federation"));
  local_credentials.push_back(std::make_unique<autofill::PasswordForm>(form));

  SetupChooseCredentials(std::move(local_credentials), origin);
  ASSERT_TRUE(controller()->current_account_chooser());

  // After picking a credential, we should pass it back to the caller via the
  // callback, but we should not pop up the autosignin prompt as there were
  // multiple credentials available.
  EXPECT_CALL(*this, OnChooseCredential(testing::Pointee(form)));
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
  std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials;
  autofill::PasswordForm form;
  form.origin = origin;
  form.display_name = base::ASCIIToUTF16("Peter");
  form.username_value = base::ASCIIToUTF16("peter@pan.test");
  local_credentials.push_back(std::make_unique<autofill::PasswordForm>(form));

  SetupChooseCredentials(std::move(local_credentials), origin);

  EXPECT_TRUE(controller()->current_account_chooser());
  AccountChooserDialogView* dialog = controller()->current_account_chooser();
  EXPECT_CALL(*this, OnChooseCredential(nullptr));
  EXPECT_CALL(*controller(), OnDialogClosed());
  dialog->GetWidget()->Close();
  EXPECT_FALSE(controller()->current_autosignin_prompt());
}

IN_PROC_BROWSER_TEST_F(PasswordDialogViewTest,
                       PopupAccountChooserWithSingleCredentialClickSignIn) {
  GURL origin("https://example.com");
  std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials;
  autofill::PasswordForm form;
  form.origin = origin;
  form.display_name = base::ASCIIToUTF16("Peter");
  form.username_value = base::ASCIIToUTF16("peter@pan.test");
  local_credentials.push_back(std::make_unique<autofill::PasswordForm>(form));

  SetupChooseCredentials(std::move(local_credentials), origin);

  EXPECT_TRUE(controller()->current_account_chooser());
  views::DialogDelegateView* dialog = controller()->current_account_chooser();
  views::test::WidgetClosingObserver bubble_observer(dialog->GetWidget());
  EXPECT_CALL(*this, OnChooseCredential(testing::Pointee(form)));
  dialog->Accept();
  EXPECT_TRUE(bubble_observer.widget_closed());
}

IN_PROC_BROWSER_TEST_F(PasswordDialogViewTest,
                       PopupAccountChooserWithSingleCredentialReturnNonEmpty) {
  GURL origin("https://example.com");
  std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials;
  autofill::PasswordForm form;
  form.origin = origin;
  form.display_name = base::ASCIIToUTF16("Peter");
  form.username_value = base::ASCIIToUTF16("peter@pan.test");
  local_credentials.push_back(std::make_unique<autofill::PasswordForm>(form));

  SetupChooseCredentials(std::move(local_credentials), origin);

  EXPECT_TRUE(controller()->current_account_chooser());

  // After picking a credential, we should pass it back to the caller via the
  // callback, and pop up the autosignin prompt iff we should show it.
  EXPECT_CALL(*this, OnChooseCredential(testing::Pointee(form)));
  EXPECT_TRUE(
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          browser()->profile()->GetPrefs()));
  controller()->ChooseCredential(
      form, password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);

  EXPECT_TRUE(controller()->current_autosignin_prompt());
}

IN_PROC_BROWSER_TEST_F(PasswordDialogViewTest,
                       PopupAccountChooserWithDisabledAutoSignin) {
  EXPECT_TRUE(
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          browser()->profile()->GetPrefs()));
  GURL origin("https://example.com");
  std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials;
  autofill::PasswordForm form;
  form.origin = origin;
  form.display_name = base::ASCIIToUTF16("Peter");
  form.username_value = base::ASCIIToUTF16("peter@pan.test");
  local_credentials.push_back(std::make_unique<autofill::PasswordForm>(form));

  SetupChooseCredentials(std::move(local_credentials), origin);

  EXPECT_TRUE(controller()->current_account_chooser());

  // After picking a credential, we should pass it back to the caller via the
  // callback, and pop up the autosignin prompt iff we should show it.
  EXPECT_CALL(*this, OnChooseCredential(testing::Pointee(form)));
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
  std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials;
  autofill::PasswordForm form;
  form.origin = origin;
  form.display_name = base::ASCIIToUTF16("Peter");
  form.username_value = base::ASCIIToUTF16("peter@pan.test");
  local_credentials.push_back(std::make_unique<autofill::PasswordForm>(form));

  Browser* incognito = CreateIncognitoBrowser();
  content::WebContents* tab = SetupTabWithTestController(incognito);
  ChromePasswordManagerClient* client =
      ChromePasswordManagerClient::FromWebContents(tab);
  client->PromptUserToChooseCredentials(
      std::move(local_credentials), origin,
      base::Bind(&PasswordDialogViewTest::OnChooseCredential,
                 base::Unretained(this)));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            controller()->GetState());
  EXPECT_TRUE(controller()->current_account_chooser());

  EXPECT_CALL(*this, OnChooseCredential(testing::Pointee(form)));
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
  views::test::WidgetClosingObserver bubble_observer(dialog->GetWidget());
  ui::Accelerator esc(ui::VKEY_ESCAPE, 0);
  EXPECT_CALL(*controller(), OnDialogClosed());
  EXPECT_TRUE(dialog->GetWidget()->client_view()->AcceleratorPressed(esc));
  EXPECT_TRUE(bubble_observer.widget_closed());
  content::RunAllPendingInMessageLoop();
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
  std::unique_ptr<autofill::PasswordForm> blocked_form(
      new autofill::PasswordForm(form));
  client()->NotifyUserCouldBeAutoSignedIn(std::move(blocked_form));
  ASSERT_FALSE(controller()->current_autosignin_prompt());

  // Successful login with a distinct form after block will not prompt:
  blocked_form.reset(new autofill::PasswordForm(form));
  client()->NotifyUserCouldBeAutoSignedIn(std::move(blocked_form));
  form.username_value = base::ASCIIToUTF16("notpeter@pan.test");
  client()->NotifySuccessfulLoginWithExistingPassword(form);
  ASSERT_FALSE(controller()->current_autosignin_prompt());

  // Successful login with the same form after block will not prompt if auto
  // sign-in is off:
  browser()->profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, false);
  blocked_form.reset(new autofill::PasswordForm(form));
  client()->NotifyUserCouldBeAutoSignedIn(std::move(blocked_form));
  client()->NotifySuccessfulLoginWithExistingPassword(form);
  ASSERT_FALSE(controller()->current_autosignin_prompt());
  browser()->profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, true);

  // Successful login with the same form after block will prompt:
  blocked_form.reset(new autofill::PasswordForm(form));
  client()->NotifyUserCouldBeAutoSignedIn(std::move(blocked_form));
  client()->NotifySuccessfulLoginWithExistingPassword(form);
  ASSERT_TRUE(controller()->current_autosignin_prompt());
}

// DialogBrowserTest methods for interactive dialog invocation.
void PasswordDialogViewTest::ShowUi(const std::string& name) {
  if (name == "AutoSigninFirstRun") {
    controller()->OnPromptEnableAutoSignin();
    return;
  }

  GURL origin("https://example.com");
  std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials;
  autofill::PasswordForm form;
  form.origin = origin;
  form.display_name = base::ASCIIToUTF16("Peter Pan");
  form.username_value = base::ASCIIToUTF16("peter@pan.test");
  if (name == "PopupAutoSigninPrompt") {
    form.icon_url = GURL("broken url");
    local_credentials.push_back(std::make_unique<autofill::PasswordForm>(form));
    form.icon_url = GURL("https://google.com/icon.png");
    form.display_name = base::ASCIIToUTF16("Peter");
    form.federation_origin =
        url::Origin::Create(GURL("https://google.com/federation"));
    local_credentials.push_back(std::make_unique<autofill::PasswordForm>(form));
    controller()->OnAutoSignin(std::move(local_credentials), origin);
    EXPECT_EQ(password_manager::ui::AUTO_SIGNIN_STATE,
              controller()->GetState());
  } else if (base::StartsWith(name, "PopupAccountChooserWith",
                              base::CompareCase::SENSITIVE)) {
    local_credentials.push_back(std::make_unique<autofill::PasswordForm>(form));
    if (name == "PopupAccountChooserWithMultipleCredentialClickSignIn") {
      form.icon_url = GURL("https://google.com/icon.png");
      form.display_name = base::ASCIIToUTF16("Tinkerbell");
      form.username_value = base::ASCIIToUTF16("tinkerbell@pan.test");
      form.federation_origin =
          url::Origin::Create(GURL("https://google.com/neverland"));
      local_credentials.push_back(
          std::make_unique<autofill::PasswordForm>(form));
      form.display_name = base::ASCIIToUTF16("James Hook");
      form.username_value = base::ASCIIToUTF16("james@pan.test");
      form.federation_origin =
          url::Origin::Create(GURL("https://google.com/jollyroger"));
      local_credentials.push_back(
          std::make_unique<autofill::PasswordForm>(form));
      form.display_name = base::ASCIIToUTF16("Wendy Darling");
      form.username_value = base::ASCIIToUTF16("wendy@pan.test");
      form.federation_origin =
          url::Origin::Create(GURL("https://google.com/london"));
      local_credentials.push_back(
          std::make_unique<autofill::PasswordForm>(form));
    }
    SetupChooseCredentials(std::move(local_credentials), origin);
  } else {
    ADD_FAILURE() << "Unknown dialog type";
    return;
  }
}

IN_PROC_BROWSER_TEST_F(PasswordDialogViewTest, InvokeUi_PopupAutoSigninPrompt) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(
    PasswordDialogViewTest,
    InvokeUi_PopupAccountChooserWithSingleCredentialClickSignIn) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(
    PasswordDialogViewTest,
    InvokeUi_PopupAccountChooserWithMultipleCredentialClickSignIn) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PasswordDialogViewTest, InvokeUi_AutoSigninFirstRun) {
  ShowAndVerifyUi();
}

}  // namespace
