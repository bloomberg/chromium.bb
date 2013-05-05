// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "chrome/browser/ui/autofill/data_model_wrapper.h"
#include "chrome/browser/ui/autofill/testable_autofill_dialog_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/browser/autofill_common_test.h"
#include "components/autofill/browser/autofill_metrics.h"
#include "components/autofill/browser/test_personal_data_manager.h"
#include "components/autofill/common/form_data.h"
#include "components/autofill/common/form_field_data.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

void MockCallback(const FormStructure*, const std::string&) {}

class MockAutofillMetrics : public AutofillMetrics {
 public:
  MockAutofillMetrics()
      : dialog_type_(static_cast<DialogType>(-1)),
        dialog_dismissal_action_(
            static_cast<AutofillMetrics::DialogDismissalAction>(-1)),
        autocheckout_status_(
            static_cast<AutofillMetrics::AutocheckoutCompletionStatus>(-1)) {}
  virtual ~MockAutofillMetrics() {}

  // AutofillMetrics:
  virtual void LogAutocheckoutDuration(
      const base::TimeDelta& duration,
      AutocheckoutCompletionStatus status) const OVERRIDE {
    // Ignore constness for testing.
    MockAutofillMetrics* mutable_this = const_cast<MockAutofillMetrics*>(this);
    mutable_this->autocheckout_status_ = status;
  }

  virtual void LogDialogUiDuration(
      const base::TimeDelta& duration,
      DialogType dialog_type,
      DialogDismissalAction dismissal_action) const OVERRIDE {
    // Ignore constness for testing.
    MockAutofillMetrics* mutable_this = const_cast<MockAutofillMetrics*>(this);
    mutable_this->dialog_type_ = dialog_type;
    mutable_this->dialog_dismissal_action_ = dismissal_action;
  }

  DialogType dialog_type() const { return dialog_type_; }
  AutofillMetrics::DialogDismissalAction dialog_dismissal_action() const {
    return dialog_dismissal_action_;
  }

  AutofillMetrics::AutocheckoutCompletionStatus autocheckout_status() const {
    return autocheckout_status_;
  }

 private:
  DialogType dialog_type_;
  AutofillMetrics::DialogDismissalAction dialog_dismissal_action_;
  AutofillMetrics::AutocheckoutCompletionStatus autocheckout_status_;

  DISALLOW_COPY_AND_ASSIGN(MockAutofillMetrics);
};

class TestAutofillDialogController : public AutofillDialogControllerImpl {
 public:
  TestAutofillDialogController(content::WebContents* contents,
                               const FormData& form_data,
                               const AutofillMetrics& metric_logger,
                               scoped_refptr<content::MessageLoopRunner> runner,
                               const DialogType dialog_type)
      : AutofillDialogControllerImpl(contents,
                                     form_data,
                                     GURL(),
                                     dialog_type,
                                     base::Bind(&MockCallback)),
        metric_logger_(metric_logger),
        message_loop_runner_(runner) {
    DisableWallet();
  }

  virtual ~TestAutofillDialogController() {}

  virtual void ViewClosed() OVERRIDE {
    message_loop_runner_->Quit();
    AutofillDialogControllerImpl::ViewClosed();
  }

  virtual bool InputIsValid(AutofillFieldType type,
                            const string16& value) const OVERRIDE {
    return true;
  }

  virtual ValidityData InputsAreValid(
      const DetailOutputMap& inputs,
      ValidationType validation_type) const OVERRIDE {
    return ValidityData();
  }

  // Increase visibility for testing.
  AutofillDialogView* view() { return AutofillDialogControllerImpl::view(); }
  const DetailInput* input_showing_popup() const {
    return AutofillDialogControllerImpl::input_showing_popup();
  }

  TestPersonalDataManager* GetTestingManager() {
    return &test_manager_;
  }

 protected:
  virtual PersonalDataManager* GetManager() OVERRIDE {
    return &test_manager_;
  }

 private:
  // To specify our own metric logger.
  virtual const AutofillMetrics& GetMetricLogger() const OVERRIDE {
    return metric_logger_;
  }

  const AutofillMetrics& metric_logger_;
  TestPersonalDataManager test_manager_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillDialogController);
};

}  // namespace

class AutofillDialogControllerTest : public InProcessBrowserTest {
 public:
  AutofillDialogControllerTest() {}
  virtual ~AutofillDialogControllerTest() {}

