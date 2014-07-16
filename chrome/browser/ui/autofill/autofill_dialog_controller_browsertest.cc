// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/account_chooser_model.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_dialog_i18n_input.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view_tester.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/data_model_wrapper.h"
#include "chrome/browser/ui/autofill/mock_address_validator.h"
#include "chrome/browser/ui/autofill/test_generated_credit_card_bubble_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/risk/proto/fingerprint.pb.h"
#include "components/autofill/content/browser/wallet/gaia_account.h"
#include "components/autofill/content/browser/wallet/mock_wallet_client.h"
#include "components/autofill/content/browser/wallet/wallet_service_url.h"
#include "components/autofill/content/browser/wallet/wallet_test_util.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/referrer.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#elif defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "chrome/browser/ui/cocoa/run_loop_testing.h"
#endif

using base::ASCIIToUTF16;

namespace autofill {

namespace {

using testing::Return;
using testing::_;

void MockCallback(AutofillClient::RequestAutocompleteResult,
                  const base::string16& message,
                  const FormStructure*) {
}

class MockAutofillMetrics : public AutofillMetrics {
 public:
  MockAutofillMetrics()
      : dialog_dismissal_action_(static_cast<DialogDismissalAction>(-1)) {}
  virtual ~MockAutofillMetrics() {}

  virtual void LogDialogUiDuration(
      const base::TimeDelta& duration,
      DialogDismissalAction dismissal_action) const OVERRIDE {
    // Ignore constness for testing.
    MockAutofillMetrics* mutable_this = const_cast<MockAutofillMetrics*>(this);
    mutable_this->dialog_dismissal_action_ = dismissal_action;
  }

  AutofillMetrics::DialogDismissalAction dialog_dismissal_action() const {
    return dialog_dismissal_action_;
  }

  MOCK_CONST_METHOD1(LogDialogDismissalState,
                     void(DialogDismissalState state));

 private:
  DialogDismissalAction dialog_dismissal_action_;

  DISALLOW_COPY_AND_ASSIGN(MockAutofillMetrics);
};

class TestAutofillDialogController : public AutofillDialogControllerImpl {
 public:
  TestAutofillDialogController(
      content::WebContents* contents,
      const FormData& form_data,
      const AutofillMetrics& metric_logger,
      scoped_refptr<content::MessageLoopRunner> runner)
      : AutofillDialogControllerImpl(contents,
                                     form_data,
                                     form_data.origin,
                                     base::Bind(&MockCallback)),
        metric_logger_(metric_logger),
        mock_wallet_client_(
            Profile::FromBrowserContext(contents->GetBrowserContext())->
                GetRequestContext(), this, form_data.origin),
        message_loop_runner_(runner),
        use_validation_(false),
        weak_ptr_factory_(this),
        sign_in_user_index_(0U) {
    test_manager_.Init(
        NULL,
        Profile::FromBrowserContext(contents->GetBrowserContext())->GetPrefs(),
        false);
  }

  virtual ~TestAutofillDialogController() {}

  virtual GURL SignInUrl() const OVERRIDE {
    return GURL(chrome::kChromeUIVersionURL);
  }

  virtual void ViewClosed() OVERRIDE {
    message_loop_runner_->Quit();
    AutofillDialogControllerImpl::ViewClosed();
  }

  virtual base::string16 InputValidityMessage(
      DialogSection section,
      ServerFieldType type,
      const base::string16& value) OVERRIDE {
    if (!use_validation_)
      return base::string16();
    return AutofillDialogControllerImpl::InputValidityMessage(
        section, type, value);
  }

  virtual ValidityMessages InputsAreValid(
      DialogSection section,
      const FieldValueMap& inputs) OVERRIDE {
    if (!use_validation_)
      return ValidityMessages();
    return AutofillDialogControllerImpl::InputsAreValid(section, inputs);
  }

  // Saving to Chrome is tested in AutofillDialogControllerImpl unit tests.
  // TODO(estade): test that the view defaults to saving to Chrome.
  virtual bool ShouldOfferToSaveInChrome() const OVERRIDE {
    return true;
  }

  void ForceFinishSubmit() {
    DoFinishSubmit();
  }

  // Increase visibility for testing.
  using AutofillDialogControllerImpl::view;
  using AutofillDialogControllerImpl::popup_input_type;

  MOCK_METHOD0(LoadRiskFingerprintData, void());

  virtual std::vector<DialogNotification> CurrentNotifications() OVERRIDE {
    return notifications_;
  }

  void set_notifications(const std::vector<DialogNotification>& notifications) {
    notifications_ = notifications;
  }

  TestPersonalDataManager* GetTestingManager() {
    return &test_manager_;
  }

  MockAddressValidator* GetMockValidator() {
    return &mock_validator_;
  }

  using AutofillDialogControllerImpl::IsEditingExistingData;
  using AutofillDialogControllerImpl::IsManuallyEditingSection;
  using AutofillDialogControllerImpl::IsPayingWithWallet;
  using AutofillDialogControllerImpl::IsSubmitPausedOn;
  using AutofillDialogControllerImpl::OnDidLoadRiskFingerprintData;
  using AutofillDialogControllerImpl::AccountChooserModelForTesting;
  using AutofillDialogControllerImpl::
      ClearLastWalletItemsFetchTimestampForTesting;

  void set_use_validation(bool use_validation) {
    use_validation_ = use_validation;
  }

  base::WeakPtr<TestAutofillDialogController> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  wallet::MockWalletClient* GetTestingWalletClient() {
    return &mock_wallet_client_;
  }

  void set_sign_in_user_index(size_t sign_in_user_index) {
    sign_in_user_index_ = sign_in_user_index;
  }

 protected:
  virtual PersonalDataManager* GetManager() const OVERRIDE {
    return &const_cast<TestAutofillDialogController*>(this)->test_manager_;
  }

  virtual AddressValidator* GetValidator() OVERRIDE {
    return &mock_validator_;
  }

  virtual wallet::WalletClient* GetWalletClient() OVERRIDE {
    return &mock_wallet_client_;
  }

  virtual bool IsSignInContinueUrl(const GURL& url, size_t* user_index) const
      OVERRIDE {
    *user_index = sign_in_user_index_;
    return url == wallet::GetSignInContinueUrl();
  }

 private:
  // To specify our own metric logger.
  virtual const AutofillMetrics& GetMetricLogger() const OVERRIDE {
    return metric_logger_;
  }

  const AutofillMetrics& metric_logger_;
  TestPersonalDataManager test_manager_;
  testing::NiceMock<MockAddressValidator> mock_validator_;
  testing::NiceMock<wallet::MockWalletClient> mock_wallet_client_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  bool use_validation_;

  // A list of notifications to show in the notification area of the dialog.
  // This is used to control what |CurrentNotifications()| returns for testing.
  std::vector<DialogNotification> notifications_;

  // Allows generation of WeakPtrs, so controller liveness can be tested.
  base::WeakPtrFactory<TestAutofillDialogController> weak_ptr_factory_;

  // The user index that is assigned in IsSignInContinueUrl().
  size_t sign_in_user_index_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillDialogController);
};

// This is a copy of ui_test_utils::UrlLoadObserver, except it observes
// NAV_ENTRY_COMMITTED instead of LOAD_STOP. This is to match the notification
// that AutofillDialogControllerImpl observes. Since NAV_ENTRY_COMMITTED comes
// before LOAD_STOP, and the controller deletes the web contents after receiving
// the former, we will sometimes fail to observe a LOAD_STOP.
// TODO(estade): Should the controller observe LOAD_STOP instead?
class NavEntryCommittedObserver : public content::WindowedNotificationObserver {
 public:
  NavEntryCommittedObserver(const GURL& url,
                            const content::NotificationSource& source)
    : WindowedNotificationObserver(content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                                   source),
      url_(url) {}

  virtual ~NavEntryCommittedObserver() {}

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    content::LoadCommittedDetails* load_details =
        content::Details<content::LoadCommittedDetails>(details).ptr();
    if (load_details->entry->GetVirtualURL() != url_)
      return;

    WindowedNotificationObserver::Observe(type, source, details);
  }

 private:
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(NavEntryCommittedObserver);
};

}  // namespace

