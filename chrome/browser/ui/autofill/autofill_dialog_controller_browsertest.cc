// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "chrome/browser/ui/autofill/data_model_wrapper.h"
#include "chrome/browser/ui/autofill/tab_autofill_manager_delegate.h"
#include "chrome/browser/ui/autofill/testable_autofill_dialog_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/wallet/mock_wallet_client.h"
#include "components/autofill/content/browser/wallet/wallet_test_util.h"
#include "components/autofill/core/browser/autofill_common_test.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace autofill {

namespace {

void MockCallback(const FormStructure*) {}

class MockAutofillMetrics : public AutofillMetrics {
 public:
  MockAutofillMetrics()
      : dialog_dismissal_action_(
            static_cast<AutofillMetrics::DialogDismissalAction>(-1)) {}
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
  AutofillMetrics::DialogDismissalAction dialog_dismissal_action_;

  DISALLOW_COPY_AND_ASSIGN(MockAutofillMetrics);
};

class TestAutofillDialogController : public AutofillDialogControllerImpl {
 public:
  TestAutofillDialogController(content::WebContents* contents,
                               const FormData& form_data,
                               const AutofillMetrics& metric_logger,
                               scoped_refptr<content::MessageLoopRunner> runner)
      : AutofillDialogControllerImpl(contents,
                                     form_data,
                                     GURL(),
                                     base::Bind(&MockCallback)),
        metric_logger_(metric_logger),
        mock_wallet_client_(
            Profile::FromBrowserContext(contents->GetBrowserContext())->
                GetRequestContext(), this),
        message_loop_runner_(runner),
        use_validation_(false),
        weak_ptr_factory_(this) {}

  virtual ~TestAutofillDialogController() {}

  virtual void ViewClosed() OVERRIDE {
    message_loop_runner_->Quit();
    AutofillDialogControllerImpl::ViewClosed();
  }

  virtual string16 InputValidityMessage(
      DialogSection section,
      ServerFieldType type,
      const string16& value) OVERRIDE {
    if (!use_validation_)
      return string16();
    return AutofillDialogControllerImpl::InputValidityMessage(
        section, type, value);
  }

  virtual ValidityMessages InputsAreValid(
      DialogSection section,
      const DetailOutputMap& inputs) OVERRIDE {
    if (!use_validation_)
      return ValidityMessages();
    return AutofillDialogControllerImpl::InputsAreValid(section, inputs);
  }

  // Saving to Chrome is tested in AutofillDialogControllerImpl unit tests.
  // TODO(estade): test that the view defaults to saving to Chrome.
  virtual bool ShouldOfferToSaveInChrome() const OVERRIDE {
    return false;
  }

  // Increase visibility for testing.
  using AutofillDialogControllerImpl::view;
  using AutofillDialogControllerImpl::input_showing_popup;

  virtual std::vector<DialogNotification> CurrentNotifications() OVERRIDE {
    return notifications_;
  }

  void set_notifications(const std::vector<DialogNotification>& notifications) {
    notifications_ = notifications;
  }

  TestPersonalDataManager* GetTestingManager() {
    return &test_manager_;
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
  virtual PersonalDataManager* GetManager() OVERRIDE {
    return &test_manager_;
  }

  virtual wallet::WalletClient* GetWalletClient() OVERRIDE {
    return &mock_wallet_client_;
  }

 private:
  // To specify our own metric logger.
  virtual const AutofillMetrics& GetMetricLogger() const OVERRIDE {
    return metric_logger_;
  }

  const AutofillMetrics& metric_logger_;
  TestPersonalDataManager test_manager_;
  testing::NiceMock<wallet::MockWalletClient> mock_wallet_client_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  bool use_validation_;

  // A list of notifications to show in the notification area of the dialog.
  // This is used to control what |CurrentNotifications()| returns for testing.
  std::vector<DialogNotification> notifications_;

  // Allows generation of WeakPtrs, so controller liveness can be tested.
  base::WeakPtrFactory<TestAutofillDialogController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillDialogController);
};

}  // namespace

