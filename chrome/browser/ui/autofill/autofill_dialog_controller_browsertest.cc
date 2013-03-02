// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_metrics.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/form_data.h"
#include "chrome/common/form_field_data.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

void MockCallback(const FormStructure*) {}

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

  virtual void LogRequestAutocompleteUiDuration(
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
                               const DialogType dialog_type)
      : AutofillDialogControllerImpl(contents,
                                     form_data,
                                     GURL(),
                                     content::SSLStatus(),
                                     metric_logger,
                                     dialog_type,
                                     base::Bind(&MockCallback)) {
  }

  virtual ~TestAutofillDialogController() {}

  virtual void ViewClosed() OVERRIDE {
    AutofillDialogControllerImpl::ViewClosed();
    MessageLoop::current()->Quit();
  }

  virtual bool InputIsValid(AutofillFieldType type,
                            const string16& value) OVERRIDE {
    return true;
  }

  // Increase visibility for testing.
  AutofillDialogView* view() { return AutofillDialogControllerImpl::view(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAutofillDialogController);
};

}  // namespace

class AutofillDialogControllerTest : public InProcessBrowserTest {
 public:
  AutofillDialogControllerTest() {}
  virtual ~AutofillDialogControllerTest() {}

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillDialogControllerTest);
};

// TODO(isherman): Enable this test on other platforms once the UI is
// implemented on those platforms.
#if defined(TOOLKIT_VIEWS)
#define MAYBE_RequestAutocompleteUiDurationMetrics \
    RequestAutocompleteUiDurationMetrics
#else
#define MAYBE_RequestAutocompleteUiDurationMetrics \
    DISABLED_RequestAutocompleteUiDurationMetrics
#endif  // defined(TOOLKIT_VIEWS)
IN_PROC_BROWSER_TEST_F(AutofillDialogControllerTest,
                       MAYBE_RequestAutocompleteUiDurationMetrics) {
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  FormFieldData field;
  field.autocomplete_attribute = "email";
  form.fields.push_back(field);

  // Submit the form data.
  {
    MockAutofillMetrics metric_logger;
    TestAutofillDialogController* dialog_controller =
        new TestAutofillDialogController(
            GetActiveWebContents(), form, metric_logger,
            DIALOG_TYPE_REQUEST_AUTOCOMPLETE);
    dialog_controller->Show();
    dialog_controller->view()->SubmitForTesting();

    content::RunMessageLoop();

    EXPECT_EQ(AutofillMetrics::DIALOG_ACCEPTED,
              metric_logger.dialog_dismissal_action());
    EXPECT_EQ(DIALOG_TYPE_REQUEST_AUTOCOMPLETE, metric_logger.dialog_type());
  }

  // Cancel out of the dialog.
  {
    MockAutofillMetrics metric_logger;
    TestAutofillDialogController* dialog_controller =
        new TestAutofillDialogController(
            GetActiveWebContents(), form, metric_logger,
            DIALOG_TYPE_AUTOCHECKOUT);
    dialog_controller->Show();
    dialog_controller->view()->CancelForTesting();

    content::RunMessageLoop();

    EXPECT_EQ(AutofillMetrics::DIALOG_CANCELED,
              metric_logger.dialog_dismissal_action());
    EXPECT_EQ(DIALOG_TYPE_AUTOCHECKOUT, metric_logger.dialog_type());
  }

  // Take some other action that dismisses the dialog.
  {
    MockAutofillMetrics metric_logger;
    TestAutofillDialogController* dialog_controller =
        new TestAutofillDialogController(
            GetActiveWebContents(), form, metric_logger,
            DIALOG_TYPE_AUTOCHECKOUT);
    dialog_controller->Show();
    dialog_controller->Hide();

    content::RunMessageLoop();

    EXPECT_EQ(AutofillMetrics::DIALOG_CANCELED,
              metric_logger.dialog_dismissal_action());
    EXPECT_EQ(DIALOG_TYPE_AUTOCHECKOUT, metric_logger.dialog_type());
  }

  // Test Autocheckout success metrics.
  {
    MockAutofillMetrics metric_logger;
    TestAutofillDialogController* dialog_controller =
        new TestAutofillDialogController(
            GetActiveWebContents(), form, metric_logger,
            DIALOG_TYPE_AUTOCHECKOUT);
    dialog_controller->Show();
    dialog_controller->view()->SubmitForTesting();

    EXPECT_EQ(AutofillMetrics::DIALOG_ACCEPTED,
              metric_logger.dialog_dismissal_action());
    EXPECT_EQ(DIALOG_TYPE_AUTOCHECKOUT, metric_logger.dialog_type());

    dialog_controller->Hide();

    content::RunMessageLoop();

    EXPECT_EQ(AutofillMetrics::AUTOCHECKOUT_SUCCEEDED,
              metric_logger.autocheckout_status());
  }

  // Test Autocheckout failure metric.
  {
    MockAutofillMetrics metric_logger;
    TestAutofillDialogController* dialog_controller =
        new TestAutofillDialogController(
            GetActiveWebContents(), form, metric_logger,
            DIALOG_TYPE_AUTOCHECKOUT);
    dialog_controller->Show();
    dialog_controller->view()->SubmitForTesting();

    EXPECT_EQ(AutofillMetrics::DIALOG_ACCEPTED,
              metric_logger.dialog_dismissal_action());
    EXPECT_EQ(DIALOG_TYPE_AUTOCHECKOUT, metric_logger.dialog_type());

    dialog_controller->OnAutocheckoutError();
    dialog_controller->view()->CancelForTesting();

    content::RunMessageLoop();

    EXPECT_EQ(AutofillMetrics::AUTOCHECKOUT_FAILED,
              metric_logger.autocheckout_status());
  }
}

}  // namespace autofill