class AutofillDialogControllerTest : public InProcessBrowserTest {
 public:
  AutofillDialogControllerTest() : controller_(NULL) {}
  virtual ~AutofillDialogControllerTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(::switches::kReduceSecurityForTesting);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    autofill::test::DisableSystemServices(browser()->profile()->GetPrefs());
    InitializeController();
  }

 protected:
  bool SectionHasField(DialogSection section, ServerFieldType type) {
    const DetailInputs& fields =
        controller()->RequestedFieldsForSection(section);
    for (size_t i = 0; i < fields.size(); ++i) {
      if (type == fields[i].type)
        return true;
    }
    return false;
  }

  // A helper function that cycles the MessageLoop, and on Mac, the Cocoa run
  // loop. It also drains the NSAutoreleasePool.
  void CycleRunLoops() {
    content::RunAllPendingInMessageLoop();
#if defined(OS_MACOSX)
    chrome::testing::NSRunLoopRunAllPending();
    AutoreleasePool()->Recycle();
#endif
  }

  void InitializeControllerWithoutShowing() {
    if (controller_)
      controller_->Hide();

    FormData form;
    form.name = ASCIIToUTF16("TestForm");
    form.user_submitted = true;

    FormFieldData field;
    field.autocomplete_attribute = "shipping tel";
    form.fields.push_back(field);

    FormFieldData cc;
    cc.autocomplete_attribute = "cc-number";
    form.fields.push_back(cc);

    test_generated_bubble_controller_ =
        new testing::NiceMock<TestGeneratedCreditCardBubbleController>(
            GetActiveWebContents());
    ASSERT_TRUE(test_generated_bubble_controller_->IsInstalled());

    message_loop_runner_ = new content::MessageLoopRunner;
    controller_ = new TestAutofillDialogController(
        GetActiveWebContents(),
        form,
        metric_logger_,
        message_loop_runner_);
  }

  void InitializeController() {
    InitializeControllerWithoutShowing();
    controller_->Show();
    CycleRunLoops();  // Ensures dialog is fully visible.
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderViewHost* GetRenderViewHost() {
    return GetActiveWebContents()->GetRenderViewHost();
  }

  scoped_ptr<AutofillDialogViewTester> GetViewTester() {
    return AutofillDialogViewTester::For(controller()->view()).Pass();
  }

  const MockAutofillMetrics& metric_logger() { return metric_logger_; }
  TestAutofillDialogController* controller() { return controller_; }

  void RunMessageLoop() {
    message_loop_runner_->Run();
  }

  // Loads an HTML page in |GetActiveWebContents()| with markup as follows:
  // <form>|form_inner_html|</form>. After loading, emulates a click event on
  // the page as requestAutocomplete() must be in response to a user gesture.
  // Returns the |AutofillDialogControllerImpl| created by this invocation.
  AutofillDialogControllerImpl* SetUpHtmlAndInvoke(
      const std::string& form_inner_html) {
    content::WebContents* contents = GetActiveWebContents();
    ChromeAutofillClient* client =
        ChromeAutofillClient::FromWebContents(contents);
    CHECK(!client->GetDialogControllerForTesting());

    ui_test_utils::NavigateToURL(
        browser(), GURL(std::string("data:text/html,") +
        "<!doctype html>"
        "<html>"
          "<body>"
            "<form>" + form_inner_html + "</form>"
            "<script>"
              "var invalidEvents = [];"
              "function recordInvalid(e) {"
                "if (e.type != 'invalid') throw 'only invalid events allowed';"
                "invalidEvents.push(e);"
              "}"
              "function send(msg) {"
                "domAutomationController.setAutomationId(0);"
                "domAutomationController.send(msg);"
              "}"
              "document.forms[0].onautocompleteerror = function(e) {"
                "send('error: ' + e.reason);"
              "};"
              "document.forms[0].onautocomplete = function() {"
                "send('success');"
              "};"
              "window.onclick = function() {"
                "var inputs = document.forms[0].querySelectorAll('input');"
                "for (var i = 0; i < inputs.length; ++i) {"
                  "inputs[i].oninvalid = recordInvalid;"
                "}"
                "document.forms[0].requestAutocomplete();"
                "send('clicked');"
              "};"
              "function loadIframe() {"
              "  var iframe = document.createElement('iframe');"
              "  iframe.onload = function() {"
              "    send('iframe loaded');"
              "  };"
              "  iframe.src = 'about:blank';"
              "  document.body.appendChild(iframe);"
              "}"
              "function getValueForFieldOfType(type) {"
              "  var fields = document.getElementsByTagName('input');"
              "  for (var i = 0; i < fields.length; i++) {"
              "    if (fields[i].autocomplete == type) {"
              "      send(fields[i].value);"
              "      return;"
              "    }"
              "  }"
              "  send('');"
              "};"
            "</script>"
          "</body>"
        "</html>"));

    InitiateDialog();
    AutofillDialogControllerImpl* controller =
        static_cast<AutofillDialogControllerImpl*>(
            client->GetDialogControllerForTesting());
    return controller;
  }

  // Loads an html page on a provided server, the causes it to launch rAc.
  // Returns whether rAc succesfully launched.
  bool RunTestPage(const net::SpawnedTestServer& server) {
    GURL url = server.GetURL(
        "files/request_autocomplete/test_page.html");
    ui_test_utils::NavigateToURL(browser(), url);

    // Pass through the broken SSL interstitial, if any.
    content::WebContents* contents = GetActiveWebContents();
    content::InterstitialPage* interstitial_page =
        contents->GetInterstitialPage();
    if (interstitial_page) {
      ui_test_utils::UrlLoadObserver observer(
          url,
          content::Source<content::NavigationController>(
              &contents->GetController()));
      interstitial_page->Proceed();
      observer.Wait();
    }

    InitiateDialog();

    ChromeAutofillClient* client =
        ChromeAutofillClient::FromWebContents(contents);
    AutofillDialogControllerImpl* controller =
        static_cast<AutofillDialogControllerImpl*>(
            client->GetDialogControllerForTesting());
    return !!controller;
  }

  // Wait for a message from the DOM automation controller (from JS in the
  // page). Requires |SetUpHtmlAndInvoke()| be called first.
  void ExpectDomMessage(const std::string& expected) {
    std::string message;
    ASSERT_TRUE(dom_message_queue_->WaitForMessage(&message));
    dom_message_queue_->ClearQueue();
    EXPECT_EQ("\"" + expected + "\"", message);
  }

  void InitiateDialog() {
    dom_message_queue_.reset(new content::DOMMessageQueue);

    // Triggers the onclick handler which invokes requestAutocomplete().
    content::WebContents* contents = GetActiveWebContents();
    content::SimulateMouseClick(contents, 0, blink::WebMouseEvent::ButtonLeft);
    ExpectDomMessage("clicked");
  }

  // Returns the value filled into the first field with autocomplete attribute
  // equal to |autocomplete_type|, or an empty string if there is no such field.
  std::string GetValueForHTMLFieldOfType(const std::string& autocomplete_type) {
    std::string script = "getValueForFieldOfType('" + autocomplete_type + "');";
    std::string result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(GetRenderViewHost(),
                                                       script,
                                                       &result));
    return result;
  }

  void AddCreditcardToProfile(Profile* profile, const CreditCard& card) {
    PersonalDataManagerFactory::GetForProfile(profile)->AddCreditCard(card);
    WaitForWebDB();
  }

  void AddAutofillProfileToProfile(Profile* profile,
                                   const AutofillProfile& autofill_profile) {
    PersonalDataManagerFactory::GetForProfile(profile)->AddProfile(
        autofill_profile);
    WaitForWebDB();
  }

  TestGeneratedCreditCardBubbleController* test_generated_bubble_controller() {
    return test_generated_bubble_controller_;
  }

 private:
  void WaitForWebDB() {
    content::RunAllPendingInMessageLoop(content::BrowserThread::DB);
  }

  testing::NiceMock<MockAutofillMetrics> metric_logger_;
  TestAutofillDialogController* controller_;  // Weak reference.
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  scoped_ptr<content::DOMMessageQueue> dom_message_queue_;

  // Weak; owned by the active web contents.
  TestGeneratedCreditCardBubbleController* test_generated_bubble_controller_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDialogControllerTest);
};

// Submit the form data.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, Submit) {
  GetViewTester()->SubmitForTesting();
  RunMessageLoop();

  EXPECT_EQ(AutofillMetrics::DIALOG_ACCEPTED,
            metric_logger().dialog_dismissal_action());
}

// Cancel out of the dialog.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, Cancel) {
  EXPECT_CALL(metric_logger(),
              LogDialogDismissalState(
                  AutofillMetrics::DIALOG_CANCELED_NO_INVALID_FIELDS));

  GetViewTester()->CancelForTesting();
  RunMessageLoop();

  EXPECT_EQ(AutofillMetrics::DIALOG_CANCELED,
            metric_logger().dialog_dismissal_action());
}