class AutofillDialogControllerTest : public InProcessBrowserTest {
 public:
  AutofillDialogControllerTest() {}
  virtual ~AutofillDialogControllerTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    autofill::test::DisableSystemServices(browser()->profile());
    InitializeController();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
#if defined(OS_MACOSX)
    // OSX support for requestAutocomplete is still hidden behind a switch.
    // Pending resolution of http://crbug.com/157274
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableInteractiveAutocomplete);
#endif
  }

  void InitializeController() {
    FormData form;
    form.name = ASCIIToUTF16("TestForm");
    form.method = ASCIIToUTF16("POST");
    form.origin = GURL("http://example.com/form.html");
    form.action = GURL("http://example.com/submit.html");
    form.user_submitted = true;

    FormFieldData field;
    field.autocomplete_attribute = "shipping tel";
    form.fields.push_back(field);

    message_loop_runner_ = new content::MessageLoopRunner;
    controller_ = new TestAutofillDialogController(
        GetActiveWebContents(),
        form,
        metric_logger_,
        message_loop_runner_);
    controller_->Show();
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
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
    TabAutofillManagerDelegate* delegate =
        TabAutofillManagerDelegate::FromWebContents(contents);
    DCHECK(!delegate->GetDialogControllerForTesting());

    ui_test_utils::NavigateToURL(
        browser(), GURL(std::string("data:text/html,") +
        "<!doctype html>"
        "<html>"
          "<body>"
            "<form>" + form_inner_html + "</form>"
            "<script>"
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
                "document.forms[0].requestAutocomplete();"
                "send('clicked');"
              "};"
            "</script>"
          "</body>"
        "</html>"));
    content::WaitForLoadStop(contents);

    dom_message_queue_.reset(new content::DOMMessageQueue);

    // Triggers the onclick handler which invokes requestAutocomplete().
    content::SimulateMouseClick(contents, 0, WebKit::WebMouseEvent::ButtonLeft);
    ExpectDomMessage("clicked");

    AutofillDialogControllerImpl* controller =
        static_cast<AutofillDialogControllerImpl*>(
            delegate->GetDialogControllerForTesting());
    DCHECK(controller);
    return controller;
  }

  // Wait for a message from the DOM automation controller (from JS in the
  // page). Requires |SetUpHtmlAndInvoke()| be called first.
  void ExpectDomMessage(const std::string& expected) {
    std::string message;
    ASSERT_TRUE(dom_message_queue_->WaitForMessage(&message));
    dom_message_queue_->ClearQueue();
    EXPECT_EQ("\"" + expected + "\"", message);
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

  MockAutofillMetrics metric_logger_;
  TestAutofillDialogController* controller_;  // Weak reference.
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  scoped_ptr<content::DOMMessageQueue> dom_message_queue_;
  DISALLOW_COPY_AND_ASSIGN(AutofillDialogControllerTest);
};

#if defined(TOOLKIT_VIEWS) || defined(OS_MACOSX)
// Submit the form data.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, Submit) {
  controller()->GetTestableView()->SubmitForTesting();

  RunMessageLoop();

  EXPECT_EQ(AutofillMetrics::DIALOG_ACCEPTED,
            metric_logger().dialog_dismissal_action());
}

// Cancel out of the dialog.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, Cancel) {
  controller()->GetTestableView()->CancelForTesting();

  RunMessageLoop();

  EXPECT_EQ(AutofillMetrics::DIALOG_CANCELED,
            metric_logger().dialog_dismissal_action());
}