  void InitializeControllerOfType(DialogType dialog_type) {
    FormData form;
    form.name = ASCIIToUTF16("TestForm");
    form.method = ASCIIToUTF16("POST");
    form.origin = GURL("http://example.com/form.html");
    form.action = GURL("http://example.com/submit.html");
    form.user_submitted = true;

    FormFieldData field;
    field.autocomplete_attribute = "email";
    form.fields.push_back(field);

    message_loop_runner_ = new content::MessageLoopRunner;
    controller_ = new TestAutofillDialogController(
        GetActiveWebContents(),
        form,
        metric_logger_,
        message_loop_runner_,
        dialog_type);
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

 private:
  MockAutofillMetrics metric_logger_;
  TestAutofillDialogController* controller_;  // Weak reference.
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  DISALLOW_COPY_AND_ASSIGN(AutofillDialogControllerTest);
};

// TODO(isherman): Enable these tests on other platforms once the UI is
// implemented on those platforms.
#if defined(TOOLKIT_VIEWS)
// Submit the form data.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, Submit) {
  InitializeControllerOfType(DIALOG_TYPE_REQUEST_AUTOCOMPLETE);
  controller()->view()->GetTestableView()->SubmitForTesting();

  RunMessageLoop();

  EXPECT_EQ(AutofillMetrics::DIALOG_ACCEPTED,
            metric_logger().dialog_dismissal_action());
  EXPECT_EQ(DIALOG_TYPE_REQUEST_AUTOCOMPLETE, metric_logger().dialog_type());
}

// Cancel out of the dialog.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, Cancel) {
  InitializeControllerOfType(DIALOG_TYPE_REQUEST_AUTOCOMPLETE);
  controller()->view()->GetTestableView()->CancelForTesting();

  RunMessageLoop();

  EXPECT_EQ(AutofillMetrics::DIALOG_CANCELED,
            metric_logger().dialog_dismissal_action());
  EXPECT_EQ(DIALOG_TYPE_REQUEST_AUTOCOMPLETE, metric_logger().dialog_type());
}

// Take some other action that dismisses the dialog.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, Hide) {
  InitializeControllerOfType(DIALOG_TYPE_REQUEST_AUTOCOMPLETE);
  controller()->Hide();

  RunMessageLoop();

  EXPECT_EQ(AutofillMetrics::DIALOG_CANCELED,
            metric_logger().dialog_dismissal_action());
  EXPECT_EQ(DIALOG_TYPE_REQUEST_AUTOCOMPLETE, metric_logger().dialog_type());
}

// Test Autocheckout success metrics.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, AutocheckoutSuccess) {
  InitializeControllerOfType(DIALOG_TYPE_AUTOCHECKOUT);
  controller()->view()->GetTestableView()->SubmitForTesting();

  EXPECT_EQ(AutofillMetrics::DIALOG_ACCEPTED,
            metric_logger().dialog_dismissal_action());
  EXPECT_EQ(DIALOG_TYPE_AUTOCHECKOUT, metric_logger().dialog_type());

  controller()->Hide();
  RunMessageLoop();

  EXPECT_EQ(AutofillMetrics::AUTOCHECKOUT_SUCCEEDED,
            metric_logger().autocheckout_status());
}

// Test Autocheckout failure metric.
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, AutocheckoutError) {
  InitializeControllerOfType(DIALOG_TYPE_AUTOCHECKOUT);
  controller()->view()->GetTestableView()->SubmitForTesting();

  EXPECT_EQ(AutofillMetrics::DIALOG_ACCEPTED,
            metric_logger().dialog_dismissal_action());
  EXPECT_EQ(DIALOG_TYPE_AUTOCHECKOUT, metric_logger().dialog_type());

  controller()->OnAutocheckoutError();
  controller()->view()->GetTestableView()->CancelForTesting();

  RunMessageLoop();

  EXPECT_EQ(AutofillMetrics::AUTOCHECKOUT_FAILED,
            metric_logger().autocheckout_status());
}

IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest, FillInputFromAutofill) {
  InitializeControllerOfType(DIALOG_TYPE_REQUEST_AUTOCOMPLETE);

  AutofillProfile full_profile(test::GetFullProfile());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);

  const DetailInputs& inputs =
      controller()->RequestedFieldsForSection(SECTION_SHIPPING);
  const DetailInput& triggering_input = inputs[0];
  string16 value = full_profile.GetRawInfo(triggering_input.type);
  TestableAutofillDialogView* view = controller()->view()->GetTestableView();
  view->SetTextContentsOfInput(triggering_input,
                               value.substr(0, value.size() / 2));
  view->ActivateInput(triggering_input);

  ASSERT_EQ(&triggering_input, controller()->input_showing_popup());
  controller()->DidAcceptSuggestion(string16(), 0);

  // All inputs should be filled.
  AutofillProfileWrapper wrapper(&full_profile, 0);
  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_EQ(wrapper.GetInfo(inputs[i].type),
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
        wrapper.GetInfo(inputs[i].type) :
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
#endif  // defined(TOOLKIT_VIEWS)

}  // namespace autofill