// Take some other action that dismisses the dialog.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, Hide) {
  EXPECT_CALL(metric_logger(),
              LogDialogDismissalState(
                  AutofillMetrics::DIALOG_CANCELED_NO_INVALID_FIELDS));
  controller()->Hide();

  RunMessageLoop();

  EXPECT_EQ(AutofillMetrics::DIALOG_CANCELED,
            metric_logger().dialog_dismissal_action());
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, CancelWithSuggestions) {
  EXPECT_CALL(metric_logger(),
              LogDialogDismissalState(
                  AutofillMetrics::DIALOG_CANCELED_NO_EDITS));

  CreditCard card(test::GetVerifiedCreditCard());
  controller()->GetTestingManager()->AddTestingCreditCard(&card);
  AutofillProfile profile(test::GetVerifiedProfile());
  controller()->GetTestingManager()->AddTestingProfile(&profile);

  EXPECT_FALSE(controller()->IsManuallyEditingSection(SECTION_CC));
  EXPECT_FALSE(controller()->IsManuallyEditingSection(SECTION_BILLING));
  EXPECT_FALSE(controller()->IsManuallyEditingSection(SECTION_SHIPPING));

  GetViewTester()->CancelForTesting();
  RunMessageLoop();

  EXPECT_EQ(AutofillMetrics::DIALOG_CANCELED,
            metric_logger().dialog_dismissal_action());
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, AcceptWithSuggestions) {
  EXPECT_CALL(metric_logger(),
              LogDialogDismissalState(
                  AutofillMetrics::DIALOG_ACCEPTED_EXISTING_AUTOFILL_DATA));

  CreditCard card(test::GetVerifiedCreditCard());
  controller()->GetTestingManager()->AddTestingCreditCard(&card);
  AutofillProfile profile(test::GetVerifiedProfile());
  controller()->GetTestingManager()->AddTestingProfile(&profile);

  EXPECT_FALSE(controller()->IsManuallyEditingSection(SECTION_CC));
  EXPECT_FALSE(controller()->IsManuallyEditingSection(SECTION_BILLING));
  EXPECT_FALSE(controller()->IsManuallyEditingSection(SECTION_SHIPPING));

  GetViewTester()->SubmitForTesting();
  RunMessageLoop();

  EXPECT_EQ(AutofillMetrics::DIALOG_ACCEPTED,
            metric_logger().dialog_dismissal_action());
}

// Ensure that Hide() will only destroy the controller object after the
// message loop has run. Otherwise, there may be read-after-free issues
// during some tests.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, DeferredDestruction) {
  base::WeakPtr<TestAutofillDialogController> weak_ptr =
      controller()->AsWeakPtr();
  EXPECT_TRUE(weak_ptr.get());

  controller()->Hide();
  EXPECT_TRUE(weak_ptr.get());

  RunMessageLoop();
  EXPECT_FALSE(weak_ptr.get());
}

// Ensure that the expected metric is logged when the dialog is closed during
// signin.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, CloseDuringSignin) {
  controller()->SignInLinkClicked();

  EXPECT_CALL(metric_logger(),
              LogDialogDismissalState(
                  AutofillMetrics::DIALOG_CANCELED_DURING_SIGNIN));
  GetViewTester()->CancelForTesting();

  RunMessageLoop();

  EXPECT_EQ(AutofillMetrics::DIALOG_CANCELED,
            metric_logger().dialog_dismissal_action());
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, FillInputFromAutofill) {
  AutofillProfile full_profile(test::GetFullProfile());
  const base::string16 formatted_phone(ASCIIToUTF16("+1 (310) 555 1234"));
  full_profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, formatted_phone);
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);

  // Dialog is already asking for a new billing address.
  EXPECT_TRUE(controller()->IsManuallyEditingSection(SECTION_BILLING));

  // Select "Add new shipping address...".
  ui::MenuModel* model = controller()->MenuModelForSection(SECTION_SHIPPING);
  model->ActivatedAt(model->GetItemCount() - 2);
  ASSERT_TRUE(controller()->IsManuallyEditingSection(SECTION_SHIPPING));

  // Enter something in a shipping input.
  const DetailInputs& inputs =
      controller()->RequestedFieldsForSection(SECTION_SHIPPING);
  const ServerFieldType triggering_type = inputs[0].type;
  base::string16 value = full_profile.GetRawInfo(triggering_type);
  scoped_ptr<AutofillDialogViewTester> view = GetViewTester();
  view->SetTextContentsOfInput(triggering_type,
                               value.substr(0, value.size() / 2));
  view->ActivateInput(triggering_type);

  ASSERT_EQ(triggering_type, controller()->popup_input_type());
  controller()->DidAcceptSuggestion(base::string16(), 0);

  // All inputs should be filled.
  AutofillProfileWrapper wrapper(&full_profile);
  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_EQ(wrapper.GetInfoForDisplay(AutofillType(inputs[i].type)),
              view->GetTextContentsOfInput(inputs[i].type));

    // Double check the correct formatting is used for the phone number.
    if (inputs[i].type == PHONE_HOME_WHOLE_NUMBER)
      EXPECT_EQ(formatted_phone, view->GetTextContentsOfInput(inputs[i].type));
  }

  // Inputs from the other section (billing) should be left alone.
  const DetailInputs& other_section_inputs =
      controller()->RequestedFieldsForSection(SECTION_BILLING);
  for (size_t i = 0; i < inputs.size(); ++i) {
    base::string16 input_value =
        view->GetTextContentsOfInput(other_section_inputs[i].type);
    // If there's a combobox, the string should be non-empty.
    if (controller()->ComboboxModelForAutofillType(
            other_section_inputs[i].type)) {
      EXPECT_NE(base::string16(), input_value);
    } else {
      EXPECT_EQ(base::string16(), input_value);
    }
  }

  // Now simulate some user edits and try again.
  std::vector<base::string16> expectations;
  for (size_t i = 0; i < inputs.size(); ++i) {
    if (controller()->ComboboxModelForAutofillType(inputs[i].type)) {
      expectations.push_back(base::string16());
      continue;
    }
    base::string16 users_input = i % 2 == 0 ? base::string16()
                                            : ASCIIToUTF16("dummy");
    view->SetTextContentsOfInput(inputs[i].type, users_input);
    // Empty inputs should be filled, others should be left alone.
    base::string16 expectation =
        inputs[i].type == triggering_type || users_input.empty() ?
        wrapper.GetInfoForDisplay(AutofillType(inputs[i].type)) :
        users_input;
    expectations.push_back(expectation);
  }

  view->SetTextContentsOfInput(triggering_type,
                               value.substr(0, value.size() / 2));
  view->ActivateInput(triggering_type);
  ASSERT_EQ(triggering_type, controller()->popup_input_type());
  controller()->DidAcceptSuggestion(base::string16(), 0);

  for (size_t i = 0; i < inputs.size(); ++i) {
    if (controller()->ComboboxModelForAutofillType(inputs[i].type))
      continue;
    EXPECT_EQ(expectations[i], view->GetTextContentsOfInput(inputs[i].type));
  }

  EXPECT_CALL(metric_logger(),
              LogDialogDismissalState(
                  AutofillMetrics::DIALOG_ACCEPTED_SAVE_TO_AUTOFILL));
  view->SubmitForTesting();
}

// This test makes sure that picking a profile variant in the Autofill
// popup works as expected.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest,
                       FillInputFromAutofillVariant) {
  AutofillProfile full_profile(test::GetFullProfile());

  // Set up some variant data.
  std::vector<base::string16> names;
  names.push_back(ASCIIToUTF16("John Doe"));
  names.push_back(ASCIIToUTF16("Jane Doe"));
  full_profile.SetRawMultiInfo(NAME_FULL, names);
  std::vector<base::string16> emails;
  emails.push_back(ASCIIToUTF16("user@example.com"));
  emails.push_back(ASCIIToUTF16("admin@example.com"));
  full_profile.SetRawMultiInfo(EMAIL_ADDRESS, emails);
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);

  const DetailInputs& inputs =
      controller()->RequestedFieldsForSection(SECTION_BILLING);
  const ServerFieldType triggering_type = inputs[0].type;
  EXPECT_EQ(NAME_BILLING_FULL, triggering_type);
  scoped_ptr<AutofillDialogViewTester> view = GetViewTester();
  view->ActivateInput(triggering_type);

  ASSERT_EQ(triggering_type, controller()->popup_input_type());

  // Choose the variant suggestion.
  controller()->DidAcceptSuggestion(base::string16(), 1);

  // All inputs should be filled.
  AutofillProfileWrapper wrapper(
      &full_profile, AutofillType(NAME_BILLING_FULL), 1);
  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_EQ(wrapper.GetInfoForDisplay(AutofillType(inputs[i].type)),
              view->GetTextContentsOfInput(inputs[i].type));
  }

  // Make sure the wrapper applies the variant index to the right group.
  EXPECT_EQ(names[1], wrapper.GetInfo(AutofillType(NAME_BILLING_FULL)));
  // Make sure the wrapper doesn't apply the variant index to the wrong group.
  EXPECT_EQ(emails[0], wrapper.GetInfo(AutofillType(EMAIL_ADDRESS)));
}