// Take some other action that dismisses the dialog.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, Hide) {
  controller()->Hide();

  RunMessageLoop();

  EXPECT_EQ(AutofillMetrics::DIALOG_CANCELED,
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
  controller()->GetTestableView()->CancelForTesting();

  RunMessageLoop();

  EXPECT_EQ(AutofillMetrics::DIALOG_CANCELED,
            metric_logger().dialog_dismissal_action());
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, FillInputFromAutofill) {
  AutofillProfile full_profile(test::GetFullProfile());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);

  const DetailInputs& inputs =
      controller()->RequestedFieldsForSection(SECTION_SHIPPING);
  const DetailInput& triggering_input = inputs[0];
  string16 value = full_profile.GetRawInfo(triggering_input.type);
  TestableAutofillDialogView* view = controller()->GetTestableView();
  view->SetTextContentsOfInput(triggering_input,
                               value.substr(0, value.size() / 2));
  view->ActivateInput(triggering_input);

  ASSERT_EQ(&triggering_input, controller()->input_showing_popup());
  controller()->DidAcceptSuggestion(string16(), 0);

  // All inputs should be filled.
  AutofillProfileWrapper wrapper(&full_profile);
  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_EQ(wrapper.GetInfo(AutofillType(inputs[i].type)),
              view->GetTextContentsOfInput(inputs[i]));
  }

  // Now simulate some user edits and try again.
  std::vector<string16> expectations;
  for (size_t i = 0; i < inputs.size(); ++i) {
    string16 users_input = i % 2 == 0 ? string16() : ASCIIToUTF16("dummy");
    view->SetTextContentsOfInput(inputs[i], users_input);
    // Empty inputs should be filled, others should be left alone.
    string16 expectation =
        &inputs[i] == &triggering_input || users_input.empty() ?
        wrapper.GetInfo(AutofillType(inputs[i].type)) :
        users_input;
    expectations.push_back(expectation);
  }

  view->SetTextContentsOfInput(triggering_input,
                               value.substr(0, value.size() / 2));
  view->ActivateInput(triggering_input);
  ASSERT_EQ(&triggering_input, controller()->input_showing_popup());
  controller()->DidAcceptSuggestion(string16(), 0);

  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_EQ(expectations[i], view->GetTextContentsOfInput(inputs[i]));
  }
}

