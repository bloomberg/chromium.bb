// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_test_base.h"

#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test_utils.h"
#include "net/cert/cert_verify_result.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/transport_security_state.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

// A helper class that synchronously waits until the password store handles a
// GetLogins() request.
class PasswordStoreResultsObserver
    : public password_manager::PasswordStoreConsumer {
 public:
  PasswordStoreResultsObserver() = default;

  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override {
    run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreResultsObserver);
};

// ManagePasswordsUIController subclass to capture the UI events.
class CustomManagePasswordsUIController : public ManagePasswordsUIController {
 public:
  explicit CustomManagePasswordsUIController(
      content::WebContents* web_contents);

  void WaitForState(password_manager::ui::State target_state);

 private:
  // PasswordsClientUIDelegate:
  void OnPasswordSubmitted(
      std::unique_ptr<password_manager::PasswordFormManager> form_manager)
      override;
  bool OnChooseCredentials(
      std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials,
      const GURL& origin,
      const ManagePasswordsState::CredentialsCallback& callback) override;
  void OnPasswordAutofilled(
      const std::map<base::string16, const autofill::PasswordForm*>&
          password_form_map,
      const GURL& origin,
      const std::vector<const autofill::PasswordForm*>* federated_matches)
      override;

  // The loop to be stopped when the target state is observed.
  base::RunLoop* run_loop_;

  // The state CustomManagePasswordsUIController is currently waiting for.
  password_manager::ui::State target_state_;

  DISALLOW_COPY_AND_ASSIGN(CustomManagePasswordsUIController);
};

CustomManagePasswordsUIController::CustomManagePasswordsUIController(
    content::WebContents* web_contents)
    : ManagePasswordsUIController(web_contents),
      run_loop_(nullptr),
      target_state_(password_manager::ui::INACTIVE_STATE) {
  // Attach CustomManagePasswordsUIController to |web_contents| so the default
  // ManagePasswordsUIController isn't created.
  // Do not silently replace an existing ManagePasswordsUIController because it
  // unregisters itself in WebContentsDestroyed().
  EXPECT_FALSE(web_contents->GetUserData(UserDataKey()));
  web_contents->SetUserData(UserDataKey(), base::WrapUnique(this));
}

void CustomManagePasswordsUIController::WaitForState(
    password_manager::ui::State target_state) {
  base::RunLoop run_loop;
  target_state_ = target_state;
  run_loop_ = &run_loop;
  run_loop_->Run();
}

void CustomManagePasswordsUIController::OnPasswordSubmitted(
    std::unique_ptr<password_manager::PasswordFormManager> form_manager) {
  if (target_state_ == password_manager::ui::PENDING_PASSWORD_STATE) {
    run_loop_->Quit();
    run_loop_ = nullptr;
    target_state_ = password_manager::ui::INACTIVE_STATE;
  }
  return ManagePasswordsUIController::OnPasswordSubmitted(
      std::move(form_manager));
}

bool CustomManagePasswordsUIController::OnChooseCredentials(
    std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials,
    const GURL& origin,
    const ManagePasswordsState::CredentialsCallback& callback) {
  if (target_state_ == password_manager::ui::CREDENTIAL_REQUEST_STATE) {
    run_loop_->Quit();
    run_loop_ = nullptr;
    target_state_ = password_manager::ui::INACTIVE_STATE;
  }
  return ManagePasswordsUIController::OnChooseCredentials(
      std::move(local_credentials), origin, callback);
}

void CustomManagePasswordsUIController::OnPasswordAutofilled(
    const std::map<base::string16, const autofill::PasswordForm*>&
        password_form_map,
    const GURL& origin,
    const std::vector<const autofill::PasswordForm*>* federated_matches) {
  if (target_state_ == password_manager::ui::MANAGE_STATE) {
    run_loop_->Quit();
    run_loop_ = nullptr;
    target_state_ = password_manager::ui::INACTIVE_STATE;
  }
  return ManagePasswordsUIController::OnPasswordAutofilled(
      password_form_map, origin, federated_matches);
}