// Tests that changing the value of a CC expiration date combobox works as
// expected when Autofill is used to fill text inputs.
//
// Flaky on Win7, WinXP, and Win Aura.  http://crbug.com/270314.
#if defined(OS_WIN)
#define MAYBE_FillComboboxFromAutofill DISABLED_FillComboboxFromAutofill
#else
#define MAYBE_FillComboboxFromAutofill FillComboboxFromAutofill
#endif
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest,
                       MAYBE_FillComboboxFromAutofill) {
  CreditCard card1;
  test::SetCreditCardInfo(&card1, "JJ Smith", "4111111111111111", "12", "2018");
  controller()->GetTestingManager()->AddTestingCreditCard(&card1);
  CreditCard card2;
  test::SetCreditCardInfo(&card2, "B Bird", "3111111111111111", "11", "2017");
  controller()->GetTestingManager()->AddTestingCreditCard(&card2);
  AutofillProfile full_profile(test::GetFullProfile());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);

  const DetailInputs& inputs =
      controller()->RequestedFieldsForSection(SECTION_CC);
  const ServerFieldType triggering_type = inputs[0].type;
  base::string16 value = card1.GetRawInfo(triggering_type);
  scoped_ptr<AutofillDialogViewTester> view = GetViewTester();
  view->SetTextContentsOfInput(triggering_type,
                               value.substr(0, value.size() / 2));
  view->ActivateInput(triggering_type);

  ASSERT_EQ(triggering_type, controller()->popup_input_type());
  controller()->DidAcceptSuggestion(base::string16(), 0);

  // All inputs should be filled.
  AutofillCreditCardWrapper wrapper1(&card1);
  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_EQ(wrapper1.GetInfo(AutofillType(inputs[i].type)),
              view->GetTextContentsOfInput(inputs[i].type));
  }

  // Try again with different data. Only expiration date and the triggering
  // input should be overwritten.
  value = card2.GetRawInfo(triggering_type);
  view->SetTextContentsOfInput(triggering_type,
                               value.substr(0, value.size() / 2));
  view->ActivateInput(triggering_type);
  ASSERT_EQ(triggering_type, controller()->popup_input_type());
  controller()->DidAcceptSuggestion(base::string16(), 0);

  AutofillCreditCardWrapper wrapper2(&card2);
  for (size_t i = 0; i < inputs.size(); ++i) {
    const ServerFieldType type = inputs[i].type;
    if (type == triggering_type ||
        type == CREDIT_CARD_EXP_MONTH ||
        type == CREDIT_CARD_EXP_4_DIGIT_YEAR) {
      EXPECT_EQ(wrapper2.GetInfo(AutofillType(type)),
                view->GetTextContentsOfInput(type));
    } else if (type == CREDIT_CARD_VERIFICATION_CODE) {
      EXPECT_TRUE(view->GetTextContentsOfInput(type).empty());
    } else {
      EXPECT_EQ(wrapper1.GetInfo(AutofillType(type)),
                view->GetTextContentsOfInput(type));
    }
  }

  // Now fill from a profile. It should not overwrite any CC info.
  const DetailInputs& billing_inputs =
      controller()->RequestedFieldsForSection(SECTION_BILLING);
  const ServerFieldType billing_triggering_type = billing_inputs[0].type;
  value = full_profile.GetRawInfo(triggering_type);
  view->SetTextContentsOfInput(billing_triggering_type,
                               value.substr(0, value.size() / 2));
  view->ActivateInput(billing_triggering_type);

  ASSERT_EQ(billing_triggering_type, controller()->popup_input_type());
  controller()->DidAcceptSuggestion(base::string16(), 0);

  for (size_t i = 0; i < inputs.size(); ++i) {
    const ServerFieldType type = inputs[i].type;
    if (type == triggering_type ||
        type == CREDIT_CARD_EXP_MONTH ||
        type == CREDIT_CARD_EXP_4_DIGIT_YEAR) {
      EXPECT_EQ(wrapper2.GetInfo(AutofillType(type)),
                view->GetTextContentsOfInput(type));
    } else if (type == CREDIT_CARD_VERIFICATION_CODE) {
      EXPECT_TRUE(view->GetTextContentsOfInput(type).empty());
    } else {
      EXPECT_EQ(wrapper1.GetInfo(AutofillType(type)),
                view->GetTextContentsOfInput(type));
    }
  }
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, ShouldShowErrorBubble) {
  controller()->set_use_validation(true);
  EXPECT_TRUE(controller()->ShouldShowErrorBubble());

  CreditCard card(test::GetCreditCard());
  ASSERT_FALSE(card.IsVerified());
  controller()->GetTestingManager()->AddTestingCreditCard(&card);

  EXPECT_TRUE(controller()->IsManuallyEditingSection(SECTION_CC));
  scoped_ptr<AutofillDialogViewTester> view = GetViewTester();
  view->SetTextContentsOfInput(
      CREDIT_CARD_NUMBER,
      card.GetRawInfo(CREDIT_CARD_NUMBER).substr(0, 1));

  view->ActivateInput(CREDIT_CARD_NUMBER);
  EXPECT_FALSE(controller()->ShouldShowErrorBubble());

  controller()->FocusMoved();
  EXPECT_TRUE(controller()->ShouldShowErrorBubble());

  EXPECT_CALL(metric_logger(),
              LogDialogDismissalState(
                  AutofillMetrics::DIALOG_CANCELED_WITH_INVALID_FIELDS));
  controller()->Hide();
}

// Ensure that expired cards trigger invalid suggestions.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, ExpiredCard) {
  CreditCard verified_card(test::GetCreditCard());
  verified_card.set_origin("Chrome settings");
  ASSERT_TRUE(verified_card.IsVerified());
  controller()->GetTestingManager()->AddTestingCreditCard(&verified_card);

  CreditCard expired_card(test::GetCreditCard());
  expired_card.set_origin("Chrome settings");
  expired_card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, ASCIIToUTF16("2007"));
  ASSERT_TRUE(expired_card.IsVerified());
  ASSERT_FALSE(
      autofill::IsValidCreditCardExpirationDate(
          expired_card.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR),
          expired_card.GetRawInfo(CREDIT_CARD_EXP_MONTH),
          base::Time::Now()));
  controller()->GetTestingManager()->AddTestingCreditCard(&expired_card);

  ui::MenuModel* model = controller()->MenuModelForSection(SECTION_CC);
  ASSERT_EQ(4, model->GetItemCount());

  ASSERT_TRUE(model->IsItemCheckedAt(0));
  EXPECT_FALSE(controller()->IsEditingExistingData(SECTION_CC));

  model->ActivatedAt(1);
  ASSERT_TRUE(model->IsItemCheckedAt(1));
  EXPECT_TRUE(controller()->IsEditingExistingData(SECTION_CC));
}

// Notifications with long message text should not make the dialog bigger.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, LongNotifications) {
  const gfx::Size no_notification_size = GetViewTester()->GetSize();
  ASSERT_GT(no_notification_size.width(), 0);

  std::vector<DialogNotification> notifications;
  notifications.push_back(
      DialogNotification(DialogNotification::DEVELOPER_WARNING, ASCIIToUTF16(
          "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do "
          "eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim "
          "ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut "
          "aliquip ex ea commodo consequat. Duis aute irure dolor in "
          "reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla "
          "pariatur. Excepteur sint occaecat cupidatat non proident, sunt in "
          "culpa qui officia deserunt mollit anim id est laborum.")));
  controller()->set_notifications(notifications);
  controller()->view()->UpdateNotificationArea();

  EXPECT_EQ(no_notification_size.width(),
            GetViewTester()->GetSize().width());
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, AutocompleteEvent) {
  AutofillDialogControllerImpl* controller =
      SetUpHtmlAndInvoke("<input autocomplete='cc-name'>");
  ASSERT_TRUE(controller);

  AddCreditcardToProfile(controller->profile(), test::GetVerifiedCreditCard());
  AddAutofillProfileToProfile(controller->profile(),
                              test::GetVerifiedProfile());

  scoped_ptr<AutofillDialogViewTester> view =
      AutofillDialogViewTester::For(
          static_cast<TestAutofillDialogController*>(controller)->view());
  view->SetTextContentsOfSuggestionInput(SECTION_CC, ASCIIToUTF16("123"));
  view->SubmitForTesting();
  ExpectDomMessage("success");
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest,
                       AutocompleteErrorEventReasonInvalid) {
  AutofillDialogControllerImpl* controller =
      SetUpHtmlAndInvoke("<input autocomplete='cc-name' pattern='.*zebra.*'>");
  ASSERT_TRUE(controller);

  const CreditCard& credit_card = test::GetVerifiedCreditCard();
  ASSERT_TRUE(
      credit_card.GetRawInfo(CREDIT_CARD_NAME).find(ASCIIToUTF16("zebra")) ==
          base::string16::npos);
  AddCreditcardToProfile(controller->profile(), credit_card);
  AddAutofillProfileToProfile(controller->profile(),
                              test::GetVerifiedProfile());

  scoped_ptr<AutofillDialogViewTester> view =
      AutofillDialogViewTester::For(
          static_cast<TestAutofillDialogController*>(controller)->view());
  view->SetTextContentsOfSuggestionInput(SECTION_CC, ASCIIToUTF16("123"));
  view->SubmitForTesting();
  ExpectDomMessage("error: invalid");

  int invalid_count = -1;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      GetRenderViewHost(), "send(invalidEvents.length);", &invalid_count));
  EXPECT_EQ(1, invalid_count);

  std::string invalid_type;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      GetRenderViewHost(),
      "send(invalidEvents[0].target.autocomplete);",
      &invalid_type));
  EXPECT_EQ("cc-name", invalid_type);
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest,
                       AutocompleteErrorEventReasonCancel) {
  AutofillDialogControllerImpl* controller =
      SetUpHtmlAndInvoke("<input autocomplete='cc-name'>");
  ASSERT_TRUE(controller);
  AutofillDialogViewTester::For(
      static_cast<TestAutofillDialogController*>(controller)->view())->
          CancelForTesting();
  ExpectDomMessage("error: cancel");
}