// This test makes sure that picking a profile variant in the Autofill
// popup works as expected.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest,
                       FillInputFromAutofillVariant) {
  AutofillProfile full_profile(test::GetFullProfile());

  // Set up some variant data.
  std::vector<string16> names;
  names.push_back(ASCIIToUTF16("John Doe"));
  names.push_back(ASCIIToUTF16("Jane Doe"));
  full_profile.SetRawMultiInfo(NAME_FULL, names);
  std::vector<string16> emails;
  emails.push_back(ASCIIToUTF16("user@example.com"));
  emails.push_back(ASCIIToUTF16("admin@example.com"));
  full_profile.SetRawMultiInfo(EMAIL_ADDRESS, emails);
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);

  const DetailInputs& inputs =
      controller()->RequestedFieldsForSection(SECTION_BILLING);
  const DetailInput& triggering_input = inputs[0];
  EXPECT_EQ(NAME_BILLING_FULL, triggering_input.type);
  TestableAutofillDialogView* view = controller()->GetTestableView();
  view->ActivateInput(triggering_input);

  ASSERT_EQ(&triggering_input, controller()->input_showing_popup());

  // Choose the variant suggestion.
  controller()->DidAcceptSuggestion(string16(), 1);

  // All inputs should be filled.
  AutofillProfileWrapper wrapper(
      &full_profile, AutofillType(NAME_BILLING_FULL), 1);
  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_EQ(wrapper.GetInfo(AutofillType(inputs[i].type)),
              view->GetTextContentsOfInput(inputs[i]));
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
// TODO(groby): Enable this test on mac once AutofillDialogCocoa handles
// comboboxes for GetTextContentsForInput. http://crbug.com/270205
#if defined(OS_MACOSX) || defined(OS_WIN)
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
  const DetailInput& triggering_input = inputs[0];
  string16 value = card1.GetRawInfo(triggering_input.type);
  TestableAutofillDialogView* view = controller()->GetTestableView();
  view->SetTextContentsOfInput(triggering_input,
                               value.substr(0, value.size() / 2));
  view->ActivateInput(triggering_input);

  ASSERT_EQ(&triggering_input, controller()->input_showing_popup());
  controller()->DidAcceptSuggestion(string16(), 0);

  // All inputs should be filled.
  AutofillCreditCardWrapper wrapper1(&card1);
  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_EQ(wrapper1.GetInfo(AutofillType(inputs[i].type)),
              view->GetTextContentsOfInput(inputs[i]));
  }

  // Try again with different data. Only expiration date and the triggering
  // input should be overwritten.
  value = card2.GetRawInfo(triggering_input.type);
  view->SetTextContentsOfInput(triggering_input,
                               value.substr(0, value.size() / 2));
  view->ActivateInput(triggering_input);
  ASSERT_EQ(&triggering_input, controller()->input_showing_popup());
  controller()->DidAcceptSuggestion(string16(), 0);

  AutofillCreditCardWrapper wrapper2(&card2);
  for (size_t i = 0; i < inputs.size(); ++i) {
    const DetailInput& input = inputs[i];
    if (&input == &triggering_input ||
        input.type == CREDIT_CARD_EXP_MONTH ||
        input.type == CREDIT_CARD_EXP_4_DIGIT_YEAR) {
      EXPECT_EQ(wrapper2.GetInfo(AutofillType(input.type)),
                view->GetTextContentsOfInput(input));
    } else if (input.type == CREDIT_CARD_VERIFICATION_CODE) {
      EXPECT_TRUE(view->GetTextContentsOfInput(input).empty());
    } else {
      EXPECT_EQ(wrapper1.GetInfo(AutofillType(input.type)),
                view->GetTextContentsOfInput(input));
    }
  }

  // Now fill from a profile. It should not overwrite any CC info.
  const DetailInputs& billing_inputs =
      controller()->RequestedFieldsForSection(SECTION_BILLING);
  const DetailInput& billing_triggering_input = billing_inputs[0];
  value = full_profile.GetRawInfo(triggering_input.type);
  view->SetTextContentsOfInput(billing_triggering_input,
                               value.substr(0, value.size() / 2));
  view->ActivateInput(billing_triggering_input);

  ASSERT_EQ(&billing_triggering_input, controller()->input_showing_popup());
  controller()->DidAcceptSuggestion(string16(), 0);

  for (size_t i = 0; i < inputs.size(); ++i) {
    const DetailInput& input = inputs[i];
    if (&input == &triggering_input ||
        input.type == CREDIT_CARD_EXP_MONTH ||
        input.type == CREDIT_CARD_EXP_4_DIGIT_YEAR) {
      EXPECT_EQ(wrapper2.GetInfo(AutofillType(input.type)),
                view->GetTextContentsOfInput(input));
    } else if (input.type == CREDIT_CARD_VERIFICATION_CODE) {
      EXPECT_TRUE(view->GetTextContentsOfInput(input).empty());
    } else {
      EXPECT_EQ(wrapper1.GetInfo(AutofillType(input.type)),
                view->GetTextContentsOfInput(input));
    }
  }
}