void AddHSTSHostImpl(
    const scoped_refptr<net::URLRequestContextGetter>& request_context,
    const std::string& host) {
  ASSERT_TRUE(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  net::TransportSecurityState* transport_security_state =
      request_context->GetURLRequestContext()->transport_security_state();
  if (!transport_security_state) {
    ADD_FAILURE();
    return;
  }

  base::Time expiry = base::Time::Now() + base::TimeDelta::FromDays(1000);
  bool include_subdomains = false;
  transport_security_state->AddHSTS(host, expiry, include_subdomains);
  EXPECT_TRUE(transport_security_state->ShouldUpgradeToSSL(host));
}

}  // namespace

NavigationObserver::NavigationObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      quit_on_entry_committed_(false) {}
NavigationObserver::~NavigationObserver() {
}

void NavigationObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted())
    return;

  if (quit_on_entry_committed_)
    run_loop_.Quit();
}

void NavigationObserver::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  render_frame_host_ = render_frame_host;
  if (!wait_for_path_.empty()) {
    if (validated_url.path() == wait_for_path_)
      run_loop_.Quit();
  } else if (!render_frame_host->GetParent()) {
    run_loop_.Quit();
  }
}

void NavigationObserver::Wait() {
  run_loop_.Run();
}

BubbleObserver::BubbleObserver(content::WebContents* web_contents)
    : passwords_ui_controller_(
          ManagePasswordsUIController::FromWebContents(web_contents)) {}

bool BubbleObserver::IsShowingSavePrompt() const {
  return passwords_ui_controller_->GetState() ==
      password_manager::ui::PENDING_PASSWORD_STATE;
}

bool BubbleObserver::IsShowingUpdatePrompt() const {
  return passwords_ui_controller_->GetState() ==
      password_manager::ui::PENDING_PASSWORD_UPDATE_STATE;
}

void BubbleObserver::Dismiss() const  {
  passwords_ui_controller_->OnBubbleHidden();
  ASSERT_EQ(password_manager::ui::INACTIVE_STATE,
            passwords_ui_controller_->GetState());
}

void BubbleObserver::AcceptSavePrompt() const {
  ASSERT_TRUE(IsShowingSavePrompt());
  passwords_ui_controller_->SavePassword();
  EXPECT_FALSE(IsShowingSavePrompt());
}

void BubbleObserver::AcceptUpdatePrompt(
    const autofill::PasswordForm& form) const {
  ASSERT_TRUE(IsShowingUpdatePrompt());
  passwords_ui_controller_->UpdatePassword(form);
  EXPECT_FALSE(IsShowingUpdatePrompt());
}

void BubbleObserver::WaitForAccountChooser() const {
  if (passwords_ui_controller_->GetState() ==
      password_manager::ui::CREDENTIAL_REQUEST_STATE)
    return;
  CustomManagePasswordsUIController* controller =
      static_cast<CustomManagePasswordsUIController*>(passwords_ui_controller_);
  controller->WaitForState(password_manager::ui::CREDENTIAL_REQUEST_STATE);
}

void BubbleObserver::WaitForManagementState() const {
  if (passwords_ui_controller_->GetState() ==
      password_manager::ui::MANAGE_STATE)
    return;
  CustomManagePasswordsUIController* controller =
      static_cast<CustomManagePasswordsUIController*>(passwords_ui_controller_);
  controller->WaitForState(password_manager::ui::MANAGE_STATE);
}

void BubbleObserver::WaitForSavePrompt() const {
  if (passwords_ui_controller_->GetState() ==
      password_manager::ui::PENDING_PASSWORD_STATE)
    return;
  CustomManagePasswordsUIController* controller =
      static_cast<CustomManagePasswordsUIController*>(passwords_ui_controller_);
  controller->WaitForState(password_manager::ui::PENDING_PASSWORD_STATE);
}

PasswordManagerBrowserTestBase::PasswordManagerBrowserTestBase()
    : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS),
      web_contents_(nullptr) {}

PasswordManagerBrowserTestBase::~PasswordManagerBrowserTestBase() = default;