// http://crbug.com/318526
#if defined(OS_MACOSX)
#define MAYBE_ErrorWithFrameNavigation DISABLED_ErrorWithFrameNavigation
#else
#define MAYBE_ErrorWithFrameNavigation ErrorWithFrameNavigation
#endif
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest,
                       MAYBE_ErrorWithFrameNavigation) {
  AutofillDialogControllerImpl* controller =
      SetUpHtmlAndInvoke("<input autocomplete='cc-name'>");
  ASSERT_TRUE(controller);

  std::string unused;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(GetRenderViewHost(),
                                                     "loadIframe();",
                                                     &unused));
  ExpectDomMessage("iframe loaded");

  AutofillDialogViewTester::For(
      static_cast<TestAutofillDialogController*>(controller)->view())->
          CancelForTesting();
  ExpectDomMessage("error: cancel");
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, NoCvcSegfault) {
  controller()->set_use_validation(true);

  CreditCard credit_card(test::GetVerifiedCreditCard());
  controller()->GetTestingManager()->AddTestingCreditCard(&credit_card);
  EXPECT_FALSE(controller()->IsEditingExistingData(SECTION_CC));

  ASSERT_NO_FATAL_FAILURE(GetViewTester()->SubmitForTesting());
}

// Flaky on Win7, WinXP, and Win Aura.  http://crbug.com/270314.
#if defined(OS_WIN)
#define MAYBE_PreservedSections DISABLED_PreservedSections
#else
#define MAYBE_PreservedSections PreservedSections
#endif
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, MAYBE_PreservedSections) {
  controller()->set_use_validation(true);

  scoped_ptr<AutofillDialogViewTester> view = GetViewTester();
  view->SetTextContentsOfInput(CREDIT_CARD_NUMBER,
                               ASCIIToUTF16("4111111111111111"));

  // Create some invalid, manually inputted shipping data.
  view->SetTextContentsOfInput(ADDRESS_HOME_ZIP, ASCIIToUTF16("shipping zip"));

  // Switch to Wallet by simulating a successful server response.
  controller()->OnDidFetchWalletCookieValue(std::string());
  controller()->OnDidGetWalletItems(
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED));
  ASSERT_TRUE(controller()->IsPayingWithWallet());

  // The valid data should be preserved.
  EXPECT_EQ(ASCIIToUTF16("4111111111111111"),
            view->GetTextContentsOfInput(CREDIT_CARD_NUMBER));

  // The invalid data should be dropped.
  EXPECT_TRUE(view->GetTextContentsOfInput(ADDRESS_HOME_ZIP).empty());

  // Switch back to Autofill.
  ui::MenuModel* account_chooser = controller()->MenuModelForAccountChooser();
  account_chooser->ActivatedAt(account_chooser->GetItemCount() - 1);
  ASSERT_FALSE(controller()->IsPayingWithWallet());

  // The valid data should still be preserved when switched back.
  EXPECT_EQ(ASCIIToUTF16("4111111111111111"),
            view->GetTextContentsOfInput(CREDIT_CARD_NUMBER));
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest,
                       GeneratedCardLastFourAfterVerifyCvv) {
  controller()->OnDidFetchWalletCookieValue(std::string());

  scoped_ptr<wallet::WalletItems> wallet_items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  wallet_items->AddAddress(wallet::GetTestShippingAddress());

  base::string16 last_four =
      wallet_items->instruments()[0]->TypeAndLastFourDigits();
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  scoped_ptr<AutofillDialogViewTester> test_view = GetViewTester();
  EXPECT_FALSE(test_view->IsShowingOverlay());
  EXPECT_CALL(*controller(), LoadRiskFingerprintData());
  controller()->OnAccept();
  EXPECT_TRUE(test_view->IsShowingOverlay());

  EXPECT_CALL(*controller()->GetTestingWalletClient(), GetFullWallet(_));
  scoped_ptr<risk::Fingerprint> fingerprint(new risk::Fingerprint());
  fingerprint->mutable_machine_characteristics()->mutable_screen_size()->
      set_width(1024);
  controller()->OnDidLoadRiskFingerprintData(fingerprint.Pass());

  controller()->OnDidGetFullWallet(
      wallet::GetTestFullWalletWithRequiredActions(
          std::vector<wallet::RequiredAction>(1, wallet::VERIFY_CVV)));

  ASSERT_TRUE(controller()->IsSubmitPausedOn(wallet::VERIFY_CVV));

  std::string fake_cvc("123");
  test_view->SetTextContentsOfSuggestionInput(SECTION_CC_BILLING,
                                              ASCIIToUTF16(fake_cvc));

  EXPECT_FALSE(test_view->IsShowingOverlay());
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              AuthenticateInstrument(_, fake_cvc));
  controller()->OnAccept();
  EXPECT_TRUE(test_view->IsShowingOverlay());

  EXPECT_CALL(metric_logger(),
              LogDialogDismissalState(
                  AutofillMetrics::DIALOG_ACCEPTED_EXISTING_WALLET_DATA));

  EXPECT_CALL(*controller()->GetTestingWalletClient(), GetFullWallet(_));
  controller()->OnDidAuthenticateInstrument(true);
  controller()->OnDidGetFullWallet(wallet::GetTestFullWallet());
  controller()->ForceFinishSubmit();

  RunMessageLoop();

  EXPECT_EQ(1, test_generated_bubble_controller()->bubbles_shown());
  EXPECT_EQ(last_four, test_generated_bubble_controller()->backing_card_name());
}

// Simulates the user signing in to the dialog from the inline web contents.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, SimulateSuccessfulSignIn) {
#if defined(OS_WIN)
  // TODO(msw): Fix potential flakiness on Windows XP; http://crbug.com/333641
  if (base::win::GetVersion() <= base::win::VERSION_XP)
    return;