// Tests that credit card number is disabled while editing a Wallet instrument.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, WalletCreditCardDisabled) {
  controller()->OnUserNameFetchSuccess("user@example.com");
  controller()->OnDidFetchWalletCookieValue(std::string());

  scoped_ptr<wallet::WalletItems> wallet_items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  // An expired card will be forced into edit mode.
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrumentWithDetails(
      "instrument_id",
      wallet::GetTestAddress(),
      wallet::WalletItems::MaskedInstrument::VISA,
      wallet::WalletItems::MaskedInstrument::EXPIRED));
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  const DetailInputs& edit_inputs =
      controller()->RequestedFieldsForSection(SECTION_CC_BILLING);
  size_t i;
  for (i = 0; i < edit_inputs.size(); ++i) {
    if (edit_inputs[i].type == CREDIT_CARD_NUMBER) {
      EXPECT_FALSE(edit_inputs[i].editable);
      break;
    }
  }
  ASSERT_LT(i, edit_inputs.size());

  // Select "Add new billing info..." while using Wallet.
  ui::MenuModel* model = controller()->MenuModelForSection(SECTION_CC_BILLING);
  model->ActivatedAt(model->GetItemCount() - 2);

  const DetailInputs& add_inputs =
      controller()->RequestedFieldsForSection(SECTION_CC_BILLING);
  for (i = 0; i < add_inputs.size(); ++i) {
    if (add_inputs[i].type == CREDIT_CARD_NUMBER) {
      EXPECT_TRUE(add_inputs[i].editable);
      break;
    }
  }
  ASSERT_LT(i, add_inputs.size());
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
  const gfx::Size no_notification_size =
      controller()->GetTestableView()->GetSize();
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
            controller()->GetTestableView()->GetSize().width());
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, AutocompleteEvent) {
  AutofillDialogControllerImpl* controller =
      SetUpHtmlAndInvoke("<input autocomplete='cc-name'>");

  AddCreditcardToProfile(controller->profile(), test::GetVerifiedCreditCard());
  AddAutofillProfileToProfile(controller->profile(),
                              test::GetVerifiedProfile());

  TestableAutofillDialogView* view = controller->GetTestableView();
  view->SetTextContentsOfSuggestionInput(SECTION_CC, ASCIIToUTF16("123"));
  view->SubmitForTesting();
  ExpectDomMessage("success");
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest,
                       AutocompleteErrorEventReasonInvalid) {
  AutofillDialogControllerImpl* controller =
      SetUpHtmlAndInvoke("<input autocomplete='cc-name' pattern='.*zebra.*'>");

  const CreditCard& credit_card = test::GetVerifiedCreditCard();
  ASSERT_TRUE(
    credit_card.GetRawInfo(CREDIT_CARD_NAME).find(ASCIIToUTF16("zebra")) ==
        base::string16::npos);
  AddCreditcardToProfile(controller->profile(), credit_card);
  AddAutofillProfileToProfile(controller->profile(),
                              test::GetVerifiedProfile());

  TestableAutofillDialogView* view = controller->GetTestableView();
  view->SetTextContentsOfSuggestionInput(SECTION_CC, ASCIIToUTF16("123"));
  view->SubmitForTesting();
  ExpectDomMessage("error: invalid");
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest,
                       AutocompleteErrorEventReasonCancel) {
  SetUpHtmlAndInvoke("<input autocomplete='cc-name'>")->GetTestableView()->
      CancelForTesting();
  ExpectDomMessage("error: cancel");
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, NoCvcSegfault) {
  controller()->set_use_validation(true);

  CreditCard credit_card(test::GetVerifiedCreditCard());
  controller()->GetTestingManager()->AddTestingCreditCard(&credit_card);
  EXPECT_FALSE(controller()->IsEditingExistingData(SECTION_CC));

  ASSERT_NO_FATAL_FAILURE(
      controller()->GetTestableView()->SubmitForTesting());
}