void PasswordManagerBrowserTestBase::SetUpOnMainThread() {
  // Use TestPasswordStore to remove a possible race. Normally the
  // PasswordStore does its database manipulation on the DB thread, which
  // creates a possible race during navigation. Specifically the
  // PasswordManager will ignore any forms in a page if the load from the
  // PasswordStore has not completed.
  PasswordStoreFactory::GetInstance()->SetTestingFactory(
      browser()->profile(),
      password_manager::BuildPasswordStore<
          content::BrowserContext, password_manager::TestPasswordStore>);
  ASSERT_TRUE(embedded_test_server()->Start());

  // Setup HTTPS server serving files from standard test directory.
  static constexpr base::FilePath::CharType kDocRoot[] =
      FILE_PATH_LITERAL("chrome/test/data");
  https_test_server().ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));
  ASSERT_TRUE(https_test_server().Start());

  // Setup the mock host resolver
  host_resolver()->AddRule("*", "127.0.0.1");

  // Whitelist all certs for the HTTPS server.
  auto cert = https_test_server().GetCertificate();
  net::CertVerifyResult verify_result;
  verify_result.cert_status = 0;
  verify_result.is_issued_by_known_root = true;
  verify_result.verified_cert = cert;
  mock_cert_verifier().AddResultForCert(cert.get(), verify_result, net::OK);

  // Add a tab with a customized ManagePasswordsUIController. Thus, we can
  // intercept useful UI events.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(tab->GetBrowserContext()));
  ASSERT_TRUE(web_contents_);

  // ManagePasswordsUIController needs ChromePasswordManagerClient for logging.
  autofill::ChromeAutofillClient::CreateForWebContents(web_contents_);
  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      web_contents_,
      autofill::ChromeAutofillClient::FromWebContents(web_contents_));
  ASSERT_TRUE(ChromePasswordManagerClient::FromWebContents(web_contents_));
  CustomManagePasswordsUIController* controller =
      new CustomManagePasswordsUIController(web_contents_);
  browser()->tab_strip_model()->AppendWebContents(web_contents_, true);
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabStripModel::CLOSE_NONE);
  ASSERT_EQ(controller,
            ManagePasswordsUIController::FromWebContents(web_contents_));
  ASSERT_EQ(web_contents_,
            browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_FALSE(web_contents_->IsLoading());
}

void PasswordManagerBrowserTestBase::TearDownOnMainThread() {
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
}

void PasswordManagerBrowserTestBase::SetUpInProcessBrowserTestFixture() {
  ProfileIOData::SetCertVerifierForTesting(&mock_cert_verifier_);
}

void PasswordManagerBrowserTestBase::TearDownInProcessBrowserTestFixture() {
  ProfileIOData::SetCertVerifierForTesting(nullptr);
}

content::WebContents* PasswordManagerBrowserTestBase::WebContents() {
  return web_contents_;
}

content::RenderViewHost* PasswordManagerBrowserTestBase::RenderViewHost() {
  return WebContents()->GetRenderViewHost();
}

content::RenderFrameHost* PasswordManagerBrowserTestBase::RenderFrameHost() {
  return WebContents()->GetMainFrame();
}

void PasswordManagerBrowserTestBase::NavigateToFile(const std::string& path) {
  ASSERT_EQ(web_contents_,
            browser()->tab_strip_model()->GetActiveWebContents());
  NavigationObserver observer(WebContents());
  GURL url = embedded_test_server()->GetURL(path);
  ui_test_utils::NavigateToURL(browser(), url);
  observer.Wait();
}

void PasswordManagerBrowserTestBase::VerifyPasswordIsSavedAndFilled(
    const std::string& filename,
    const std::string& submission_script,
    const std::string& expected_element,
    const std::string& expected_value) {
  password_manager::TestPasswordStore* password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS).get());
  EXPECT_TRUE(password_store->IsEmpty());

  NavigateToFile(filename);

  NavigationObserver observer(WebContents());
  ASSERT_TRUE(content::ExecuteScript(RenderFrameHost(), submission_script));
  observer.Wait();
  WaitForPasswordStore();

  BubbleObserver(WebContents()).AcceptSavePrompt();

  // Spin the message loop to make sure the password store had a chance to save
  // the password.
  WaitForPasswordStore();
  ASSERT_FALSE(password_store->IsEmpty());

  NavigateToFile(filename);

  // Let the user interact with the page, so that DOM gets modification events,
  // needed for autofilling fields.
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(1, 1));

  // Wait until that interaction causes the password value to be revealed.
  WaitForElementValue(expected_element, expected_value);
}