#endif
  browser()->profile()->GetPrefs()->SetBoolean(
      ::prefs::kAutofillDialogPayWithoutWallet,
      true);

  InitializeController();

  controller()->OnDidFetchWalletCookieValue(std::string());
  controller()->OnDidGetWalletItems(
      wallet::GetTestWalletItemsWithRequiredAction(wallet::GAIA_AUTH));

  NavEntryCommittedObserver sign_in_page_observer(
      controller()->SignInUrl(),
      content::NotificationService::AllSources());

  // Simulate a user clicking "Sign In" (which loads dialog's web contents).
  controller()->SignInLinkClicked();
  EXPECT_TRUE(controller()->ShouldShowSignInWebView());

  scoped_ptr<AutofillDialogViewTester> view = GetViewTester();
  content::WebContents* sign_in_contents = view->GetSignInWebContents();
  ASSERT_TRUE(sign_in_contents);

  sign_in_page_observer.Wait();

  NavEntryCommittedObserver continue_page_observer(
      wallet::GetSignInContinueUrl(),
      content::NotificationService::AllSources());

  EXPECT_EQ(sign_in_contents->GetURL(), controller()->SignInUrl());

  AccountChooserModel* account_chooser_model =
      controller()->AccountChooserModelForTesting();
  EXPECT_FALSE(account_chooser_model->WalletIsSelected());

  content::OpenURLParams params(wallet::GetSignInContinueUrl(),
                                content::Referrer(),
                                CURRENT_TAB,
                                content::PAGE_TRANSITION_LINK,
                                true);

  sign_in_contents->GetDelegate()->OpenURLFromTab(sign_in_contents, params);

  EXPECT_CALL(*controller()->GetTestingWalletClient(), GetWalletItems(_, _));
  continue_page_observer.Wait();
  content::RunAllPendingInMessageLoop();

  EXPECT_FALSE(controller()->ShouldShowSignInWebView());

  scoped_ptr<wallet::WalletItems> wallet_items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  // Wallet should now be selected and Chrome shouldn't have crashed (which can
  // happen if the WebContents is deleted while proccessing a nav entry commit).
  EXPECT_TRUE(account_chooser_model->WalletIsSelected());
  EXPECT_FALSE(controller()->IsManuallyEditingSection(SECTION_CC_BILLING));
  EXPECT_FALSE(controller()->IsManuallyEditingSection(SECTION_SHIPPING));

  EXPECT_CALL(metric_logger(),
              LogDialogDismissalState(
                  AutofillMetrics::DIALOG_ACCEPTED_EXISTING_WALLET_DATA));
  view->SubmitForTesting();
  controller()->OnDidGetFullWallet(wallet::GetTestFullWallet());
  controller()->ForceFinishSubmit();
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, AddAccount) {
#if defined(OS_WIN)
  // TODO(msw): Fix potential flakiness on Windows XP; http://crbug.com/333641
  if (base::win::GetVersion() <= base::win::VERSION_XP)
    return;
#endif

  controller()->OnDidFetchWalletCookieValue(std::string());
  std::vector<std::string> usernames;
  usernames.push_back("user_0@example.com");
  controller()->OnDidGetWalletItems(
      wallet::GetTestWalletItemsWithUsers(usernames, 0));

  // Switch to Autofill.
  AccountChooserModel* account_chooser_model =
      controller()->AccountChooserModelForTesting();
  account_chooser_model->ActivatedAt(
      account_chooser_model->GetItemCount() - 1);

  NavEntryCommittedObserver sign_in_page_observer(
      controller()->SignInUrl(),
      content::NotificationService::AllSources());

  // Simulate a user clicking "add account".
  account_chooser_model->ActivatedAt(
      account_chooser_model->GetItemCount() - 2);
  EXPECT_TRUE(controller()->ShouldShowSignInWebView());

  scoped_ptr<AutofillDialogViewTester> view = GetViewTester();
  content::WebContents* sign_in_contents = view->GetSignInWebContents();
  ASSERT_TRUE(sign_in_contents);

  sign_in_page_observer.Wait();

  NavEntryCommittedObserver continue_page_observer(
      wallet::GetSignInContinueUrl(),
      content::NotificationService::AllSources());

  EXPECT_EQ(sign_in_contents->GetURL(), controller()->SignInUrl());

  EXPECT_FALSE(account_chooser_model->WalletIsSelected());

  // User signs into new account, account 3.
  controller()->set_sign_in_user_index(3U);
  content::OpenURLParams params(wallet::GetSignInContinueUrl(),
                                content::Referrer(),
                                CURRENT_TAB,
                                content::PAGE_TRANSITION_LINK,
                                true);
  sign_in_contents->GetDelegate()->OpenURLFromTab(sign_in_contents, params);

  EXPECT_CALL(*controller()->GetTestingWalletClient(), GetWalletItems(_, _));
  continue_page_observer.Wait();
  content::RunAllPendingInMessageLoop();

  EXPECT_FALSE(controller()->ShouldShowSignInWebView());
  EXPECT_EQ(3U, controller()->GetTestingWalletClient()->user_index());

  usernames.push_back("user_1@example.com");
  usernames.push_back("user_2@example.com");
  usernames.push_back("user_3@example.com");
  usernames.push_back("user_4@example.com");
  // Curveball: wallet items comes back with user 4 selected.
  controller()->OnDidGetWalletItems(
      wallet::GetTestWalletItemsWithUsers(usernames, 4U));

  EXPECT_TRUE(account_chooser_model->WalletIsSelected());
  EXPECT_EQ(4U, account_chooser_model->GetActiveWalletAccountIndex());
  EXPECT_EQ(4U, controller()->GetTestingWalletClient()->user_index());
}

// Verify that filling a form works correctly, including filling the CVC when
// that is requested separately.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest,
                       FillFormIncludesCVC) {
  AutofillDialogControllerImpl* controller =
      SetUpHtmlAndInvoke("<input autocomplete='cc-csc'>");
  ASSERT_TRUE(controller);

  AddCreditcardToProfile(controller->profile(), test::GetVerifiedCreditCard());
  AddAutofillProfileToProfile(controller->profile(),
                              test::GetVerifiedProfile());

  scoped_ptr<AutofillDialogViewTester> view =
      AutofillDialogViewTester::For(
          static_cast<TestAutofillDialogController*>(controller)->view());
  view->SetTextContentsOfSuggestionInput(SECTION_CC, ASCIIToUTF16("123"));
  view->SubmitForTesting();
  ExpectDomMessage("success");
  EXPECT_EQ("123", GetValueForHTMLFieldOfType("cc-csc"));
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, AddNewClearsComboboxes) {
  // Ensure the input under test is a combobox.
  ASSERT_TRUE(
      controller()->ComboboxModelForAutofillType(CREDIT_CARD_EXP_MONTH));

  // Set up an expired card.
  CreditCard card;
  test::SetCreditCardInfo(&card, "Roy Demeo", "4111111111111111", "8", "2013");
  card.set_origin("Chrome settings");
  ASSERT_TRUE(card.IsVerified());

  // Add the card and check that there's a menu for that section.
  controller()->GetTestingManager()->AddTestingCreditCard(&card);
  ASSERT_TRUE(controller()->MenuModelForSection(SECTION_CC));

  // Select the invalid, suggested card from the menu.
  controller()->MenuModelForSection(SECTION_CC)->ActivatedAt(0);
  EXPECT_TRUE(controller()->IsEditingExistingData(SECTION_CC));

  // Get the contents of the combobox of the credit card's expiration month.
  scoped_ptr<AutofillDialogViewTester> view = GetViewTester();
  base::string16 cc_exp_month_text =
      view->GetTextContentsOfInput(CREDIT_CARD_EXP_MONTH);

  // Select "New X..." from the suggestion menu to clear the section's inputs.
  controller()->MenuModelForSection(SECTION_CC)->ActivatedAt(1);
  EXPECT_FALSE(controller()->IsEditingExistingData(SECTION_CC));

  // Ensure that the credit card expiration month has changed.
  EXPECT_NE(cc_exp_month_text,
            view->GetTextContentsOfInput(CREDIT_CARD_EXP_MONTH));
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, TabOpensToJustRight) {
  ASSERT_TRUE(browser()->is_type_tabbed());

  // Tabs should currently be: / rAc() \.
  content::WebContents* dialog_invoker = controller()->GetWebContents();
  EXPECT_EQ(dialog_invoker, GetActiveWebContents());

  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(0, tab_strip->GetIndexOfWebContents(dialog_invoker));

  // Open a tab to about:blank in the background at the end of the tab strip.
  chrome::AddTabAt(browser(), GURL(), -1, false);
  // Tabs should now be: / rAc() \/ blank \.
  EXPECT_EQ(2, tab_strip->count());
  EXPECT_EQ(0, tab_strip->active_index());
  EXPECT_EQ(dialog_invoker, GetActiveWebContents());

  content::WebContents* blank_tab = tab_strip->GetWebContentsAt(1);

  // Simulate clicking "Manage X...".
  controller()->MenuModelForSection(SECTION_SHIPPING)->ActivatedAt(2);
  // Tab should now be: / rAc() \/ manage 1 \/ blank \.
  EXPECT_EQ(3, tab_strip->count());
  int dialog_index = tab_strip->GetIndexOfWebContents(dialog_invoker);
  EXPECT_EQ(0, dialog_index);
  EXPECT_EQ(1, tab_strip->active_index());
  EXPECT_EQ(2, tab_strip->GetIndexOfWebContents(blank_tab));

  content::WebContents* first_manage_tab = tab_strip->GetWebContentsAt(1);

  // Re-activate the dialog's tab (like a user would have to).
  tab_strip->ActivateTabAt(dialog_index, true);
  EXPECT_EQ(dialog_invoker, GetActiveWebContents());

  // Simulate clicking "Manage X...".
  controller()->MenuModelForSection(SECTION_SHIPPING)->ActivatedAt(2);
  // Tabs should now be: / rAc() \/ manage 2 \/ manage 1 \/ blank \.
  EXPECT_EQ(4, tab_strip->count());
  EXPECT_EQ(0, tab_strip->GetIndexOfWebContents(dialog_invoker));
  EXPECT_EQ(1, tab_strip->active_index());
  EXPECT_EQ(2, tab_strip->GetIndexOfWebContents(first_manage_tab));
  EXPECT_EQ(3, tab_strip->GetIndexOfWebContents(blank_tab));
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest,
                       SignInWebViewOpensLinksInNewTab) {
  controller()->OnDidFetchWalletCookieValue(std::string());
  controller()->OnDidGetWalletItems(
      wallet::GetTestWalletItemsWithRequiredAction(wallet::GAIA_AUTH));

  NavEntryCommittedObserver sign_in_page_observer(
      controller()->SignInUrl(),
      content::NotificationService::AllSources());

  controller()->SignInLinkClicked();

  content::WebContents* sign_in_contents =
      GetViewTester()->GetSignInWebContents();
  ASSERT_TRUE(sign_in_contents);

  sign_in_page_observer.Wait();

  content::OpenURLParams params(GURL("http://google.com"),
                                content::Referrer(),
                                CURRENT_TAB,
                                content::PAGE_TRANSITION_LINK,
                                true);
  params.user_gesture = true;

  int num_tabs = browser()->tab_strip_model()->count();
  sign_in_contents->GetDelegate()->OpenURLFromTab(sign_in_contents, params);
  EXPECT_EQ(num_tabs + 1, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, RefreshOnManageTabClose) {
  ASSERT_TRUE(browser()->is_type_tabbed());

  // GetWalletItems(_, _) is called when controller() is created in SetUp().
  EXPECT_CALL(*controller()->GetTestingWalletClient(), GetWalletItems(_, _));
  controller()->OnDidFetchWalletCookieValue(std::string());
  controller()->OnDidGetWalletItems(
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED));

  content::WebContents* dialog_invoker = controller()->GetWebContents();
  ChromeAutofillClient::FromWebContents(dialog_invoker)
      ->SetDialogControllerForTesting(controller()->AsWeakPtr());

  // Open a new tab by selecting "Manage my shipping details..." in Wallet mode.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  controller()->MenuModelForSection(SECTION_SHIPPING)->ActivatedAt(2);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_NE(dialog_invoker, GetActiveWebContents());

  // Closing the tab opened by "Manage my shipping details..." should refresh
  // the dialog.
  controller()->ClearLastWalletItemsFetchTimestampForTesting();
  EXPECT_CALL(*controller()->GetTestingWalletClient(), GetWalletItems(_, _));
  GetActiveWebContents()->Close();
}