// Flaky on Win7, WinXP, and Win Aura.  http://crbug.com/270314.
#if defined(OS_WIN)
#define MAYBE_PreservedSections DISABLED_PreservedSections
#else
#define MAYBE_PreservedSections PreservedSections
#endif
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, MAYBE_PreservedSections) {
  controller()->set_use_validation(true);

  // Set up some Autofill state.
  CreditCard credit_card(test::GetVerifiedCreditCard());
  controller()->GetTestingManager()->AddTestingCreditCard(&credit_card);

  AutofillProfile profile(test::GetVerifiedProfile());
  controller()->GetTestingManager()->AddTestingProfile(&profile);

  EXPECT_TRUE(controller()->SectionIsActive(SECTION_CC));
  EXPECT_TRUE(controller()->SectionIsActive(SECTION_BILLING));
  EXPECT_FALSE(controller()->SectionIsActive(SECTION_CC_BILLING));
  EXPECT_TRUE(controller()->SectionIsActive(SECTION_SHIPPING));

  EXPECT_FALSE(controller()->IsManuallyEditingSection(SECTION_CC));
  EXPECT_FALSE(controller()->IsManuallyEditingSection(SECTION_BILLING));
  EXPECT_FALSE(controller()->IsManuallyEditingSection(SECTION_SHIPPING));

  // Set up some Wallet state.
  controller()->OnUserNameFetchSuccess("user@example.com");
  controller()->OnDidFetchWalletCookieValue(std::string());
  controller()->OnDidGetWalletItems(
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED));

  ui::MenuModel* account_chooser = controller()->MenuModelForAccountChooser();
  ASSERT_TRUE(account_chooser->IsItemCheckedAt(0));

  // Check that the view's in the state we expect before starting to simulate
  // user input.
  EXPECT_FALSE(controller()->SectionIsActive(SECTION_CC));
  EXPECT_FALSE(controller()->SectionIsActive(SECTION_BILLING));
  EXPECT_TRUE(controller()->SectionIsActive(SECTION_CC_BILLING));
  EXPECT_TRUE(controller()->SectionIsActive(SECTION_SHIPPING));

  EXPECT_TRUE(controller()->IsManuallyEditingSection(SECTION_CC_BILLING));

  // Create some valid inputted billing data.
  const DetailInput& cc_number =
      controller()->RequestedFieldsForSection(SECTION_CC_BILLING)[0];
  EXPECT_EQ(CREDIT_CARD_NUMBER, cc_number.type);
  TestableAutofillDialogView* view = controller()->GetTestableView();
  view->SetTextContentsOfInput(cc_number, ASCIIToUTF16("4111111111111111"));

  // Select "Add new shipping info..." from suggestions menu.
  ui::MenuModel* shipping_model =
      controller()->MenuModelForSection(SECTION_SHIPPING);
  shipping_model->ActivatedAt(shipping_model->GetItemCount() - 2);

  EXPECT_TRUE(controller()->IsManuallyEditingSection(SECTION_SHIPPING));

  // Create some invalid, manually inputted shipping data.
  const DetailInput& shipping_zip =
      controller()->RequestedFieldsForSection(SECTION_SHIPPING)[5];
  ASSERT_EQ(ADDRESS_HOME_ZIP, shipping_zip.type);
  view->SetTextContentsOfInput(shipping_zip, ASCIIToUTF16("shipping zip"));

  // Switch to using Autofill.
  account_chooser->ActivatedAt(1);

  // Check that appropriate sections are preserved and in manually editing mode
  // (or disabled, in the case of the combined cc + billing section).
  EXPECT_TRUE(controller()->SectionIsActive(SECTION_CC));
  EXPECT_TRUE(controller()->SectionIsActive(SECTION_BILLING));
  EXPECT_FALSE(controller()->SectionIsActive(SECTION_CC_BILLING));
  EXPECT_TRUE(controller()->SectionIsActive(SECTION_SHIPPING));

  EXPECT_TRUE(controller()->IsManuallyEditingSection(SECTION_CC));
  EXPECT_FALSE(controller()->IsManuallyEditingSection(SECTION_BILLING));
  EXPECT_FALSE(controller()->IsManuallyEditingSection(SECTION_SHIPPING));

  const DetailInput& new_cc_number =
      controller()->RequestedFieldsForSection(SECTION_CC).front();
  EXPECT_EQ(cc_number.type, new_cc_number.type);
  EXPECT_EQ(ASCIIToUTF16("4111111111111111"),
            view->GetTextContentsOfInput(new_cc_number));

  EXPECT_NE(ASCIIToUTF16("shipping name"),
            view->GetTextContentsOfInput(shipping_zip));
}
#endif  // defined(TOOLKIT_VIEWS) || defined(OS_MACOSX)

}  // namespace autofill
