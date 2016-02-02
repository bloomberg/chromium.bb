// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_dialog_i18n_input.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view_tester.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/data_model_wrapper.h"
#include "chrome/browser/ui/autofill/mock_address_validator.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/risk/proto/fingerprint.pb.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_manager.h"
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
#include "content/public/common/referrer.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#elif defined(OS_MACOSX)
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

class TestAutofillDialogController : public AutofillDialogControllerImpl {
 public:
  TestAutofillDialogController(
      content::WebContents* contents,
      const FormData& form_data,
      scoped_refptr<content::MessageLoopRunner> runner)
      : AutofillDialogControllerImpl(contents,
                                     form_data,
                                     form_data.origin,
                                     base::Bind(&MockCallback)),
        message_loop_runner_(runner),
        use_validation_(false),
        weak_ptr_factory_(this) {
    Profile* profile =
        Profile::FromBrowserContext(contents->GetBrowserContext());
    test_manager_.Init(
        NULL,
        profile->GetPrefs(),
        AccountTrackerServiceFactory::GetForProfile(profile),
        SigninManagerFactory::GetForProfile(profile),
        false);
  }

  ~TestAutofillDialogController() override {}

  GURL FakeSignInUrl() const {
    return GURL(chrome::kChromeUIVersionURL);
  }

  void ViewClosed() override {
    message_loop_runner_->Quit();
    AutofillDialogControllerImpl::ViewClosed();
  }

  base::string16 InputValidityMessage(
      DialogSection section,
      ServerFieldType type,
      const base::string16& value) override {
    if (!use_validation_)
      return base::string16();
    return AutofillDialogControllerImpl::InputValidityMessage(
        section, type, value);
  }

  ValidityMessages InputsAreValid(
      DialogSection section,
      const FieldValueMap& inputs) override {
    if (!use_validation_)
      return ValidityMessages();
    return AutofillDialogControllerImpl::InputsAreValid(section, inputs);
  }

  // Saving to Chrome is tested in AutofillDialogControllerImpl unit tests.
  // TODO(estade): test that the view defaults to saving to Chrome.
  bool ShouldOfferToSaveInChrome() const override {
    return true;
  }

  // Increase visibility for testing.
  using AutofillDialogControllerImpl::view;
  using AutofillDialogControllerImpl::popup_input_type;

  MOCK_METHOD0(LoadRiskFingerprintData, void());

  std::vector<DialogNotification> CurrentNotifications() override {
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

  void set_use_validation(bool use_validation) {
    use_validation_ = use_validation;
  }

  base::WeakPtr<TestAutofillDialogController> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  PersonalDataManager* GetManager() const override {
    return &const_cast<TestAutofillDialogController*>(this)->test_manager_;
  }

  AddressValidator* GetValidator() override {
    return &mock_validator_;
  }

 private:
  TestPersonalDataManager test_manager_;
  testing::NiceMock<MockAddressValidator> mock_validator_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  bool use_validation_;

  // A list of notifications to show in the notification area of the dialog.
  // This is used to control what |CurrentNotifications()| returns for testing.
  std::vector<DialogNotification> notifications_;

  // Allows generation of WeakPtrs, so controller liveness can be tested.
  base::WeakPtrFactory<TestAutofillDialogController> weak_ptr_factory_;

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

  ~NavEntryCommittedObserver() override {}

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
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
  ~AutofillDialogControllerTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(::switches::kReduceSecurityForTesting);
  }