// Changes from Wallet to Autofill and verifies that the combined billing/cc
// sections are showing (or not) at the correct times.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest,
                       ChangingDataSourceShowsCorrectSections) {
  scoped_ptr<AutofillDialogViewTester> view = GetViewTester();
  EXPECT_TRUE(view->IsShowingSection(SECTION_CC));
  EXPECT_TRUE(view->IsShowingSection(SECTION_BILLING));
  EXPECT_FALSE(view->IsShowingSection(SECTION_CC_BILLING));

  // Switch the dialog to paying with Wallet.
  controller()->OnDidFetchWalletCookieValue(std::string());
  controller()->OnDidGetWalletItems(
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED));
  ASSERT_TRUE(controller()->IsPayingWithWallet());

  EXPECT_FALSE(view->IsShowingSection(SECTION_CC));
  EXPECT_FALSE(view->IsShowingSection(SECTION_BILLING));
  EXPECT_TRUE(view->IsShowingSection(SECTION_CC_BILLING));

  // Now switch back to Autofill to ensure this direction works as well.
  ui::MenuModel* account_chooser = controller()->MenuModelForAccountChooser();
  account_chooser->ActivatedAt(account_chooser->GetItemCount() - 1);

  EXPECT_TRUE(view->IsShowingSection(SECTION_CC));
  EXPECT_TRUE(view->IsShowingSection(SECTION_BILLING));
  EXPECT_FALSE(view->IsShowingSection(SECTION_CC_BILLING));
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest,
                       DoesWorkOnHttpWithFlag) {
  net::SpawnedTestServer http_server(
      net::SpawnedTestServer::TYPE_HTTP,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(http_server.Start());
  EXPECT_TRUE(RunTestPage(http_server));
}

// Like the parent test, but doesn't add the --reduce-security-for-testing flag.
class AutofillDialogControllerSecurityTest :
    public AutofillDialogControllerTest {
 public:
  AutofillDialogControllerSecurityTest() {}
  virtual ~AutofillDialogControllerSecurityTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    CHECK(!command_line->HasSwitch(::switches::kReduceSecurityForTesting));
  }

  typedef net::BaseTestServer::SSLOptions SSLOptions;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillDialogControllerSecurityTest);
};

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerSecurityTest,
                       DoesntWorkOnHttp) {
  net::SpawnedTestServer http_server(
      net::SpawnedTestServer::TYPE_HTTP,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(http_server.Start());
  EXPECT_FALSE(RunTestPage(http_server));
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerSecurityTest,
                       DoesWorkOnHttpWithFlags) {
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      SSLOptions(SSLOptions::CERT_OK),
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  EXPECT_TRUE(RunTestPage(https_server));
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerSecurityTest,
                       DoesntWorkOnBrokenHttps) {
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      SSLOptions(SSLOptions::CERT_EXPIRED),
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  EXPECT_FALSE(RunTestPage(https_server));
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest,
                       CountryChangeRebuildsSection) {
  EXPECT_FALSE(SectionHasField(SECTION_BILLING, ADDRESS_BILLING_SORTING_CODE));
  EXPECT_FALSE(SectionHasField(SECTION_SHIPPING, ADDRESS_HOME_SORTING_CODE));

  // Select "Add new shipping address...".
  controller()->MenuModelForSection(SECTION_SHIPPING)->ActivatedAt(1);

  // Add some valid user input that should be preserved when country changes.
  scoped_ptr<AutofillDialogViewTester> view = GetViewTester();
  view->SetTextContentsOfInput(NAME_FULL, ASCIIToUTF16("B. Loblaw"));

  // Change both sections' countries.
  view->SetTextContentsOfInput(ADDRESS_BILLING_COUNTRY, ASCIIToUTF16("France"));
  view->ActivateInput(ADDRESS_BILLING_COUNTRY);
  view->SetTextContentsOfInput(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("Belarus"));
  view->ActivateInput(ADDRESS_HOME_COUNTRY);

  // Verify the name is still there.
  EXPECT_EQ(ASCIIToUTF16("B. Loblaw"), view->GetTextContentsOfInput(NAME_FULL));

  EXPECT_TRUE(SectionHasField(SECTION_BILLING, ADDRESS_BILLING_SORTING_CODE));
  EXPECT_TRUE(SectionHasField(SECTION_SHIPPING, ADDRESS_HOME_SORTING_CODE));
}

// Changing the data source to or from Wallet preserves the shipping country,
// but not the billing country because Wallet only supports US billing
// addresses.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest,
                       ChangingDataSourcePreservesCountry) {
  InitializeControllerWithoutShowing();
  controller()->GetTestingManager()->set_default_country_code("CA");
  controller()->Show();
  CycleRunLoops();

  AutofillProfile verified_profile(test::GetVerifiedProfile());
  controller()->GetTestingManager()->AddTestingProfile(&verified_profile);

  CreditCard verified_card(test::GetVerifiedCreditCard());
  controller()->GetTestingManager()->AddTestingCreditCard(&verified_card);
  EXPECT_FALSE(controller()->IsManuallyEditingSection(SECTION_SHIPPING));

  controller()->OnDidFetchWalletCookieValue(std::string());
  scoped_ptr<wallet::WalletItems> items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  items->AddAccount(wallet::GetTestGaiaAccount());
  items->AddInstrument(wallet::GetTestMaskedInstrument());
  items->AddAddress(wallet::GetTestShippingAddress());
  controller()->OnDidGetWalletItems(items.Pass());

  EXPECT_TRUE(controller()->IsPayingWithWallet());

  // Select "Add new shipping address...".
  controller()->MenuModelForSection(SECTION_SHIPPING)->ActivatedAt(2);
  EXPECT_TRUE(controller()->IsManuallyEditingSection(SECTION_SHIPPING));

  // Default shipping country matches PDM's default, but default billing is
  // always US in Wallet mode.
  scoped_ptr<AutofillDialogViewTester> view = GetViewTester();
  ASSERT_EQ(ASCIIToUTF16("Canada"),
            view->GetTextContentsOfInput(ADDRESS_HOME_COUNTRY));
  ASSERT_EQ(ASCIIToUTF16("United States"),
            view->GetTextContentsOfInput(ADDRESS_BILLING_COUNTRY));

  // Switch the shipping country.
  view->SetTextContentsOfInput(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("Belarus"));
  EXPECT_EQ(ASCIIToUTF16("Belarus"),
            view->GetTextContentsOfInput(ADDRESS_HOME_COUNTRY));
  view->ActivateInput(ADDRESS_HOME_COUNTRY);

  // Switch to using Autofill instead of Wallet.
  ui::MenuModel* account_chooser = controller()->MenuModelForAccountChooser();
  account_chooser->ActivatedAt(account_chooser->GetItemCount() - 1);

  EXPECT_FALSE(controller()->IsPayingWithWallet());

  // Shipping country should have stayed the same.
  EXPECT_EQ(ASCIIToUTF16("Belarus"),
            view->GetTextContentsOfInput(ADDRESS_HOME_COUNTRY));
  ASSERT_TRUE(SectionHasField(SECTION_SHIPPING, ADDRESS_HOME_SORTING_CODE));

  controller()->MenuModelForSection(SECTION_BILLING)->ActivatedAt(1);
  view->SetTextContentsOfInput(ADDRESS_BILLING_COUNTRY,
                               ASCIIToUTF16("Belarus"));
  view->ActivateInput(ADDRESS_BILLING_COUNTRY);
  EXPECT_EQ(ASCIIToUTF16("Belarus"),
            view->GetTextContentsOfInput(ADDRESS_BILLING_COUNTRY));
  ASSERT_TRUE(SectionHasField(SECTION_BILLING, ADDRESS_BILLING_SORTING_CODE));

  // Switch back to Wallet. Country should go back to US.
  account_chooser->ActivatedAt(0);
  EXPECT_EQ(ASCIIToUTF16("United States"),
            view->GetTextContentsOfInput(ADDRESS_BILLING_COUNTRY));
  ASSERT_FALSE(
      SectionHasField(SECTION_CC_BILLING, ADDRESS_BILLING_SORTING_CODE));

  // Make sure shipping is still on Belarus.
  EXPECT_EQ(ASCIIToUTF16("Belarus"),
            view->GetTextContentsOfInput(ADDRESS_HOME_COUNTRY));
  ASSERT_TRUE(SectionHasField(SECTION_SHIPPING, ADDRESS_HOME_SORTING_CODE));
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, AddNewResetsCountry) {
  AutofillProfile verified_profile(test::GetVerifiedProfile());
  controller()->GetTestingManager()->AddTestingProfile(&verified_profile);

  // Select "Add new billing/shipping address...".
  controller()->MenuModelForSection(SECTION_BILLING)->ActivatedAt(1);
  controller()->MenuModelForSection(SECTION_SHIPPING)->ActivatedAt(2);

  scoped_ptr<AutofillDialogViewTester> view = GetViewTester();
  ASSERT_EQ(ASCIIToUTF16("United States"),
            view->GetTextContentsOfInput(ADDRESS_BILLING_COUNTRY));
  ASSERT_EQ(ASCIIToUTF16("United States"),
            view->GetTextContentsOfInput(ADDRESS_HOME_COUNTRY));

  // Switch both billing and shipping countries.
  view->SetTextContentsOfInput(ADDRESS_BILLING_COUNTRY, ASCIIToUTF16("Brazil"));
  view->ActivateInput(ADDRESS_BILLING_COUNTRY);
  view->SetTextContentsOfInput(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("France"));
  view->ActivateInput(ADDRESS_HOME_COUNTRY);

  // Select "Add new billing/shipping address...".
  controller()->MenuModelForSection(SECTION_BILLING)->ActivatedAt(1);
  controller()->MenuModelForSection(SECTION_SHIPPING)->ActivatedAt(2);

  EXPECT_EQ(ASCIIToUTF16("United States"),
            view->GetTextContentsOfInput(ADDRESS_BILLING_COUNTRY));
  EXPECT_EQ(ASCIIToUTF16("United States"),
            view->GetTextContentsOfInput(ADDRESS_HOME_COUNTRY));
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest,
                       FillingFormRebuildsInputs) {
  AutofillProfile full_profile(test::GetFullProfile());
  full_profile.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("DE"));
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);

  // Select "Add new shipping address...".
  controller()->MenuModelForSection(SECTION_SHIPPING)->ActivatedAt(1);

  scoped_ptr<AutofillDialogViewTester> view = GetViewTester();
  ASSERT_EQ(ASCIIToUTF16("United States"),
            view->GetTextContentsOfInput(ADDRESS_BILLING_COUNTRY));
  ASSERT_EQ(ASCIIToUTF16("United States"),
            view->GetTextContentsOfInput(ADDRESS_HOME_COUNTRY));

  const ServerFieldType input_type = EMAIL_ADDRESS;
  base::string16 name = full_profile.GetRawInfo(input_type);
  view->SetTextContentsOfInput(input_type, name.substr(0, name.size() / 2));
  view->ActivateInput(input_type);
  ASSERT_EQ(input_type, controller()->popup_input_type());
  controller()->DidAcceptSuggestion(base::string16(), 0);

  EXPECT_EQ(ASCIIToUTF16("Germany"),
            view->GetTextContentsOfInput(ADDRESS_BILLING_COUNTRY));
  EXPECT_EQ(ASCIIToUTF16("United States"),
            view->GetTextContentsOfInput(ADDRESS_HOME_COUNTRY));
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest,
                       FillingFormPreservesChangedCountry) {
  AutofillProfile full_profile(test::GetFullProfile());
  full_profile.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("DE"));
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);

  // Select "Add new shipping address...".
  controller()->MenuModelForSection(SECTION_SHIPPING)->ActivatedAt(1);

  scoped_ptr<AutofillDialogViewTester> view = GetViewTester();
  view->SetTextContentsOfInput(ADDRESS_BILLING_COUNTRY, ASCIIToUTF16("France"));
  view->ActivateInput(ADDRESS_BILLING_COUNTRY);
  view->SetTextContentsOfInput(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("Belarus"));
  view->ActivateInput(ADDRESS_HOME_COUNTRY);

  base::string16 name = full_profile.GetRawInfo(NAME_FULL);
  view->SetTextContentsOfInput(NAME_FULL, name.substr(0, name.size() / 2));
  view->ActivateInput(NAME_FULL);
  ASSERT_EQ(NAME_FULL, controller()->popup_input_type());
  controller()->DidAcceptSuggestion(base::string16(), 0);

  EXPECT_EQ(ASCIIToUTF16("France"),
            view->GetTextContentsOfInput(ADDRESS_BILLING_COUNTRY));
  EXPECT_EQ(ASCIIToUTF16("Belarus"),
            view->GetTextContentsOfInput(ADDRESS_HOME_COUNTRY));
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, RulesLoaded) {
  // Select "Add new shipping address...".
  controller()->MenuModelForSection(SECTION_SHIPPING)->ActivatedAt(1);
  controller()->set_use_validation(true);

  EXPECT_CALL(*controller()->GetMockValidator(),
              ValidateAddress(CountryCodeMatcher("DE"), _, _)).Times(2).
              WillOnce(Return(AddressValidator::RULES_NOT_READY));

  // Validation should occur on country change and see the rules haven't loaded.
  scoped_ptr<AutofillDialogViewTester> view = GetViewTester();
  view->SetTextContentsOfInput(ADDRESS_HOME_ZIP, ASCIIToUTF16("123"));
  view->SetTextContentsOfInput(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("Germany"));
  view->ActivateInput(ADDRESS_HOME_COUNTRY);

  // Different country loaded, validation should not occur.
  controller()->OnAddressValidationRulesLoaded("FR", true);

  // Relevant country loaded, validation should occur.
  controller()->OnAddressValidationRulesLoaded("DE", true);

  // Relevant country loaded but revalidation already happened, no further
  // validation should occur.
  controller()->OnAddressValidationRulesLoaded("DE", false);

  // Cancelling the dialog causes additional validation to see if the user
  // cancelled with invalid fields, so verify and clear here.
  testing::Mock::VerifyAndClearExpectations(controller()->GetMockValidator());
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest,
                       TransactionAmount) {
  std::string html(
      "<input type='number' step='0.01'"
      "   autocomplete='transaction-amount' value='24'>"
      "<input autocomplete='transaction-currency' value='USD'>"
      "<input autocomplete='cc-csc'>");
  AutofillDialogControllerImpl* controller = SetUpHtmlAndInvoke(html);
  ASSERT_TRUE(controller);

  EXPECT_EQ(ASCIIToUTF16("24"), controller->transaction_amount_);
  EXPECT_EQ(ASCIIToUTF16("USD"), controller->transaction_currency_);
}

}  // namespace autofill