void PasswordManagerBrowserTestBase::WaitForElementValue(
    const std::string& element_id,
    const std::string& expected_value) {
  WaitForElementValue("null", element_id, expected_value);
}

void PasswordManagerBrowserTestBase::WaitForElementValue(
    const std::string& iframe_id,
    const std::string& element_id,
    const std::string& expected_value) {
  enum ReturnCodes {  // Possible results of the JavaScript code.
    RETURN_CODE_OK,
    RETURN_CODE_NO_ELEMENT,
    RETURN_CODE_WRONG_VALUE,
    RETURN_CODE_INVALID,
  };
  const std::string value_check_function = base::StringPrintf(
      "function valueCheck() {"
      "  if (%s)"
      "    var element = document.getElementById("
      "        '%s').contentDocument.getElementById('%s');"
      "  else "
      "    var element = document.getElementById('%s');"
      "  return element && element.value == '%s';"
      "}",
      iframe_id.c_str(), iframe_id.c_str(), element_id.c_str(),
      element_id.c_str(), expected_value.c_str());
  const std::string script =
      value_check_function +
      base::StringPrintf(
          "if (valueCheck()) {"
          "  /* Spin the event loop with setTimeout. */"
          "  setTimeout(window.domAutomationController.send(%d), 0);"
          "} else {"
          "  if (%s)"
          "    var element = document.getElementById("
          "        '%s').contentDocument.getElementById('%s');"
          "  else "
          "    var element = document.getElementById('%s');"
          "  if (!element)"
          "    window.domAutomationController.send(%d);"
          "  element.onchange = function() {"
          "    if (valueCheck()) {"
          "      /* Spin the event loop with setTimeout. */"
          "      setTimeout(window.domAutomationController.send(%d), 0);"
          "    } else {"
          "      window.domAutomationController.send(%d);"
          "    }"
          "  };"
          "}",
          RETURN_CODE_OK, iframe_id.c_str(), iframe_id.c_str(),
          element_id.c_str(), element_id.c_str(), RETURN_CODE_NO_ELEMENT,
          RETURN_CODE_OK, RETURN_CODE_WRONG_VALUE);
  int return_value = RETURN_CODE_INVALID;
  ASSERT_TRUE(content::ExecuteScriptWithoutUserGestureAndExtractInt(
      RenderFrameHost(), script, &return_value));
  EXPECT_EQ(RETURN_CODE_OK, return_value)
      << "element_id = " << element_id
      << ", expected_value = " << expected_value;
}

void PasswordManagerBrowserTestBase::WaitForPasswordStore() {
  scoped_refptr<password_manager::PasswordStore> password_store =
      PasswordStoreFactory::GetForProfile(browser()->profile(),
                                          ServiceAccessType::IMPLICIT_ACCESS);
  PasswordStoreResultsObserver syncer;
  password_store->GetAutofillableLoginsWithAffiliatedRealms(&syncer);
  syncer.Wait();
}

void PasswordManagerBrowserTestBase::CheckElementValue(
    const std::string& element_id,
    const std::string& expected_value) {
  CheckElementValue("null", element_id, expected_value);
}

void PasswordManagerBrowserTestBase::CheckElementValue(
    const std::string& iframe_id,
    const std::string& element_id,
    const std::string& expected_value) {
  const std::string value_get_script = base::StringPrintf(
      "if (%s)"
      "  var element = document.getElementById("
      "      '%s').contentDocument.getElementById('%s');"
      "else "
      "  var element = document.getElementById('%s');"
      "var value = element ? element.value : 'element not found';"
      "window.domAutomationController.send(value);",
      iframe_id.c_str(), iframe_id.c_str(), element_id.c_str(),
      element_id.c_str());
  std::string return_value;
  ASSERT_TRUE(content::ExecuteScriptWithoutUserGestureAndExtractString(
      RenderFrameHost(), value_get_script, &return_value));
  EXPECT_EQ(expected_value, return_value) << "element_id = " << element_id;
}

void PasswordManagerBrowserTestBase::AddHSTSHost(const std::string& host) {
  base::RunLoop run_loop;

  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          &AddHSTSHostImpl,
          make_scoped_refptr(browser()->profile()->GetRequestContext()), host),
      run_loop.QuitClosure());

  run_loop.Run();
}