  void SetUpOnMainThread() override {
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

    FormFieldData field;
    field.autocomplete_attribute = "shipping tel";
    form.fields.push_back(field);

    FormFieldData cc;
    cc.autocomplete_attribute = "cc-number";
    form.fields.push_back(cc);

    message_loop_runner_ = new content::MessageLoopRunner;
    controller_ = new TestAutofillDialogController(
        GetActiveWebContents(),
        form,
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
    return AutofillDialogViewTester::For(controller()->view());
  }

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
  bool RunTestPage(const net::EmbeddedTestServer& server) {
    GURL url = server.GetURL("/request_autocomplete/test_page.html");
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

  void RunTestPageInIframe(const net::EmbeddedTestServer& server) {
    InitializeDOMMessageQueue();
    GURL iframe_url = server.GetURL("/request_autocomplete/test_page.html");

    ui_test_utils::NavigateToURL(
        browser(), GURL(std::string("data:text/html,") +
        "<!doctype html>"
        "<html>"
          "<body>"
            "<iframe style='position: fixed;"
                           "height: 100%;"
                           "width: 100%;'"
                "id='racFrame'></iframe>"
            "<script>"
              "function send(msg) {"
                "domAutomationController.setAutomationId(0);"
                "domAutomationController.send(msg);"
              "}"
              "var racFrame = document.getElementById('racFrame');"
              "racFrame.onload = function() {"
                "send('iframe loaded');"
              "};"
              "racFrame.src = \"" + iframe_url.spec() + "\";"
              "function navigateFrame() {"
                "racFrame.src = 'about:blank';"
              "}"
            "</script>"
          "</body>"
        "</html>"));

    ChromeAutofillClient* client =
        ChromeAutofillClient::FromWebContents(GetActiveWebContents());
    ExpectDomMessage("iframe loaded");
    EXPECT_FALSE(client->GetDialogControllerForTesting());
    InitiateDialog();
    EXPECT_TRUE(client->GetDialogControllerForTesting());
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
    InitializeDOMMessageQueue();
    // Triggers the onclick handler which invokes requestAutocomplete().
    content::WebContents* contents = GetActiveWebContents();
    content::SimulateMouseClick(contents, 0, blink::WebMouseEvent::ButtonLeft);
    ExpectDomMessage("clicked");
  }

  void InitializeDOMMessageQueue() {
    dom_message_queue_.reset(new content::DOMMessageQueue);
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

 private:
  void WaitForWebDB() {
    content::RunAllPendingInMessageLoop(content::BrowserThread::DB);
  }

  TestAutofillDialogController* controller_;  // Weak reference.
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  scoped_ptr<content::DOMMessageQueue> dom_message_queue_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDialogControllerTest);
};

// Submit the form data.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, Submit) {
  base::HistogramTester histogram;
  AddCreditcardToProfile(controller()->profile(),
                         test::GetVerifiedCreditCard());
  AddAutofillProfileToProfile(controller()->profile(),
                              test::GetVerifiedProfile());
  scoped_ptr<AutofillDialogViewTester> view = AutofillDialogViewTester::For(
      controller()->view());
  view->SetTextContentsOfSuggestionInput(SECTION_CC, ASCIIToUTF16("123"));
  GetViewTester()->SubmitForTesting();
  RunMessageLoop();

  histogram.ExpectTotalCount("RequestAutocomplete.UiDuration.Submit", 1);
  histogram.ExpectTotalCount("RequestAutocomplete.UiDuration.Cancel", 0);
}

// Cancel out of the dialog.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, Cancel) {
  base::HistogramTester histogram;
  GetViewTester()->CancelForTesting();
  RunMessageLoop();

  histogram.ExpectTotalCount("RequestAutocomplete.UiDuration.Submit", 0);
  histogram.ExpectTotalCount("RequestAutocomplete.UiDuration.Cancel", 1);
  histogram.ExpectUniqueSample(
      "RequestAutocomplete.DismissalState",
      AutofillMetrics::DIALOG_CANCELED_NO_INVALID_FIELDS, 1);
}

// Take some other action that dismisses the dialog.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, Hide) {
  base::HistogramTester histogram;
  controller()->Hide();

  RunMessageLoop();

  histogram.ExpectTotalCount("RequestAutocomplete.UiDuration.Submit", 0);
  histogram.ExpectTotalCount("RequestAutocomplete.UiDuration.Cancel", 1);
  histogram.ExpectUniqueSample(
      "RequestAutocomplete.DismissalState",
      AutofillMetrics::DIALOG_CANCELED_NO_INVALID_FIELDS, 1);
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, CancelWithSuggestions) {
  base::HistogramTester histogram;

  CreditCard card(test::GetVerifiedCreditCard());
  controller()->GetTestingManager()->AddTestingCreditCard(&card);
  AutofillProfile profile(test::GetVerifiedProfile());
  controller()->GetTestingManager()->AddTestingProfile(&profile);

  EXPECT_FALSE(controller()->IsManuallyEditingSection(SECTION_CC));
  EXPECT_FALSE(controller()->IsManuallyEditingSection(SECTION_BILLING));
  EXPECT_FALSE(controller()->IsManuallyEditingSection(SECTION_SHIPPING));

  GetViewTester()->CancelForTesting();
  RunMessageLoop();

  histogram.ExpectTotalCount("RequestAutocomplete.UiDuration.Submit", 0);
  histogram.ExpectTotalCount("RequestAutocomplete.UiDuration.Cancel", 1);
  histogram.ExpectUniqueSample("RequestAutocomplete.DismissalState",
                               AutofillMetrics::DIALOG_CANCELED_NO_EDITS, 1);
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, AcceptWithSuggestions) {
  base::HistogramTester histogram;
  CreditCard card(test::GetVerifiedCreditCard());
  controller()->GetTestingManager()->AddTestingCreditCard(&card);
  AutofillProfile profile(test::GetVerifiedProfile());
  controller()->GetTestingManager()->AddTestingProfile(&profile);

  EXPECT_FALSE(controller()->IsManuallyEditingSection(SECTION_CC));
  EXPECT_FALSE(controller()->IsManuallyEditingSection(SECTION_BILLING));
  EXPECT_FALSE(controller()->IsManuallyEditingSection(SECTION_SHIPPING));

  GetViewTester()->SubmitForTesting();
  RunMessageLoop();

  histogram.ExpectTotalCount("RequestAutocomplete.UiDuration.Submit", 1);
  histogram.ExpectTotalCount("RequestAutocomplete.UiDuration.Cancel", 0);
  histogram.ExpectUniqueSample(
      "RequestAutocomplete.DismissalState",
      AutofillMetrics::DIALOG_ACCEPTED_EXISTING_AUTOFILL_DATA, 1);
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
  controller()->DidAcceptSuggestion(base::string16(), 0, 1);

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
  controller()->DidAcceptSuggestion(base::string16(), 0, 1);

  for (size_t i = 0; i < inputs.size(); ++i) {
    if (controller()->ComboboxModelForAutofillType(inputs[i].type))
      continue;
    EXPECT_EQ(expectations[i], view->GetTextContentsOfInput(inputs[i].type));
  }

  base::HistogramTester histogram;
  view->SubmitForTesting();
  histogram.ExpectUniqueSample(
      "RequestAutocomplete.DismissalState",
      AutofillMetrics::DIALOG_ACCEPTED_SAVE_TO_AUTOFILL, 1);
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
  controller()->DidAcceptSuggestion(base::string16(), 0, 1);

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
  controller()->DidAcceptSuggestion(base::string16(), 0, 1);

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
  controller()->DidAcceptSuggestion(base::string16(), 0, 1);

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

  base::HistogramTester histogram;
  controller()->Hide();
  histogram.ExpectUniqueSample(
      "RequestAutocomplete.DismissalState",
      AutofillMetrics::DIALOG_CANCELED_WITH_INVALID_FIELDS, 1);
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
      AutofillDialogViewTester::For(controller->view());
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
      AutofillDialogViewTester::For(controller->view());
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
  AutofillDialogViewTester::For(controller->view())->
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

  AutofillDialogViewTester::For(controller->view())->
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
      AutofillDialogViewTester::For(controller->view());
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

// Flaky on Win7 (http://crbug.com/446432)
#if defined(OS_WIN)
#define MAYBE_TabOpensToJustRight DISABLED_TabOpensToJustRight
#else
#define MAYBE_TabOpensToJustRight TabOpensToJustRight
#endif
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest,
                       MAYBE_TabOpensToJustRight) {
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
                       DoesWorkOnHttpWithFlag) {
  net::EmbeddedTestServer http_server;
  http_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(http_server.Start());
  EXPECT_TRUE(RunTestPage(http_server));
}

