// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "chrome/browser/ui/cocoa/autofill/autofill_dialog_cocoa.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

void MockCallback(AutofillClient::RequestAutocompleteResult result,
                  const base::string16&,
                  const FormStructure*) {
}

class TestAutofillDialogController : public AutofillDialogControllerImpl {
 public:
  TestAutofillDialogController(
      content::WebContents* contents,
      const FormData& form_structure,
      const AutofillMetrics& metric_logger,
      scoped_refptr<content::MessageLoopRunner> runner)
      : AutofillDialogControllerImpl(contents,
                                     form_structure,
                                     GURL(),
                                     base::Bind(MockCallback)),
        metric_logger_(metric_logger) ,
        runner_(runner) {}

  virtual ~TestAutofillDialogController() {}

  virtual void ViewClosed() OVERRIDE {
    DCHECK(runner_.get());
    runner_->Quit();
    AutofillDialogControllerImpl::ViewClosed();
  }

  AutofillDialogCocoa* GetView() {
    return static_cast<AutofillDialogCocoa*>(
        AutofillDialogControllerImpl::view());
  }

 private:
  // To specify our own metric logger.
  virtual const AutofillMetrics& GetMetricLogger() const OVERRIDE {
    return metric_logger_;
  }

  const AutofillMetrics& metric_logger_;
  scoped_refptr<content::MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillDialogController);
};

class AutofillDialogCocoaBrowserTest : public InProcessBrowserTest {
 public:
  AutofillDialogCocoaBrowserTest() : InProcessBrowserTest() {}

  virtual ~AutofillDialogCocoaBrowserTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    // Ensure Mac OS X does not pop up a modal dialog for the Address Book.
    autofill::test::DisableSystemServices(browser()->profile()->GetPrefs());

    // Stick to local autofill mode.
    browser()->profile()->GetPrefs()->SetBoolean(
        ::prefs::kAutofillDialogPayWithoutWallet, true);

    FormFieldData field;
    field.autocomplete_attribute = "cc-number";
    FormData form_data;
    form_data.fields.push_back(field);
    runner_ = new content::MessageLoopRunner;
    controller_ = new TestAutofillDialogController(
        browser()->tab_strip_model()->GetActiveWebContents(),
        form_data,
        metric_logger_,
        runner_);
  }

  TestAutofillDialogController* controller() { return controller_; }

  void RunMessageLoop() {
    DCHECK(runner_.get());
    runner_->Run();
  }

 private:
  // The controller owns itself.
  TestAutofillDialogController* controller_;

  // The following members must outlive the controller.
  AutofillMetrics metric_logger_;
  scoped_refptr<content::MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDialogCocoaBrowserTest);
};

IN_PROC_BROWSER_TEST_F(AutofillDialogCocoaBrowserTest, DisplayUI) {
  controller()->Show();
  controller()->OnCancel();
  controller()->Hide();

  RunMessageLoop();
}

}  // namespace

}  // namespace autofill