// Like the parent test, but doesn't add the --reduce-security-for-testing flag.
class AutofillDialogControllerSecurityTest :
    public AutofillDialogControllerTest {
 public:
  AutofillDialogControllerSecurityTest() {}
  ~AutofillDialogControllerSecurityTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    CHECK(!command_line->HasSwitch(::switches::kReduceSecurityForTesting));
  }

  typedef net::BaseTestServer::SSLOptions SSLOptions;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillDialogControllerSecurityTest);
};

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerSecurityTest,
                       DoesntWorkOnHttp) {
  net::EmbeddedTestServer http_server;
  http_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(http_server.Start());
  EXPECT_FALSE(RunTestPage(http_server));
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerSecurityTest,
                       DoesWorkOnHttpWithFlags) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());
  EXPECT_TRUE(RunTestPage(https_server));
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerSecurityTest,
                       DISABLED_DoesntWorkOnBrokenHttps) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
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
  controller()->DidAcceptSuggestion(base::string16(), 0, 1);

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
  controller()->DidAcceptSuggestion(base::string16(), 0, 1);

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

// Same as above, plus readonly attribute.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest,
                       TransactionAmountReadonly) {
  std::string html(
      "<input type='number' step='0.01'"
      "   autocomplete='transaction-amount' value='24' readonly>"
      "<input autocomplete='transaction-currency' value='USD' readonly>"
      "<input autocomplete='cc-csc'>");
  AutofillDialogControllerImpl* controller = SetUpHtmlAndInvoke(html);
  ASSERT_TRUE(controller);

  EXPECT_EQ(ASCIIToUTF16("24"), controller->transaction_amount_);
  EXPECT_EQ(ASCIIToUTF16("USD"), controller->transaction_currency_);
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, HideOnNavigate) {
  base::WeakPtr<TestAutofillDialogController> weak_ptr =
      controller()->AsWeakPtr();
  EXPECT_TRUE(weak_ptr.get());

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  EXPECT_FALSE(weak_ptr.get());
}

// Tests that the rAc dialog hides when the main frame is navigated, even if
// it was invoked from a child frame.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, HideOnNavigateMainFrame) {
  net::EmbeddedTestServer http_server;
  http_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(http_server.Start());
  RunTestPageInIframe(http_server);

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  ChromeAutofillClient* client =
      ChromeAutofillClient::FromWebContents(GetActiveWebContents());
  EXPECT_FALSE(client->GetDialogControllerForTesting());
}

// Tests that the rAc dialog hides when the iframe it's in is navigated.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, HideOnNavigateIframe) {
  net::EmbeddedTestServer http_server;
  http_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(http_server.Start());
  RunTestPageInIframe(http_server);

  std::string unused;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(GetRenderViewHost(),
                                                     "navigateFrame();",
                                                     &unused));
  ExpectDomMessage("iframe loaded");
  ChromeAutofillClient* client =
      ChromeAutofillClient::FromWebContents(GetActiveWebContents());
  EXPECT_FALSE(client->GetDialogControllerForTesting());
}

}  // namespace autofill
